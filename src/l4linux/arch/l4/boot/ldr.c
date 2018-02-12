#undef __always_inline

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define __USE_GNU /* For mremap */
#include <sys/mman.h>
#include <bits/l4-malloc.h>

#include <l4/util/elf.h>
#include <l4/util/util.h>
#include <l4/log/log.h>
#include <l4/re/c/mem_alloc.h>
#include <l4/re/c/rm.h>
#include <l4/re/c/util/cap_alloc.h>
#include <l4/re/elf_aux.h>

#include <l4/sys/compiler.h>
#include <l4/sys/kdebug.h>

#include "launch.h"

L4RE_ELF_AUX_ELEM_T(l4re_elf_aux_mword_t, _stack_size,
                    L4RE_ELF_AUX_T_STACK_SIZE, 0x1000);
L4RE_ELF_AUX_ELEM_T(l4re_elf_aux_mword_t, _stack_addr,
                    L4RE_ELF_AUX_T_STACK_ADDR, 0xafff4000);

extern const char image_vmlinux_start[];

struct shared_data {
	unsigned long (*external_resolver)(void);
	L4_CV l4_utcb_t *(*l4lx_utcb)(void);
	void *l4re_global_env;
	void *kip;
};
struct shared_data exchg;

unsigned long __l4_external_resolver(void);

L4_CV l4_utcb_t *l4_utcb_wrap(void)
{
	if (exchg.l4lx_utcb)
		return exchg.l4lx_utcb();
	l4_barrier();
	return l4_utcb_direct();
}

static char morecore_buf[0x2000];
static void *morecore_end;

void *uclibc_morecore(long bytes)
{
	if (bytes <= 0)
		return morecore_end;

	if (morecore_end) {
		LOG_printf("uclibc_morecore(%ld): Called too often.\n", bytes);
		return MAP_FAILED;
	}

	morecore_end = morecore_buf + sizeof(morecore_buf);

	return morecore_end;
}

void *mmap(void *addr, size_t length, int prot, int flags,
           int fd, off_t offset)
{
	(void)addr;
	(void)length;
	(void)prot;
	(void)flags;
	(void)fd;
	(void)offset;
	LOG_printf("mmap called, should only happen for exceptions\n");
	return MAP_FAILED;
}

void *mremap(void *old_addr, size_t old_size, size_t new_size,
             int flags, ...)
{
	(void)old_addr;
	(void)old_size;
	(void)new_size;
	(void)flags;
	LOG_printf("Unimplemented mremap called\n");
	return MAP_FAILED;
}

int munmap(void *addr, size_t length)
{
	(void)addr;
	(void)length;
	LOG_printf("munmap called, should only happen for exceptions\n");
	return -1;
}


void l4x_external_exit(int code);
void l4x_external_exit(int code)
{
	_exit(code);
}

void do_resolve_error(const char *funcname);
void do_resolve_error(const char *funcname)
{
	LOG_printf("Symbol '%s' not found\n", funcname);
	enter_kdebug("Symbol not found!");
}

#ifdef ARCH_amd64
#define FMT "%016llx"
#else
#define FMT "%08x"
#endif

int main(int argc, char **argv)
{
	ElfW(Ehdr) *ehdr = (void *)image_vmlinux_start;
	int i;

	if (!l4util_elf_check_magic(ehdr)) {
		printf("lxldr: Invalid vmlinux binary (No ELF)\n");
		return 1;
	}

	if (!l4util_elf_check_arch(ehdr)) {
		printf("lxldr: Invalid vmlinux binary architecture\n");
		return 1;
	}

	for (i = 0; i < ehdr->e_phnum; ++i) {
		int r;
		l4_addr_t map_addr;
		unsigned long map_size;
		unsigned flags;
		l4re_ds_t ds, new_ds;
		l4_addr_t offset;

		ElfW(Phdr) *ph = (ElfW(Phdr)*)((l4_addr_t)l4util_elf_phdr(ehdr)
		                               + i * ehdr->e_phentsize);
		printf("PH %2d offs=" FMT " flags=%c%c%c PH-type=0x%x\n"
		       "      virt=" FMT " evirt=" FMT "\n"
		       "      phys=" FMT " ephys=" FMT "\n"
		       "      f_sz=" FMT " memsz=" FMT "\n",
		       i, ph->p_offset,
		       ph->p_flags & PF_R ? 'r' : '-',
		       ph->p_flags & PF_W ? 'w' : '-',
		       ph->p_flags & PF_X ? 'x' : '-',
		       ph->p_type,
		       ph->p_vaddr, ph->p_vaddr + ph->p_memsz,
		       ph->p_paddr, ph->p_paddr + ph->p_memsz,
		       ph->p_filesz, ph->p_memsz);
		if (ph->p_type != PT_LOAD)
			continue;

		if (ph->p_vaddr & ~L4_PAGEMASK
		    || ph->p_paddr & ~L4_PAGEMASK) {
			printf("lxldr: unaligned section\n");
			continue;
		}

		if (ph->p_vaddr != ph->p_paddr) {
			printf("lxldr: vaddr vs paddr mismatch ["FMT"/"FMT"]\n",
			       ph->p_vaddr, ph->p_paddr);
			if (0) // x86-64 kernel has vaddr=0 for its init-section
				continue;
		}

		if (ph->p_filesz < ph->p_memsz) {

			ds = l4re_util_cap_alloc();
			if (l4_is_invalid_cap(ds)) {
				printf("lxldr: out of caps\n");
				return 1;
			}

			r = l4re_ma_alloc(ph->p_memsz, ds,
			                  L4RE_MA_CONTINUOUS | L4RE_MA_PINNED);
			if (r) {
				printf("lxldr: could not allocate memory: %d\n",
				       r);
				return 1;
			}

			map_addr = ph->p_paddr;
			r = l4re_rm_attach((void **)&map_addr,
			                    ph->p_memsz, L4RE_RM_EAGER_MAP,
			                    ds | L4_CAP_FPAGE_RW, 0, 0);
			if (r) {
				printf("lxldr: failed attaching memory (%d):"
				       " "FMT" - "FMT"\n",
				       r, ph->p_paddr,
				       ph->p_paddr + ph->p_memsz - 1);
				return 1;
			}
			memcpy((void *)ph->p_paddr,
			       (char *)image_vmlinux_start + ph->p_offset,
			       ph->p_filesz);
			memset((void *)ph->p_paddr + ph->p_filesz, 0,
			       ph->p_memsz - ph->p_filesz);
			continue;
		}

		new_ds = l4re_util_cap_alloc();
		if (l4_is_invalid_cap(new_ds)) {
			printf("lxldr: out of caps\n");
			return 1;
		}

		r = l4re_ma_alloc(ph->p_memsz, new_ds,
		                  L4RE_MA_CONTINUOUS | L4RE_MA_PINNED);
		if (r) {
			printf("lxldr: could not allocate memory: %d\n", r);
			return 1;
		}

		/* We just need the dataspace and offset */
		map_addr = (l4_addr_t)image_vmlinux_start + ph->p_offset;
		map_size = ph->p_memsz;
		r = l4re_rm_find(&map_addr, &map_size, &offset, &flags, &ds);
		offset += ((l4_addr_t)image_vmlinux_start + ph->p_offset) - map_addr;
		if (r) {
			printf("lxldr: Failed lookup %p (%p + %lx): %d\n",
			       (char *)image_vmlinux_start + ph->p_offset,
			       (char *)image_vmlinux_start,
			       (unsigned long)ph->p_offset, r);
			return 1;
		}

		r = l4re_ds_copy_in(new_ds, 0, ds, offset, ph->p_memsz);
		if (r)
			printf("l4dm_mem_copyin failed: %d\n", r);

		map_addr = ph->p_paddr;
		r = l4re_rm_attach((void **)&map_addr,
		                   ph->p_memsz, L4RE_RM_EAGER_MAP,
		                   new_ds | L4_CAP_FPAGE_RW, 0, 0);
		if (r) {
			printf("lxldr: failed to attach section\n");
			return 1;
		}
	}

	printf("Starting binary at %p, argc=%d argv=%p *argv=%p argv0=%s\n",
	       (void *)ehdr->e_entry, argc, argv, *argv, *argv);
	exchg.external_resolver = __l4_external_resolver;
	exchg.l4re_global_env  = l4re_global_env;
	exchg.kip              = l4re_kip();
	printf("External resolver is at %p\n", __l4_external_resolver);

	return launch_kernel(argc, argv, &exchg, ehdr->e_entry);
}
