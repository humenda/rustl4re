/*
 * Main & misc. file.
 */
#include <linux/ctype.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/pm.h>

#ifdef CONFIG_BLK_DEV_INITRD
#include <linux/initrd.h>
#endif

#include <linux/kprobes.h>

#include <asm/page.h>
#include <asm/setup.h>
#include <asm/uaccess.h>
#include <asm/segment.h>
#include <asm/unistd.h>
#include <asm/generic/l4lib.h>
#include <asm-generic/sections.h>

#include <l4/sys/err.h>
#include <l4/sys/kdebug.h>
#include <l4/sys/thread.h>
#include <l4/sys/factory.h>
#include <l4/sys/scheduler.h>
#include <l4/sys/task.h>
#include <l4/sys/debugger.h>
#include <l4/sys/irq.h>
#include <l4/re/c/mem_alloc.h>
#include <l4/re/c/dma_space.h>
#include <l4/re/c/rm.h>
#include <l4/re/c/debug.h>
#include <l4/re/c/util/cap_alloc.h>
#include <l4/re/c/namespace.h>
#include <l4/re/env.h>
#include <l4/re/protocols.h>
#include <l4/log/log.h>
#include <l4/util/cpu.h>
#include <l4/util/l4_macros.h>
#include <l4/util/kip.h>
#include <l4/util/atomic.h>
#include <l4/io/io.h>
#include <l4/vbus/vbus.h>
#ifdef CONFIG_L4_USE_L4SHMC
#include <l4/shmc/shmc.h>
#endif

#include <asm/api/config.h>
#include <asm/api/macros.h>

#include <asm/l4lxapi/thread.h>
#include <asm/l4lxapi/misc.h>
#include <asm/l4lxapi/task.h>
#include <asm/l4lxapi/irq.h>

#include <asm/generic/dispatch.h>
#include <asm/generic/ioremap.h>
#include <asm/generic/kthreads.h>
#include <asm/generic/memory.h>
#include <asm/generic/setup.h>
#include <asm/generic/smp.h>
#include <asm/generic/stack_id.h>
#include <asm/generic/stats.h>
#include <asm/generic/tamed.h>
#include <asm/generic/task.h>
#include <asm/generic/cap_alloc.h>
#include <asm/generic/log.h>
#include <asm/generic/util.h>
#include <asm/generic/vcpu.h>
#include <asm/generic/jdb.h>
#include <asm/server/server.h>

#include <asm/l4x/exception.h>
#include <asm/l4x/lx_syscalls.h>
#include <asm/l4x/utcb.h>
#ifdef CONFIG_ARM
#include <asm/cputype.h>
#include <asm/generic/upage.h>
#include <asm/l4x/dma.h>
#include <asm/l4x/upage.h>
#endif

#include <l4/sys/platform_control.h>

#ifdef CONFIG_X86
#include <l4/sys/segment.h>

#include <asm/desc.h>
#include <asm/io.h>
#include <asm/i8259.h>
#include <asm/fixmap.h>
#ifdef CONFIG_L4_EXTERNAL_RTC
#include <l4/rtc/rtc.h>
#endif
#include <asm/l4x/smp.h>
#ifdef CONFIG_X86_LOCAL_APIC
#include <asm/smp.h>
#endif
#endif /* X86 */

L4_CV void l4x_external_exit(int);

unsigned long __l4_external_resolver;
EXPORT_SYMBOL(__l4_external_resolver);

L4_EXTERNAL_FUNC_VARGS(LOG_printf);
EXPORT_SYMBOL(LOG_printf);
L4_EXTERNAL_FUNC_AND_EXPORT(LOG_vprintf);
L4_EXTERNAL_FUNC_AND_EXPORT(LOG_flush);
L4_EXTERNAL_FUNC(l4_sleep);

L4_EXTERNAL_FUNC_AND_EXPORT(l4util_kip_kernel_has_feature);
L4_EXTERNAL_FUNC_AND_EXPORT(l4util_kip_kernel_abi_version);
L4_EXTERNAL_FUNC_VARGS(l4_kprintf);
L4_EXTERNAL_FUNC(l4x_external_exit);

L4_EXTERNAL_FUNC_AND_EXPORT(l4sys_errtostr);
L4_EXTERNAL_FUNC_AND_EXPORT(l4re_util_cap_alloc);
L4_EXTERNAL_FUNC_AND_EXPORT(l4re_util_cap_free);
L4_EXTERNAL_FUNC_AND_EXPORT(l4re_debug_obj_debug);

L4_EXTERNAL_FUNC_AND_EXPORT(l4re_dma_space_map);
L4_EXTERNAL_FUNC_AND_EXPORT(l4re_dma_space_unmap);
L4_EXTERNAL_FUNC_AND_EXPORT(l4re_ds_size);
L4_EXTERNAL_FUNC_AND_EXPORT(l4re_ds_phys);
L4_EXTERNAL_FUNC_AND_EXPORT(l4re_ds_info);
L4_EXTERNAL_FUNC_AND_EXPORT(l4re_ds_copy_in);
L4_EXTERNAL_FUNC_AND_EXPORT(l4re_ds_map_region);

L4_EXTERNAL_FUNC_AND_EXPORT(l4re_ns_query_to_srv);

L4_EXTERNAL_FUNC_AND_EXPORT(l4re_rm_find_srv);
L4_EXTERNAL_FUNC_AND_EXPORT(l4re_rm_show_lists_srv);
L4_EXTERNAL_FUNC_AND_EXPORT(l4re_rm_attach_srv);
L4_EXTERNAL_FUNC_AND_EXPORT(l4re_rm_detach_srv);
L4_EXTERNAL_FUNC_AND_EXPORT(l4re_rm_reserve_area_srv);
L4_EXTERNAL_FUNC_AND_EXPORT(l4re_rm_free_area_srv);

L4_EXTERNAL_FUNC_AND_EXPORT(l4re_ma_alloc_align_srv);

L4_EXTERNAL_FUNC(l4io_request_iomem);
L4_EXTERNAL_FUNC(l4io_request_iomem_region);
L4_EXTERNAL_FUNC(l4io_release_iomem);
L4_EXTERNAL_FUNC(l4io_request_ioport);
L4_EXTERNAL_FUNC(l4io_request_icu);
L4_EXTERNAL_FUNC(l4io_search_iomem_region);
L4_EXTERNAL_FUNC(l4io_lookup_device);
L4_EXTERNAL_FUNC(l4io_lookup_resource);
L4_EXTERNAL_FUNC(l4io_iterate_devices);
L4_EXTERNAL_FUNC(l4io_has_resource);
L4_EXTERNAL_FUNC(l4vbus_assign_dma_domain);

#ifdef CONFIG_L4_USE_L4SHMC
L4_EXTERNAL_FUNC_AND_EXPORT(l4shmc_create);
L4_EXTERNAL_FUNC_AND_EXPORT(l4shmc_attach_to);
L4_EXTERNAL_FUNC_AND_EXPORT(l4shmc_get_chunk_to);
L4_EXTERNAL_FUNC_AND_EXPORT(l4shmc_attach_signal_to);
L4_EXTERNAL_FUNC_AND_EXPORT(l4shmc_connect_chunk_signal);
L4_EXTERNAL_FUNC_AND_EXPORT(l4shmc_wait_chunk_to);
L4_EXTERNAL_FUNC_AND_EXPORT(l4shmc_add_chunk);
L4_EXTERNAL_FUNC_AND_EXPORT(l4shmc_add_signal);
L4_EXTERNAL_FUNC_AND_EXPORT(l4shmc_get_signal_to);
L4_EXTERNAL_FUNC_AND_EXPORT(l4shmc_iterate_chunk);
L4_EXTERNAL_FUNC_AND_EXPORT(l4shmc_area_overhead);
L4_EXTERNAL_FUNC_AND_EXPORT(l4shmc_chunk_overhead);
#endif

#ifdef CONFIG_L4_SERVER
L4_EXTERNAL_FUNC(l4x_srv_init);
L4_EXTERNAL_FUNC(l4x_srv_setup_recv);
L4_EXTERNAL_FUNC(l4x_srv_register_c);
#endif


#ifdef CONFIG_X86
u32 *trampoline_cr4_features;

unsigned l4x_x86_fiasco_gdt_entry_offset;
unsigned l4x_x86_fiasco_user_cs;
unsigned l4x_x86_fiasco_user_ds;
#ifdef CONFIG_X86_64
unsigned l4x_x86_fiasco_user32_cs;
#ifndef CONFIG_L4_VCPU
struct l4x_segment_user_state_t l4x_current_user_segments[NR_CPUS];
#endif
#endif

unsigned long l4x_fixmap_space_start;

l4_utcb_t *l4x_utcb_pointer[L4X_UTCB_POINTERS];
#endif /* x86 */

unsigned int  l4x_nr_cpus = 1;
struct l4x_cpu_physmap_struct
{
	unsigned int phys_id;
};
static struct l4x_cpu_physmap_struct l4x_cpu_physmap[NR_CPUS];

unsigned l4x_fiasco_nr_of_syscalls = 3;

#ifdef CONFIG_ARM
extern void start_kernel(void);
#endif

l4_cap_idx_t l4x_start_thread_id __nosavedata = L4_INVALID_CAP;
l4_cap_idx_t l4x_start_thread_pager_id __nosavedata = L4_INVALID_CAP;

enum { NUM_DS_MAINMEM = 10 };
static l4re_dma_space_t l4x_dma_space;
static l4re_ds_t l4x_ds_mainmem[NUM_DS_MAINMEM] __nosavedata;
static bool l4x_phys_mem_disabled;
void *l4x_main_memory_start;
unsigned long l4x_vmalloc_memory_start;
l4_kernel_info_t *l4lx_kinfo;
l4_cap_idx_t l4x_user_gate[NR_CPUS];

l4re_env_t *l4re_global_env;

DEFINE_SPINLOCK(l4x_cap_lock);
EXPORT_SYMBOL(l4x_cap_lock);

int l4x_debug_show_exceptions;
int l4x_debug_show_ghost_regions;
#ifdef CONFIG_L4_DEBUG_STATS
struct l4x_debug_stats l4x_debug_stats_data;
#endif

struct l4x_phys_virt_mem {
	l4_addr_t phys;      /* physical address */
	void    * virt;      /* virtual address */
	l4_size_t size;      /* size of chunk in Bytes */
	l4_cap_idx_t ds;     /* data-sapce capability for this region,
	                      * if DMA is allowed on this region (including
	                      * virtio DMA) */
	l4_addr_t ds_offset; /* offset within the data space that is attached
	                      * at 'virt' */
};

enum { L4X_PHYS_VIRT_ADDRS_MAX_ITEMS = 50 };

/* Default memory size */
static unsigned long l4x_mainmem_size;
static bool l4x_init_finished;

static struct l4x_phys_virt_mem l4x_phys_virt_addrs[L4X_PHYS_VIRT_ADDRS_MAX_ITEMS] __nosavedata;
DEFINE_SPINLOCK(l4x_v2p_lock);

l4_cap_idx_t l4x_jdb_cap;

#ifdef CONFIG_L4_CONFIG_CHECKS
static const unsigned long required_kernel_abi_version = 3;
static const char *required_kernel_features[] =
  {
  };

static void l4x_configuration_sanity_check(const char *cmdline)
{
	int i;

	for (i = 0; i < sizeof(required_kernel_features)
		          / sizeof(required_kernel_features[0]); i++) {
		if (!l4util_kip_kernel_has_feature(l4lx_kinfo, required_kernel_features[i])) {
			LOG_printf("The running microkernel does not have the\n"
			           "      %s\n"
			           "feature enabled!\n",
			           required_kernel_features[i]);
			while (1)
				enter_kdebug("Microkernel feature missing!");
		}
	}

	if (l4util_kip_kernel_abi_version(l4lx_kinfo) < required_kernel_abi_version) {
		LOG_printf("The ABI version of the microkernel is too low: it has %ld, "
		           "I need %ld\n",
		           l4util_kip_kernel_abi_version(l4lx_kinfo),
		           required_kernel_abi_version);
		while (1)
			enter_kdebug("Stop!");
	}

#ifndef CONFIG_ARM
	{
		char *p;
		if ((p = strstr(cmdline, "mem="))
		    && (memparse(p + 4, &p) < (16 << 20))) {
			LOG_printf("Minimal 16MB recommended.\n");
			enter_kdebug("Increase value in mem= option.");
		}
	}
#endif

#ifndef CONFIG_L4_SERIAL
	if (strstr(cmdline, "console=ttyLv")) {
		LOG_printf("Console output set to ttyLvx but driver not compiled in.\n");
		enter_kdebug("L4 serial driver not enabled");
	}
#endif
}
#else
static void l4x_configuration_sanity_check(const char *cmdline)
{
	LOG_printf("Configuration checks disabled, you know what you're doing.\n");
}
#endif

static int l4x_check_setup(const char *cmdline)
{
#ifdef CONFIG_ARM
	if (!l4util_kip_kernel_has_feature(l4lx_kinfo, "armv6plus")) {
#if defined(CONFIG_CPU_V6) || defined(CONFIG_CPU_V6K) || defined(CONFIG_CPU_V7)
		LOG_printf("Running Fiasco is not compiled for v6 "
		           "or better architecture.\nRequired when "
		           "L4Linux is compiled for v6 (e.g. for TLS).\n");
		/* Note, running on a armv5 fiasco is possible when using
		 * TLS trap-emulation (see dispatch.c). But just do not do
		 * this.
		 */
		return 1;
#endif
	}
#endif
	l4x_configuration_sanity_check(cmdline);
	return 0;
}

static void l4x_server_loop(void);

static void l4x_v2p_init(void)
{
}

#define v2p_iterator_begin (l4x_phys_virt_addrs)
#define v2p_iterator_end   (l4x_phys_virt_addrs + L4X_PHYS_VIRT_ADDRS_MAX_ITEMS)

#define v2p_for_each(i) \
	for (i = v2p_iterator_begin; i < v2p_iterator_end; ++i)

void l4x_v2p_for_each(void (*cb)(l4_addr_t phys, void *virt,
                                 l4_size_t size, l4_cap_idx_t ds,
                                 l4_addr_t ds_offset, void *data), void *data)
{

	struct l4x_phys_virt_mem *i;

	v2p_for_each(i) {
		if (!i->size)
			continue;

		cb(i->phys, i->virt, i->size, i->ds, i->ds_offset, data);
	}
}
EXPORT_SYMBOL(l4x_v2p_for_each);

static bool l4x_v2p_is_registered(void *virt, l4_size_t size)
{
	struct l4x_phys_virt_mem *i;

	v2p_for_each(i)
		if (i->size && i->virt <= virt && virt < i->virt + i->size)
			return true;

	return false;
}

void l4x_v2p_add_item_ds(l4_addr_t phys, void *virt, l4_size_t size,
                         l4_cap_idx_t ds, l4_addr_t ds_offset)
{
	struct l4x_phys_virt_mem *i;

	/* may be we should check for mismatching ds and ds_offset */
	if (l4x_v2p_is_registered(virt, size))
		return;

	if (likely(l4x_init_finished))
		spin_lock(&l4x_v2p_lock);

	v2p_for_each(i) {
		if (i->size)
			continue;

		*i = (struct l4x_phys_virt_mem){
				.phys = phys,
				.virt = virt,
				.ds = ds,
				.ds_offset = ds_offset,
			};
		smp_wmb();
		i->size = size;
		break;
	}
	if (likely(l4x_init_finished))
		spin_unlock(&l4x_v2p_lock);

	if (i == v2p_iterator_end)
		LOG_printf("v2p filled up!\n");
}

unsigned long l4x_v2p_del_item(void *virt)
{
	struct l4x_phys_virt_mem *i;
	unsigned long r = 0;

	if (likely(l4x_init_finished))
		spin_lock(&l4x_v2p_lock);

	v2p_for_each(i) {
		if (!i->size || i->virt != virt)
			continue;

		r = i->size;
		i->size = 0;
		smp_wmb();
		i->phys = 0;
		i->virt = NULL;
		break;
	}
	if (likely(l4x_init_finished))
		spin_unlock(&l4x_v2p_lock);

	return r;
}

static void l4x_virt_to_phys_show(void)
{
	struct l4x_phys_virt_mem *i;

	v2p_for_each(i)
		if (i->virt || i->phys || i->size)
			l4x_printf("v2p: %zd: v:%08lx-%08lx p:%08lx-%08lx sz:%08zx\n",
			           i - v2p_iterator_begin,
			           (unsigned long)i->virt,
			           (unsigned long)i->virt + i->size,
			           i->phys, i->phys + i->size, i->size);

	l4x_ioremap_show();
}

unsigned long l4x_virt_to_phys(volatile void * address)
{
	struct l4x_phys_virt_mem *i;
	unsigned long p;

	v2p_for_each(i) {
		if (i->size
		    && i->virt <= address && address < i->virt + i->size)
			return (address - i->virt) + i->phys;
	}

	p = find_ioremap_entry_phys((unsigned long)address);
	if (p)
		return p;

	/* Whitelist: */

	/* Debugging check: don't miss a translation, can give nasty
	 *                  DMA problems */
	l4x_printf("%s: Could not translate virt. address %p\n",
	           __func__, address);
	l4x_virt_to_phys_show();
	WARN_ON(1);

	return __pa(address);
}
EXPORT_SYMBOL(l4x_virt_to_phys);


void *l4x_phys_to_virt(unsigned long address)
{
	struct l4x_phys_virt_mem *i;
	unsigned long v;

	v2p_for_each(i) {
		if (i->size
		    && i->phys <= address && address < i->phys + i->size)
			return (address - i->phys) + i->virt;
	}

	v = find_ioremap_entry(address);
	if (v)
		return __va(v);

#ifdef CONFIG_ARM
	/* Avoid warning for typical DMA masks (__dma_supported) */
	if (address == ~0UL)
		return __va(address);
#endif

	/* Whitelist: */
#ifdef CONFIG_X86
	if (address < 0x1000 ||
	    address == 0xb8000 ||
	    address == 0xa0000 ||
	    address == 0xc0000)
		return __va(address);
#endif

	/* Debugging check: don't miss a translation, can give nasty
	 *                  DMA problems */
	l4x_printf("%s: Could not translate phys. address 0x%lx\n",
	           __func__, address);
	l4x_virt_to_phys_show();
	WARN_ON(1);

	return __va(address);
}
EXPORT_SYMBOL(l4x_phys_to_virt);

phys_addr_t l4x_dma_phys_low_limit;

static void l4x_init_dma_phys_low_limit(void)
{
	struct l4x_phys_virt_mem *e;
	phys_addr_t v = 0;
	phys_addr_t limit = 0xfffffffful;

	v2p_for_each(e) {
		phys_addr_t p = e->phys;

		if  (!e->size)
			continue;

		if (p > limit)
			continue;

		if (p + e->size - 1 < limit)
			v = max(v, (phys_addr_t)e->virt + e->size - 1);
		else
			v = max(v, (phys_addr_t)e->virt + limit - p);
	}

	l4x_dma_phys_low_limit = v;
}



#if defined(CONFIG_X86) && defined(CONFIG_VGA_CONSOLE)
// we may want to make this function available for a broader usage
static int l4x_pagein(unsigned long addr, unsigned long size,
                      int rw)
{
	int err;
	l4_addr_t a;
	unsigned long sz;
	l4_addr_t off;
	unsigned fl;
	l4re_ds_t ds;
	unsigned long map_flags = rw ? L4RE_DS_MAP_FLAG_RW
		                     : L4RE_DS_MAP_FLAG_RO;

	if (size == 0)
		return 0;

	size += addr & ~L4_PAGEMASK;
	size  = l4_round_page(size);
	addr &= L4_PAGEMASK;

	do {
		a = addr;
		sz = 1;

		err = l4re_rm_find(&a, &sz, &off, &fl, &ds);
		if (err < 0)
			break;

		if (sz > size)
			sz = size;

		err = l4re_ds_map_region(ds, off + (addr - a), map_flags,
		                         addr, addr + sz);
		if (err < 0)
			break;

		size -= sz;
		addr += sz;
	} while (size);

	return err;
}
#endif

#ifdef CONFIG_X86_32
static unsigned l4x_x86_orig_utcb_segment;
static inline void l4x_x86_utcb_save_orig_segment(void)
{
	asm volatile ("mov %%fs, %0": "=r" (l4x_x86_orig_utcb_segment));
}
unsigned l4x_x86_utcb_get_orig_segment(void)
{
	return l4x_x86_orig_utcb_segment;
}
#else
static inline void l4x_x86_utcb_save_orig_segment(void) {}
#endif

l4_vcpu_state_t *l4x_vcpu_ptr[NR_CPUS];

L4_CV l4_utcb_t *l4_utcb_wrap(void)
{
#if defined(CONFIG_ARM) || defined(CONFIG_X86_64)
	return l4_utcb_direct();
#elif defined(CONFIG_X86_32)
	unsigned long v;
	asm ("mov %%fs, %0": "=r" (v));
	if (v == 0x43 || v == 7) {
		asm volatile ("mov %%fs:0, %0" : "=r" (v));
		return (l4_utcb_t *)v;
	}
	return l4x_utcb_current();
#else
#error Add your arch
#endif
}

/*
 * Full fledged name resolving, including cap allocation.
 */
static int
l4x_re_resolve_name_noctx(const char *name, unsigned len, l4_cap_idx_t *cap)
{
	int r, l;
	l4re_env_cap_entry_t const *entry;

	for (l = 0; l < len && name[l] != '/'; ++l)
		;

	entry = l4re_env_get_cap_l(name, l, l4re_env());
	if (!entry)
		return -ENOENT;

	if (len == l) {
		// full name resolved, no slashes, return
		*cap = entry->cap;
		return 0;
	}

	if (l4_is_invalid_cap(*cap = l4x_cap_alloc()))
		return -ENOMEM;

	r = l4re_ns_query_srv(entry->cap, &name[l + 1], *cap);
	if (r) {
		l4x_printf("Failed to query name '%s': %s(%d)\n",
		           name, l4sys_errtostr(r), r);
		goto free_cap;
	}

	return 0;

free_cap:
	l4x_cap_free(*cap);
	return -ENOENT;
}

int l4x_re_resolve_name(const char *name, l4_cap_idx_t *cap)
{
	return L4XV_FN_i(l4x_re_resolve_name_noctx(name, strlen(name), cap));
}

int l4x_query_and_get_ds(const char *name, const char *logprefix,
                         l4_cap_idx_t *dscap, void **addr,
                         l4re_ds_stats_t *dsstat)
{
	int ret;
	L4XV_V(f);

	ret = l4x_re_resolve_name(name, dscap);
	if (ret)
		return ret;

	L4XV_L(f);
	if ((ret = l4re_ds_info(*dscap, dsstat)))
		goto out;

	*addr = NULL;
	if (l4re_rm_attach(addr, dsstat->size,
	                   L4RE_RM_SEARCH_ADDR
	                    | L4RE_RM_EAGER_MAP
			    | L4RE_RM_READ_ONLY,
	                   *dscap,
	                   0, L4_PAGESHIFT))
		goto out;

	L4XV_U(f);
	return 0;
out:
	l4x_cap_free(*dscap);
	L4XV_U(f);
	return ret;
}
EXPORT_SYMBOL(l4x_query_and_get_ds);

int l4x_query_and_get_cow_ds(const char *name, const char *logprefix,
                             l4_cap_idx_t *memcap,
                             l4_cap_idx_t *dscap, void **addr,
                             l4re_ds_stats_t *stat,
                             int pass_through_if_rw_src_ds)
{
	int ret;
	L4XV_V(f);

	*memcap = L4_INVALID_CAP;
	*addr = NULL;

	ret = l4x_re_resolve_name(name, dscap);
	if (ret)
		return ret;

	L4XV_L(f);
	if ((ret = l4re_ds_info(*dscap, stat)))
		goto out1;

	if ((stat->flags & L4RE_DS_MAP_FLAG_RW)
	    && pass_through_if_rw_src_ds) {
		// pass-through mode

		if ((ret = l4re_rm_attach(addr, stat->size,
		                          L4RE_RM_SEARCH_ADDR
		                            | L4RE_RM_EAGER_MAP,
		                          *dscap | L4_CAP_FPAGE_RW,
		                          0, L4_PAGESHIFT)))
			goto out1;

	} else {
		// cow
		if (l4_is_invalid_cap(*memcap = l4x_cap_alloc()))
			goto out1;

		if ((ret = l4re_ma_alloc(stat->size, *memcap, 0)))
			goto out2;

		if ((ret = l4re_ds_copy_in(*memcap, 0, *dscap, 0, stat->size)))
			goto out3;

		if ((ret = l4re_rm_attach(addr, stat->size,
		                          L4RE_RM_SEARCH_ADDR
		                            | L4RE_RM_EAGER_MAP,
		                          *memcap | L4_CAP_FPAGE_RW,
		                          0, L4_PAGESHIFT)))
			goto out3;

		ret = -ENOMEM;
		if (l4re_ds_info(*memcap, stat))
			goto out4;
	}

	L4XV_U(f);
	return 0;

out4:
	if (*addr)
		l4re_rm_detach(*addr);
out3:
	l4_task_delete_obj(L4RE_THIS_TASK_CAP, *memcap);
out2:
	l4x_cap_free(*memcap);
out1:
	l4_task_delete_obj(L4RE_THIS_TASK_CAP, *dscap);
	L4XV_U(f);
	l4x_cap_free(*dscap);
	return ret;
}
EXPORT_SYMBOL(l4x_query_and_get_cow_ds);

int l4x_detach_and_free_ds(l4_cap_idx_t dscap, void *addr)
{
	int r;

	if ((r = L4XV_FN_i(l4re_rm_detach(addr)))) {
		LOG_printf("Failed to detach at %p (%d)\n", addr, r);
		return r;
	}

	if ((r = L4XV_FN_e(l4_task_delete_obj(L4RE_THIS_TASK_CAP, dscap))))
		LOG_printf("Failed to release/unmap cap: %d\n", r);

	l4x_cap_free(dscap);
	return 0;
}
EXPORT_SYMBOL(l4x_detach_and_free_ds);

int l4x_detach_and_free_cow_ds(l4_cap_idx_t memcap,
                               l4_cap_idx_t dscap, void *addr)
{
	int r;

	if ((r = L4XV_FN_i(l4re_rm_detach(addr)))) {
		LOG_printf("Failed to detach at %p (%d)\n", addr, r);
		return r;
	}

	if ((r = L4XV_FN_e(l4_task_delete_obj(L4RE_THIS_TASK_CAP, dscap))))
		LOG_printf("Failed to release/unmap cap: %d\n", r);

	if ((r = L4XV_FN_e(l4_task_delete_obj(L4RE_THIS_TASK_CAP, memcap))))
		LOG_printf("Failed to release/unmap cap: %d\n", r);

	l4x_cap_free(dscap);
	l4x_cap_free(memcap);
	return 0;
}
EXPORT_SYMBOL(l4x_detach_and_free_cow_ds);

static int l4x_check_kern_region_noctx(void *address, unsigned long size,
                                       int rw)
{
	l4re_ds_t ds;
	l4_addr_t off;
	unsigned flags;

	return (   l4re_rm_find((unsigned long *)&address,
	                         &size, &off, &flags, &ds)
	        || (flags & L4RE_RM_IN_AREA))
	       || (rw && (flags & L4RE_RM_READ_ONLY));
}

int l4x_check_kern_region(void *address, unsigned long size, int rw)
{
	return L4XV_FN_i(l4x_check_kern_region_noctx(address, size, rw));
}

/* ---------------------------------------------------------------- */

typedef void (*at_exit_function_type)(void);

struct cxa_atexit_item {
	void (*f)(void *);
	void *arg;
	void *dso_handle;
};

static struct cxa_atexit_item at_exit_functions[10];
static const int at_exit_nr_of_functions
	= sizeof(at_exit_functions) / sizeof(at_exit_functions[0]);
static int __current_exititem;

static struct cxa_atexit_item *__next_atexit(void)
{
	if (__current_exititem >= at_exit_nr_of_functions) {
		LOG_printf("WARNING: atexit array overflow, increase!\n");
		return 0;
	}
	return &at_exit_functions[__current_exititem++];
}

int __cxa_atexit(void (*f)(void *), void *arg, void *dso_handle)
{
	struct cxa_atexit_item *h = __next_atexit();

	if (!h)
		return -1;

	h->f = f;
	h->arg = arg;
	h->dso_handle = dso_handle;

	return 0;
}

void __cxa_finalize(void *dso_handle)
{
	const int verbose = 0;
	register int i = __current_exititem;
	while (i) {
		struct cxa_atexit_item *h = &at_exit_functions[--i];
		if (h->f && (dso_handle == 0 || h->dso_handle == dso_handle)) {
			if (verbose)
				LOG_printf("Calling func %p\n", h->f);
			h->f(h->arg);
			if (verbose)
				LOG_printf("done calling %p.\n", h->f);
			h->f = 0;
		}
	}
}


int atexit(void (*function)(void))
{
	return __cxa_atexit((void (*)(void*))function, 0, 0);
}

void
l4x_linux_main_exit(void)
{
	extern void exit(int);
	LOG_printf("Terminating L4Linux.\n");
	exit(0);
}

/* ---------------------------------------------------------------- */

static inline int l4x_is_writable_area(unsigned long a)
{
	return    ((unsigned long)&_sdata <= a
	           && a < (unsigned long)&_edata)
	       || ((unsigned long)&__init_begin <= a
	           && a < (unsigned long)&__init_end)
	       || ((unsigned long)&__bss_start <= a
	           && a < (unsigned long)&__bss_stop);
}
static int l4x_forward_pf(l4_umword_t addr, l4_umword_t pc, int extra_write)
{
	l4_msgtag_t tag;
	l4_umword_t err;
	l4_utcb_t *u = l4_utcb();

	do {
		l4_msg_regs_t *mr = l4_utcb_mr_u(u);
		/* In case the PF comes from vCPU mode, we need to clear the
		 * lower 4 bit, if it's a normal PF we actually do not need
		 * to do it */
		mr->mr[0] = (addr & ~0xful) | (extra_write ? 2 : 0);
		mr->mr[1] = pc;
		mr->mr[2] = L4_ITEM_MAP | addr;
		mr->mr[3] = l4_fpage(addr, L4_LOG2_PAGESIZE, L4_FPAGE_RWX).raw;
		tag = l4_msgtag(L4_PROTO_PAGE_FAULT, 2, 1, 0);
		tag = l4_ipc_call(l4x_start_thread_pager_id, u, tag,
		                  L4_IPC_NEVER);
		err = l4_ipc_error(tag, u);
	} while (err == L4_IPC_SECANCELED || err == L4_IPC_SEABORTED);

	if (unlikely(l4_msgtag_has_error(tag)))
		LOG_printf("Error forwarding page fault: %lx\n",
	                   l4_utcb_tcr_u(u)->error);

	if (unlikely(l4_msgtag_words(tag) > 0
	             && l4_utcb_mr_u(u)->mr[0] == ~0))
		// unresolvable page fault, we're supposed to trigger an
		// exception
		return 0;

	return 1;
}

#ifdef CONFIG_X86
/*
 * We need to get the lowest virtual address and we can find out in the
 * KIP's memory descriptors. Fiasco-UX on system like Ubuntu set the minimum
 * mappable address to 0x10000 so we cannot just plain begin at 0x1000.
 */
static unsigned long get_min_virt_address(void)
{
	l4_kernel_info_t *kip = l4re_kip();
	struct md_t {
		unsigned long _l, _h;
	};
	struct md_t *md
	  = (struct md_t *)((char *)kip +
			    (kip->mem_info >> ((sizeof(unsigned long) / 2) * 8)));
	unsigned long count
	  = kip->mem_info & ((1UL << ((sizeof(unsigned long)/2)*8)) - 1);
	unsigned long i;

	for (i = 0; i < count; ++i, md++) {
		/* Not a virtual descriptor? */
		if (!(md->_l & 0x200))
			continue;

		/* Return start address of descriptor, there should only be
		 * a single one, so just return the first */
		return md->_l & ~0x3ffUL;
	}

	return 0x1000;
}


/* To get mmap of /dev/mem working, map address space before
 * start of mainmem with a ro page,
 * lets try with this... */
static void l4x_mbm_request_ghost(l4re_ds_t *ghost_ds)
{
	unsigned int i;
	void *addr;

	if (l4_is_invalid_cap(*ghost_ds = l4re_util_cap_alloc()))
		l4x_exit_l4linux_msg("%s: Out of caps\n", __func__);

	/* Get a page from our dataspace manager */
	if (l4re_ma_alloc(L4_PAGESIZE, *ghost_ds, L4RE_MA_PINNED))
		l4x_exit_l4linux_msg("%s: Can't get ghost page!\n", __func__);


	/* Poison the page, this is optional */

	/* Map page RW */
	addr = 0;
	if (l4re_rm_attach(&addr, L4_PAGESIZE, L4RE_RM_SEARCH_ADDR,
	                   *ghost_ds | L4_CAP_FPAGE_RW,
	                   0, L4_PAGESHIFT))
		l4x_exit_l4linux_msg("%s: Can't map ghost page\n", __func__);

	/* Write a certain value in to the page so that we can
	 * easily recognize it */
	for (i = 0; i < L4_PAGESIZE; i += sizeof(i))
		*(unsigned int *)((unsigned long)addr + i) = 0xf4f4f4f4;

	/* Detach it again */
	if (l4re_rm_detach(addr))
		l4x_exit_l4linux_msg("%s: Can't unmap ghost page\n", __func__);
}

static void l4x_map_below_mainmem_print_region(l4_addr_t s, l4_addr_t e)
{
	if (!l4x_debug_show_ghost_regions)
		return;
	if (s == ~0UL)
		return;

	LOG_printf("Ghost region: %08lx - %08lx [%4ld]\n", s, e, (e - s) >> 12);
}

static void l4x_map_below_mainmem(void)
{
	unsigned long i;
	l4re_ds_t ds, ghost_ds = L4_INVALID_CAP;
	l4_addr_t off;
	l4_addr_t map_addr;
	unsigned long map_size;
	unsigned flags;
	long ret;
	unsigned long i_inc;
	int map_count = 0, map_count_all = 0;
	l4_addr_t reg_start = ~0UL;

	LOG_printf("Filling lower ptabs...\n");
	LOG_flush();

	/* Loop through free address space before mainmem */
	for (i = get_min_virt_address();
	     i < (unsigned long)l4x_main_memory_start && i < (1UL << 20); i += i_inc) {
		map_addr = i;
		map_size = L4_PAGESIZE;
		ret = l4re_rm_find(&map_addr, &map_size, &off, &flags, &ds);
		if (ret == 0) {
			// success, something there
			if (i != map_addr)
				enter_kdebug("shouldn't be, hmm?");
			i_inc = map_size;
			l4x_map_below_mainmem_print_region(reg_start, i);
			reg_start = ~0UL;
			continue;
		}

		if (reg_start == ~0UL)
			reg_start = i;

		i_inc = L4_PAGESIZE;

		if (ret != -L4_ENOENT)
			l4x_exit_l4linux_msg("l4re_rm_find call failure: %s(%ld)\n",
			                     l4sys_errtostr(ret), ret);

		if (!map_count) {
			/* Get new ghost page every 1024 mappings
			 * to overcome a Fiasco mapping db
			 * limitation. */
			l4x_mbm_request_ghost(&ghost_ds);
			map_count = 1014;
		}
		map_count--;
		map_count_all++;
		map_addr = i;
		if (l4re_rm_attach((void **)&map_addr, L4_PAGESIZE,
		                   L4RE_RM_READ_ONLY | L4RE_RM_EAGER_MAP,
		                   ghost_ds, 0, L4_PAGESHIFT))
			l4x_exit_l4linux_msg("%s: Can't attach ghost page at %lx!\n",
			                     __func__, i);
	}
	l4x_map_below_mainmem_print_region(reg_start, i);
	LOG_printf("Done (%d entries).\n", map_count_all);
	LOG_flush();
}
#endif /* X86 */

#ifdef CONFIG_ARM
unsigned long upage_addr;

static __ref void l4x_setup_upage(void)
{
	l4re_ds_t ds;

	if (l4_is_invalid_cap(ds = l4re_util_cap_alloc())) {
		LOG_printf("%s: Cap alloc failed\n", __func__);
		l4x_linux_main_exit();
	}

	if (l4re_ma_alloc(L4_PAGESIZE, ds, L4RE_MA_PINNED)) {
		LOG_printf("Memory request for upage failed\n");
		l4x_linux_main_exit();
	}

	upage_addr = UPAGE_USER_ADDRESS;
	if (l4re_rm_attach((void **)&upage_addr, L4_PAGESIZE,
	                   L4RE_RM_EAGER_MAP, ds | L4_CAP_FPAGE_RW,
	                   0, L4_PAGESHIFT)) {
		LOG_printf("Cannot attach upage properly\n");
		l4x_linux_main_exit();
	}
}
#endif /* CONFIG_ARM */

static void l4x_register_region(const l4re_ds_t ds, void *start,
                                unsigned long ds_offset, unsigned long size,
                                int allow_noncontig, int may_use_for_dma,
                                const char *tag)
{
	unsigned long offset = 0;
	l4_addr_t virt = (l4_addr_t)start;
	l4re_dma_space_dma_addr_t phys_addr;
	l4_size_t phys_size;

	/* add initial offset to the size we have to encounter */
	size = min(size, l4_round_page(l4re_ds_size(ds) - ds_offset));

	LOG_printf("L4x: %15s: Virt: %p to %p [%lu KiB]\n",
	           tag, start, start + size - 1, size >> 10);

	if (l4_is_invalid_cap(l4x_dma_space)) {
		l4x_v2p_add_item_ds(virt, (void*)virt, size,
		                    may_use_for_dma ? ds : L4_INVALID_CAP,
		                    ds_offset);
		return;
	}

	while (offset < size) {
		phys_size = size - offset;
		if (l4x_phys_mem_disabled
		    || l4re_dma_space_map(l4x_dma_space, ds | L4_CAP_FPAGE_RW,
		                          ds_offset, &phys_size,
		                          0, 0, &phys_addr)) {
			LOG_printf("error: failed to get physical address for %lx.\n",
			           virt);
			break;
		}

		/* assume complete pages, even if the DS API may sometimes
		 * return fractions of a page */
		phys_size = l4_round_page(phys_size);

		if (!allow_noncontig && phys_size < size)
			LOG_printf("Noncontiguous region for %s\n", tag);

		/* limit phys size to the locally mapped range */
		if (offset + phys_size > size)
			phys_size = size - offset;

		LOG_printf("%15s: Phys: 0x%08llx to 0x%08llx, [%zu KiB]\n",
		           tag, phys_addr,
		           phys_addr + phys_size - 1, phys_size >> 10);

		l4x_v2p_add_item_ds(phys_addr, (void *)virt, phys_size,
		                    may_use_for_dma ? ds : L4_INVALID_CAP,
		                    ds_offset);

		offset    += phys_size;
		ds_offset += phys_size;
		virt      += phys_size;
	}
}

/*
 * Register program section(s) for virt_to_phys, at least initdata is used
 * as normal storage later (including DMA usage).
 */
static void l4x_register_range(void *p_in_addr, void *p_in_addr_end,
                               int allow_noncontig, int may_use_for_dma,
                               int allow_holes, const char *tag)
{
	l4re_ds_t ds;
	l4_addr_t off;
	l4_addr_t addr;
	unsigned long size;
	unsigned flags;

	addr = (l4_addr_t)p_in_addr;
	do {
		size = 1;
		if (0 == l4re_rm_find(&addr, &size, &off, &flags, &ds))
			l4x_register_region(ds, (void *)addr, off, size,
			                    allow_noncontig,
			                    may_use_for_dma, tag);
		else if (!allow_holes) {
			LOG_printf("Cannot find anything at %p.\n", p_in_addr);
			l4re_rm_show_lists();
			enter_kdebug("l4re_rm_find failed");
			return;
		} else
			size = L4_PAGESIZE;

		addr += size;
	} while (addr < (l4_addr_t)p_in_addr_end);
}

/* Reserve some part of the virtual address space for vmalloc */
static void __init l4x_reserve_vmalloc_space(void)
{
	unsigned long sz;
	unsigned flags   = L4RE_RM_SEARCH_ADDR;

#if defined(CONFIG_X86_64)
	l4x_vmalloc_memory_start = VMALLOC_START;
	flags &= ~L4RE_RM_SEARCH_ADDR;
	sz = VMALLOC_END - VMALLOC_START + 1;
#elif defined(CONFIG_X86_32)
	l4x_vmalloc_memory_start = (unsigned long)l4x_main_memory_start;
	sz = __VMALLOC_RESERVE;
#elif defined(CONFIG_ARM)
	l4x_vmalloc_memory_start = (unsigned long)l4x_main_memory_start;
	sz = VMALLOC_SIZE << 20;
#else
#error Check this for your architecture
#endif

	if (l4re_rm_reserve_area(&l4x_vmalloc_memory_start,
	                         sz, flags, PGDIR_SHIFT))
		l4x_exit_l4linux_msg("l4x: Error reserving vmalloc memory area of %ldBytes!\n", sz);

	LOG_printf("L4x: vmalloc area: %08lx - %08lx\n",
	           l4x_vmalloc_memory_start,
	           l4x_vmalloc_memory_start + sz);
}

void __init l4x_setup_memory(char *cmdl,
                             unsigned long *main_mem_start,
                             unsigned long *main_mem_size)
{
	long res;
	char *str;
	void *a;
	l4_addr_t memory_area_id = (l4_addr_t)&_text;
	long virt_phys_alignment = L4_SUPERPAGESHIFT;
	l4_uint32_t dm_flags = L4RE_MA_CONTINUOUS | L4RE_MA_PINNED;
	int i;
	unsigned long mem_chunk_sz[ARRAY_SIZE(l4x_ds_mainmem)];
	unsigned long mem_chunk_phys[ARRAY_SIZE(l4x_ds_mainmem)];
	unsigned num_mem_chunk = 0;

	/* See if we find mem-related options in the command line */
	for (i = 0; i < ARRAY_SIZE(l4x_ds_mainmem); ++i) {
		l4x_ds_mainmem[i] = L4_INVALID_CAP;
		mem_chunk_sz[i] = 0;
	}

	if ((str = strstr(cmdl, "l4memds="))) {
		str += 8;

		while (1) {
			long sz;
			l4_cap_idx_t cap;
			char *s = str;
			while (*s && !isspace(*s) && *s != ',')
				s++;

			res = l4x_re_resolve_name_noctx(str, s - str, &cap);
			if (res)
				l4x_exit_l4linux_msg("L4x: Memory cap name "
				                     "'%.*s' failed/invalid (%ld).\n",
				                     (int)(s - str), str, res);

			sz = l4re_ds_size(cap);
			if (sz < 0)
				l4x_exit_l4linux_msg("L4x: Cannot query size of '%.*s' dataspace.\n",
				                     (int)(s - str), str);

			l4x_ds_mainmem[num_mem_chunk] = cap;
			mem_chunk_sz[num_mem_chunk] = sz;

			LOG_printf("L4x: Memory dataspace %d has %ldKiB\n",
			           num_mem_chunk, sz >> 10);

			num_mem_chunk++;

			if (num_mem_chunk == ARRAY_SIZE(mem_chunk_sz))
				break;

			if (*s == ',')
				str = s + 1;
			else
				break;
		}
	}

	if ((str = strstr(cmdl, "mem="))) {
		str += 4;
		while (num_mem_chunk < ARRAY_SIZE(mem_chunk_sz)) {
			res = memparse(str, &str);
			if (res)
				mem_chunk_sz[num_mem_chunk++] = res;

			if (*str == ',' || *str == '+')
				str++;
			else
				break;
		}
	}

	if (num_mem_chunk == 0) {
		mem_chunk_sz[0] = CONFIG_L4_MEMSIZE << 20;
		num_mem_chunk = 1;
	}

	l4x_mainmem_size = 0;
	for (i = 0; i < num_mem_chunk; ++i)
		l4x_mainmem_size += mem_chunk_sz[i];

	if (l4x_mainmem_size == 0)
		l4x_exit_l4linux_msg("l4x: Invalid memory configuration.\n");

	LOG_printf("L4x: Memory size: %ldMB\n", l4x_mainmem_size >> 20);

	// just to avoid strange things are going on
	if (l4x_mainmem_size < (4 << 20))
		l4x_exit_l4linux_msg("Not enough main memory - aborting!\n");

	if ((str = strstr(cmdl, "l4memtype="))) {
		str += 9;
		do {
			str++;
			if (!strncmp(str, "floating", 8)) {
				dm_flags &= ~L4RE_MA_PINNED;
				str += 8;
			} else if (!strncmp(str, "pinned", 6)) {
				dm_flags |= L4RE_MA_PINNED;
				str += 6;
			} else if (!strncmp(str, "distributed", 11)) {
				dm_flags &= ~L4RE_MA_CONTINUOUS;
				str += 11;
			} else if (!strncmp(str, "continuous", 10)) {
				dm_flags |= L4RE_MA_CONTINUOUS;
				str += 10;
			} else
				l4x_exit_l4linux_msg("Unknown l4memtype option, use:\n"
				                     "  floating, pinned, distributed, continuous\n"
				                     "as a comma separated list\n");
		} while (*str == ',');
	}

	if ((str = strstr(cmdl, "l4memalign="))) {
		unsigned long val = memparse(str + 11, NULL);
		if (val > L4_SUPERPAGESHIFT
		    && val < sizeof(unsigned long) * 8) {
			virt_phys_alignment = val;
			LOG_printf("L4x: Setting virt/phys alignment to %ld\n",
			           virt_phys_alignment);
		}
	}

	for (i = 0; i < num_mem_chunk; ++i) {
		l4_uint32_t f = dm_flags;

		if (l4_is_valid_cap(l4x_ds_mainmem[i]))
			continue;

		if ((mem_chunk_sz[i] % L4_SUPERPAGESIZE) == 0) {
			if (num_mem_chunk == 1)
				LOG_printf("L4x: Setting superpages for main memory\n");
			else
				LOG_printf("L4x: Setting superpages for memory chunk %d\n", i + 1);
			/* force ds-mgr to allocate superpages */
			f |= L4RE_MA_SUPER_PAGES;
		}

		/* Allocate main memory */
		if (l4_is_invalid_cap(l4x_ds_mainmem[i] = l4re_util_cap_alloc()))
			l4x_exit_l4linux_msg("%s: Out of caps\n", __func__);

		if (l4re_ma_alloc_align(mem_chunk_sz[i], l4x_ds_mainmem[i],
		                        f, virt_phys_alignment)) {
			LOG_printf("%s: Can't get main memory of %ldkiB!\n",
				   __func__, mem_chunk_sz[i] >> 10);
			l4re_debug_obj_debug(l4re_env()->mem_alloc, 0);
			l4x_exit_l4linux();
		}

		if (num_mem_chunk > 1)
			LOG_printf("L4x: Memory chunk %d, size %ldkiB\n",
			           i, mem_chunk_sz[i] >> 10);
	}

	if (dm_flags & L4RE_MA_PINNED) {
		l4_addr_t p;
		l4_size_t ps;
		if (!l4re_ds_phys(l4x_ds_mainmem[0], 0, &p, &ps)) {
			p &= (1 << virt_phys_alignment) - 1;

			memory_area_id &= ~((1 << virt_phys_alignment) - 1);
			memory_area_id |= p;

			LOG_printf("L4x: Adjusted memory start: %08lx\n",
			           memory_area_id);
		}
	}


	/* Get contiguous region in our virtual address space to put
	 * the dataspaces in */
	if (l4re_rm_reserve_area(&memory_area_id, l4x_mainmem_size,
	                         L4RE_RM_SEARCH_ADDR,
	                         virt_phys_alignment)) {
		LOG_printf("L4x: Error reserving memory area\n");
		l4x_exit_l4linux();
	}


	for (i = 0; i < num_mem_chunk; ++i) {
		l4_addr_t p;
		l4_size_t ps, s = 0;
		l4_addr_t last_phys = 0;
		l4_size_t sz = l4re_ds_size(l4x_ds_mainmem[i]);

		if (sz < 0)
			l4x_exit_l4linux_msg("Failed to get DS size\n");

		while (s < sz) {
			if (l4re_ds_phys(l4x_ds_mainmem[i], s, &p, &ps) < 0) {
				l4x_phys_mem_disabled = 1; /* All, or nothing */
				break;
			}

			if (s == 0)
				mem_chunk_phys[i] = p;

			if (last_phys > p) {
				/* This should not happen with current
				 * implementations, however, if it does,
				 * we could map the pieces of the dataspace
				 * in the right order. */
				LOG_printf("L4x: Reverse order of physical "
				           "memory in chunk %d, offset=%zx\n",
				           i, s);
				if (!l4x_phys_mem_disabled)
					LOG_printf("L4x: Disabling physical "
					           "memory\n");
				l4x_phys_mem_disabled = 1;
			}

			last_phys = p;
			s += ps;
		}
	}

	l4x_main_memory_start = (void *)memory_area_id;

	a = l4x_main_memory_start;
	for (i = 0; i < num_mem_chunk; ++i) {
		int j = 1;
		int p_i = 0;

		for (; j < num_mem_chunk; ++j)
			if (mem_chunk_phys[j] < mem_chunk_phys[p_i])
				p_i = j;

		res = l4re_rm_attach(&a, mem_chunk_sz[p_i],
		                     L4RE_RM_IN_AREA | L4RE_RM_EAGER_MAP,
		                     l4x_ds_mainmem[p_i] | L4_CAP_FPAGE_RW, 0,
		                     MAX_ORDER + PAGE_SHIFT);
		if (res)
			l4x_exit_l4linux_msg("L4x: Error attaching to L4Linux main memory: %ld\n", res);

		l4x_register_region(l4x_ds_mainmem[p_i], a, 0, mem_chunk_sz[p_i],
		                    0, 1, "Main memory");
		a = (char *)a + mem_chunk_sz[p_i];
		mem_chunk_phys[p_i] = ~0ul;
	}

	/* Release area ... make possible hole available again */
	if (l4re_rm_free_area(memory_area_id))
		l4x_exit_l4linux_msg("Error releasing area\n");

	*main_mem_start = (unsigned long)l4x_main_memory_start;
	*main_mem_size  = l4x_mainmem_size;

	l4x_reserve_vmalloc_space();

#ifdef CONFIG_ARM
	{
		extern void * /*__initdata*/ vmalloc_min;
		vmalloc_min = (void *)l4x_vmalloc_memory_start;

		BUILD_BUG_ON(MODULES_VADDR > MODULES_END);
	}
#endif

#ifdef CONFIG_X86
	{
		unsigned long rflags = 0;
#ifdef CONFIG_X86_32
		/* use a dynamic address for the fixmap area on 32bit */
		rflags |= L4RE_RM_SEARCH_ADDR;
		l4x_fixmap_space_start = (unsigned long)l4x_main_memory_start;
#endif
#ifdef CONFIG_X86_64
		/* use a fixed address for the fixmap area on 64bit */
		l4x_fixmap_space_start = FIXADDR_TOP
			                 - __end_of_fixed_addresses * PAGE_SIZE;
#endif
		if (l4re_rm_reserve_area(&l4x_fixmap_space_start,
		                         __end_of_fixed_addresses * PAGE_SIZE,
		                         rflags, PAGE_SHIFT) < 0)
			l4x_exit_l4linux_msg("%s: Failed reserving fixmap space!\n", __func__);

#ifdef CONFIG_X86_32
		__FIXADDR_TOP = l4x_fixmap_space_start
		                 + __end_of_fixed_addresses * PAGE_SIZE;
#endif
	}
#endif /* X86 */

#ifdef CONFIG_X86_64
	{
		unsigned long a = VMEMMAP_START;

		if (VMALLOC_START <= (unsigned long)l4x_main_memory_start
		                     + l4x_mainmem_size)
			l4x_exit_l4linux_msg("Too much memory requested!\n");

		if (l4re_rm_reserve_area(&a, L4X_VMEMMAP_END - a, 0, 20))
			l4x_exit_l4linux_msg("Failed reserving vmemmap space!\n");
	}
#endif

#ifdef CONFIG_X86
	l4x_map_below_mainmem();
#endif

	// that happened with some version of ld...
	if ((unsigned long)&_end < 0x100000)
		LOG_printf("_end == %p, unreasonable small\n", &_end);

	l4x_register_range((void *)((unsigned long)&_text),
	                   (void *)((unsigned long)&_etext),
	                   0, 0, 1, "text");

	l4x_init_dma_phys_low_limit();
}

#if defined(CONFIG_X86_64) || defined(CONFIG_ARM)
static void setup_module_area(void)
{
	l4_addr_t start = MODULES_VADDR;
	if (l4re_rm_reserve_area(&start, MODULES_END - MODULES_VADDR,
	                         0, PGDIR_SHIFT))
		LOG_printf("Could not reserve module area %08lx-%08lx, "
		           "modules won't work!\n",
		           MODULES_VADDR, MODULES_END);
}
#endif

#ifdef CONFIG_ARM
void *xlate_dev_mem_and_kmem_ptr_l4x(unsigned long x)
{
	if (MODULES_VADDR <= x && x < MODULES_END)
		return empty_zero_page;
	return (void *)x;
}

/* Sanity check for hard-coded values used in dispatch.c */
static int l4x_sanity_check_kuser_cmpxchg(void)
{
#if __LINUX_ARM_ARCH__ < 6
	extern char l4x_kuser_cmpxchg_critical_start[];
	extern char l4x_kuser_cmpxchg_critical_end[];

	if (   (unsigned long)l4x_kuser_cmpxchg_critical_start != L4X_UPAGE_KUSER_CMPXCHG_CRITICAL_START
	    || (unsigned long)l4x_kuser_cmpxchg_critical_end   != L4X_UPAGE_KUSER_CMPXCHG_CRITICAL_END) {
		LOG_printf("__kuser_cmpxchg value incorrect; Aborting.\n");
		return 1;
	}
#endif
	return 0;
}

#endif

static void l4x_create_ugate(l4_cap_idx_t forthread, unsigned cpu)
{
	l4_msgtag_t r;

	l4x_user_gate[cpu] = l4x_cap_alloc_noctx();
	if (l4_is_invalid_cap(l4x_user_gate[cpu]))
		LOG_printf("Error getting cap\n");
	r = l4_factory_create_gate(l4re_env()->factory,
	                           l4x_user_gate[cpu],
	                           forthread, 0x10);
	if (l4_error(r))
		LOG_printf("Error creating user-gate %d, error=%lx\n",
		           cpu, l4_error(r));

	l4x_dbg_set_object_name(l4x_user_gate[cpu], "ugate%d", cpu);
}

#ifdef CONFIG_HOTPLUG_CPU
void l4x_destroy_ugate(unsigned cpu)
{
	long e;
	e = L4XV_FN_e(l4_task_delete_obj(L4RE_THIS_TASK_CAP,
	              l4x_user_gate[cpu]));
	if (e)
		l4x_printf("Error destroying user-gate%d\n", cpu);
	l4x_cap_free(l4x_user_gate[cpu]);
	l4x_user_gate[cpu] = L4_INVALID_CAP;
}
#endif

unsigned l4x_cpu_physmap_get_id(unsigned lcpu)
{
	return l4x_cpu_physmap[lcpu].phys_id;
}

l4lx_thread_t l4x_cpu_threads[NR_CPUS];
l4_cap_idx_t  l4x_cpu_thread_caps[NR_CPUS];

l4lx_thread_t l4x_cpu_thread_get(int cpu)
{
	BUG_ON(cpu >= NR_CPUS);
	return l4x_cpu_threads[cpu];
}

l4_cap_idx_t l4x_cpu_thread_get_cap(int cpu)
{
	return l4lx_thread_get_cap(l4x_cpu_thread_get(cpu));
}

static void l4x_cpu_thread_set(int cpu, l4lx_thread_t tid)
{
	BUG_ON(cpu >= NR_CPUS);
	l4x_cpu_threads[cpu] = tid;
}

#ifdef CONFIG_SMP

#include <l4/sys/ktrace.h>

#ifdef CONFIG_X86
void l4x_load_percpu_gdt_descriptor(struct desc_struct *gdt)
{
#ifdef CONFIG_X86_32
	long r;
	int nr = IS_ENABLED(CONFIG_L4_VCPU) ? 2 : 0;

	/* we need a DPL=3 descriptor or Fiasco will not set it */
	struct desc_struct desc = gdt[GDT_ENTRY_PERCPU];
	desc.dpl = 3;

	if ((r = fiasco_gdt_set(L4_INVALID_CAP,
	                        &desc, 8, nr, l4_utcb())) < 0)
		LOG_printf("GDT setting failed: %ld\n", r);
	asm("mov %0, %%fs"
	    : : "r" ((l4x_x86_fiasco_gdt_entry_offset + nr) * 8 + 3) : "memory");
#endif
}
#endif

static int l4x_cpu_check_pcpu(unsigned pcpu, l4_umword_t max_cpus)
{
	return pcpu < max_cpus
	       && l4_scheduler_is_online(l4re_env()->scheduler, pcpu);
}

static struct task_struct *l4x_cpu_idler[NR_CPUS] = { &init_task, 0, };

int l4x_cpu_cpu_get(void)
{
	int i = 0;
	l4_cap_idx_t id = l4x_cap_current();

	for (; i < NR_CPUS; i++)
		if (l4x_cpu_threads[i] &&
		    l4_capability_equal(id, l4lx_thread_get_cap(l4x_cpu_threads[i])))
			return i;

	BUG();
}

struct task_struct *l4x_cpu_idle_get(int cpu)
{
	BUG_ON(cpu >= NR_CPUS);
	return l4x_cpu_idler[cpu];
}

#ifndef CONFIG_L4_VCPU
static l4lx_thread_t l4x_cpu_ipi_threads[NR_CPUS];
#endif
static l4_cap_idx_t  l4x_cpu_ipi_irqs[NR_CPUS];
static l4_umword_t   l4x_cpu_ipi_vector_mask[NR_CPUS];

#ifndef CONFIG_L4_VCPU
#include <asm/generic/sched.h>
#include <asm/generic/do_irq.h>
#include <l4/sys/irq.h>
static __noreturn L4_CV void l4x_cpu_ipi_thread(void *x)
{
	unsigned cpu = *(unsigned *)x;
	l4_msgtag_t tag;
	l4_cap_idx_t cap;
	int err;
	l4_utcb_t *utcb = l4_utcb();

	l4x_prepare_irq_thread(current_stack_pointer, cpu);

	// wait until parent has setup our cap and attached us
	while (l4_is_invalid_cap(l4x_cpu_ipi_irqs[cpu])) {
		mb();
		l4_sleep(2);
	}

	cap = l4x_cpu_ipi_irqs[cpu];

	while (1) {
		tag = l4_irq_receive(cap, L4_IPC_NEVER);
		if (unlikely(err = l4_ipc_error(tag, utcb)))
			LOG_printf("ipi%d: ipc error %d (cap: %lx)\n",
			           cpu, err, cap);

		while (1) {
			l4_umword_t ret, mask;
			unsigned vector;
			do {
				mask = l4x_cpu_ipi_vector_mask[cpu];
				ret = l4util_cmpxchg(&l4x_cpu_ipi_vector_mask[cpu],
				                     mask, 0);
			} while (unlikely(!ret));

			if (!mask)
				break;

			while (mask) {
				vector = ffs(mask) - 1;
				l4x_do_IPI(vector);
				mask &= ~(1 << vector);
			}
		}
	}
}
#else /* vcpu */
void l4x_vcpu_handle_ipi(struct pt_regs *regs)
{
	unsigned cpu = smp_processor_id();

	while (1) {
		l4_umword_t ret, mask;
		unsigned vector;
		do {
			mask = l4x_cpu_ipi_vector_mask[cpu];
			ret = l4util_cmpxchg(&l4x_cpu_ipi_vector_mask[cpu],
			                     mask, 0);
		} while (unlikely(!ret));

		if (!mask)
			break;

		while (mask) {
			vector = ffs(mask) - 1;
			l4x_smp_process_IPI(vector, regs);
			mask &= ~(1 << vector);
		}
	}
}
#endif


void l4x_cpu_ipi_setup(unsigned cpu)
{
	l4_msgtag_t t;
	l4_cap_idx_t c;
	char s[14];

#ifdef CONFIG_L4_VCPU
	BUG_ON(l4x_vcpu_ptr[cpu]->state & L4_VCPU_F_IRQ);
#endif

	snprintf(s, sizeof(s), "ipi%d", cpu);
	s[sizeof(s) - 1] = 0;

	l4x_cpu_ipi_irqs[cpu] = L4_INVALID_CAP;

#ifndef CONFIG_L4_VCPU
	if (l4lx_thread_create(&l4x_cpu_ipi_threads[cpu],
	                       l4x_cpu_ipi_thread,
	                       cpu,
	                       NULL, &cpu, sizeof(cpu),
	                       l4x_cap_alloc(),
	                       l4lx_irq_prio_get(0), 0, 0, s, NULL))
		l4x_exit_l4linux_msg("Failed to create thread %s\n", s);
#endif

	c = l4x_cap_alloc_noctx();
	if (l4_is_invalid_cap(c))
		l4x_exit_l4linux_msg("Failed to alloc cap\n");

	t = l4_factory_create_irq(l4re_env()->factory, c);
	if (l4_error(t))
		l4x_exit_l4linux_msg("Failed to create IRQ\n");

	l4x_dbg_set_object_name(c, "ipi%d", cpu);

#ifdef CONFIG_L4_VCPU
	t = l4_rcv_ep_bind_thread(c, l4lx_thread_get_cap(l4x_cpu_threads[cpu]),
	                          L4X_VCPU_IRQ_IPI << 2);
#else
	t = l4_rcv_ep_bind_thread(c,
	                          l4lx_thread_get_cap(l4x_cpu_ipi_threads[cpu]),
	                          0);
#endif
	if (l4_error(t))
		l4x_exit_l4linux_msg("Failed to attach IPI IRQ%d\n", cpu);

	// now commit and make cap visible to child, it can run now
	mb();
	l4x_cpu_ipi_irqs[cpu] = c;
}

void l4x_cpu_ipi_stop(unsigned cpu)
{
	long e;

	e = L4XV_FN_e(l4_irq_detach(l4x_cpu_ipi_irqs[cpu]));
	if (e) {
		l4x_printf("Failed to detach for IPI IRQ%d\n", cpu);
		return;
	}

	e = L4XV_FN_e(l4_task_delete_obj(L4RE_THIS_TASK_CAP,
	              l4x_cpu_ipi_irqs[cpu]));
	if (e) {
		l4x_printf("Failed to unmap IPI IRQ%d\n", cpu);
		return;
	}

	l4x_cap_free(l4x_cpu_ipi_irqs[cpu]);
	l4x_cpu_ipi_irqs[cpu] = L4_INVALID_CAP;

#ifndef CONFIG_L4_VCPU
	l4lx_thread_shutdown(l4x_cpu_ipi_threads[cpu], 1, NULL, 1);
#endif
}

static L4_CV void __cpu_starter(void *data)
{
	int cpu = *(int *)data;
	l4_utcb_t *u = l4_utcb();

	l4x_stack_set(current_stack_pointer, u);

#ifdef CONFIG_X86
	load_percpu_segment(cpu);
#endif

	l4x_create_ugate(l4x_cap_current_utcb(u), cpu);
	l4lx_thread_pager_change(l4x_cap_current_utcb(u), l4x_start_thread_id);

#ifdef CONFIG_L4_VCPU
	l4x_vcpu_init(l4x_vcpu_ptr[cpu]);
#else
	l4x_global_cli();
#endif

#ifdef CONFIG_X86
	l4x_stack_set(initial_stack & ~(THREAD_SIZE - 1), u);
#ifdef CONFIG_X86_32
	asm volatile ("mov (initial_stack), %esp; jmp *(initial_code)");
#endif /* X86_32 */
#ifdef CONFIG_X86_64
	asm volatile ("mov (initial_stack), %rsp; jmp *(initial_code)");
#endif /* X86_64 */
#endif /* X86 */
#ifdef CONFIG_ARM
	l4x_arm_secondary_start_kernel();
#endif
	panic("CPU startup failed");
}


static struct l4lx_thread_start_info_t l4x_cpu_bootup_state[NR_CPUS];
struct l4x_cpu_bootup_stack {
	unsigned long stack[0x2000];
};
static struct l4x_cpu_bootup_stack l4x_cpu_bootup_stacks[NR_CPUS] __attribute__((aligned(sizeof(struct l4x_cpu_bootup_stack))));

void l4x_cpu_spawn(int cpu, struct task_struct *idle)
{
	char name[8];

	BUG_ON(cpu >= NR_CPUS);

#ifndef CONFIG_L4_VCPU
	l4x_tamed_set_mapping(cpu, l4x_cpu_physmap_get_id(cpu));
	l4x_tamed_start(cpu);
#endif

	snprintf(name, sizeof(name), "cpu%d", cpu);
	name[sizeof(name)-1] = 0;

	l4x_cpu_idler[cpu] = idle;
	mb();

	l4x_printf("Launching %s on pcpu %d at %p\n",
	           name, l4x_cpu_physmap_get_id(cpu), __cpu_starter);

	/* CPU threads kill themselves, not being able to free the
	 * cap-slot, so we need to reuse */
	if (!l4x_cpu_thread_caps[cpu])
		l4x_cpu_thread_caps[cpu] = l4x_cap_alloc_noctx();

	/* Now wait until the CPU really disappeared,
	 * avoiding the create/delete race */
	while (1) {
		l4_umword_t r;
		r = L4XV_FN(l4_umword_t,
		            l4_ipc_error(l4_thread_switch(l4x_cpu_thread_caps[cpu]),
		                         l4_utcb()));
		if (r == L4_IPC_ENOT_EXISTENT)
			break;
		msleep(1);
	}

	while (1) {
		if (!L4XV_FN_i(l4lx_thread_create(&l4x_cpu_threads[cpu],
		                                  __cpu_starter, cpu,
		                                  &l4x_cpu_bootup_stacks[cpu].stack[ARRAY_SIZE(l4x_cpu_bootup_stacks[cpu].stack)],
		                                  &cpu, sizeof(cpu),
		                                  l4x_cpu_thread_caps[cpu],
		                                  CONFIG_L4_PRIO_SERVER_PROC,
		                                  l4x_cpu_threads[cpu],
		                                  &l4x_vcpu_ptr[cpu],
		                                  name, &l4x_cpu_bootup_state[cpu])))
			break;

		l4x_evict_tasks(NULL);
	}
}

void l4x_cpu_release(int cpu)
{
	L4XV_FN_v(l4lx_thread_start(&l4x_cpu_bootup_state[cpu]));
}

void l4x_cpu_ipi_trigger(unsigned cpu)
{
	long e = L4XV_FN_e(l4_irq_trigger(l4x_cpu_ipi_irqs[cpu]));
	if (unlikely(e != L4_PROTO_IRQ))
		l4x_printf("Trigger of IPI%d failed\n", cpu);
}

void l4x_cpu_ipi_enqueue_vector(unsigned cpu, unsigned vector)
{
	l4_umword_t old, ret;
	do {
		old = l4x_cpu_ipi_vector_mask[cpu];
		ret = l4util_cmpxchg(&l4x_cpu_ipi_vector_mask[cpu],
		                     old, old | (1 << vector));
	} while (!ret);
}

#ifdef CONFIG_X86
void l4x_send_IPI_mask_bitmask(unsigned long mask, int vector)
{
	int cpu;

	if (unlikely(vector >= (sizeof(unsigned long) * 8)))
		l4x_printf("BIG vector %d (caller %lx)\n", vector, _RET_IP_);

	BUILD_BUG_ON(NR_CPUS > sizeof(l4x_cpu_ipi_vector_mask[cpu]) * 8);
	for (cpu = 0; cpu < NR_CPUS; cpu++) {
		if (!(mask & (1UL << cpu)))
			continue;

		l4x_cpu_ipi_enqueue_vector(cpu, vector);
		l4x_cpu_ipi_trigger(cpu);
	}
}

__visible void smp_irq_work_interrupt(struct pt_regs *regs);

void l4x_smp_process_IPI(int vector, struct pt_regs *regs)
{
	switch (vector | L4X_SMP_IPI_VECTOR_BASE) {
		case RESCHEDULE_VECTOR:
			smp_reschedule_interrupt(regs);
			break;
		case CALL_FUNCTION_VECTOR:
			smp_call_function_interrupt(regs);
			break;
		case CALL_FUNCTION_SINGLE_VECTOR:
			smp_call_function_single_interrupt(regs);
			break;
		case REBOOT_VECTOR:
			irq_enter();
			stop_this_cpu(NULL);
			irq_exit();
			break;
		case IRQ_WORK_VECTOR:
			smp_irq_work_interrupt(regs);
			break;
		default:
			LOG_printf("Spurious IPI on CPU%d: %x\n",
			           smp_processor_id(), vector);
			break;
	}
}
#endif

void l4x_migrate_thread(l4_cap_idx_t thread, unsigned from_cpu, unsigned to_cpu)
{
	l4_msgtag_t t;
	l4_sched_param_t sp = l4_sched_param(CONFIG_L4_PRIO_SERVER_PROC, 0);
	sp.affinity = l4_sched_cpu_set(l4x_cpu_physmap_get_id(to_cpu), 0, 1);

	if (l4x_cpu_physmap_get_id(to_cpu) == l4x_cpu_physmap_get_id(from_cpu))
		return;

	t = l4_scheduler_run_thread(l4re_env()->scheduler, thread, &sp);
	if (l4_error(t))
		printk("Failed to migrate thread %lx\n", thread);
}

//----------------------
// repnop start
#ifndef CONFIG_L4_VCPU
l4lx_thread_t l4x_repnop_id;

static char l4x_repnop_stack[L4LX_THREAD_STACK_SIZE];

static L4_CV void l4x_repnop_thread(void *d)
{
	l4_cap_idx_t label;
	l4_msgtag_t tag;
	int error;
	l4_utcb_t *u = l4_utcb();

	tag = l4_ipc_wait(u, &label, L4_IPC_SEND_TIMEOUT_0);
	while (1) {
		if ((error = l4_ipc_error(tag, u)))
			LOG_printf("%s: IPC error = %x\n", __func__, error);

		tag = l4_ipc_reply_and_wait(u,
		                            l4_msgtag(0, 0, 0, L4_MSGTAG_SCHEDULE),
		                            &label, L4_IPC_SEND_TIMEOUT_0);
	}
}

void l4x_rep_nop(void)
{
	l4_msgtag_t t;
	t = l4_ipc_call(l4lx_thread_get_cap(l4x_repnop_id), l4_utcb(),
	                l4_msgtag(0, 0, 0, L4_MSGTAG_SCHEDULE), L4_IPC_NEVER);
	BUG_ON(l4_ipc_error(t, l4_utcb()));
}

static void l4x_repnop_init(void)
{
	l4lx_thread_create(&l4x_repnop_id,
	                   l4x_repnop_thread, 0,
	                   l4x_repnop_stack
	                     + sizeof(l4x_repnop_stack),
	                   NULL, 0, l4x_cap_alloc_noctx(),
	                   CONFIG_L4_PRIO_SERVER_PROC - 1,
	                   0, 0, "nop", NULL);
}
// repnop end
// ---------------
#endif

#else
#ifndef CONFIG_L4_VCPU
static inline void l4x_repnop_init(void) {}
#endif
#endif

static __init int l4x_cpu_virt_phys_map_init(const char *boot_command_line)
{
	l4_umword_t max_cpus = 1;
	l4_sched_cpu_set_t cs = l4_sched_cpu_set(0, 0, 0);
	unsigned i;

#ifdef CONFIG_SMP
	char overbooking = 0;
	char *p;

	if ((p = strstr(boot_command_line, "l4x_cpus="))) {
		l4x_nr_cpus = simple_strtoul(p + 9, NULL, 0);
		if (l4x_nr_cpus > NR_CPUS) {
			LOG_printf("Linux only configured for max. %d CPUs. Limited to %d.\n",
			           NR_CPUS, NR_CPUS);
			l4x_nr_cpus = NR_CPUS;
		}
	}


	if (l4_error(l4_scheduler_info(l4re_env()->scheduler,
	                               &max_cpus, &cs)) == L4_EOK) {
		if ((p = strstr(boot_command_line, "l4x_cpus_map="))) {
			// l4x_cpus_map=0,1,2,3,4,...
			// the list specifies the physical CPU for each
			// logical, the number of logical CPUs is limited to
			// the size of the given list
			unsigned pcpu;
			char *q;
			p += 13;
			l4x_nr_cpus = 0;
			while (*p && *p != ' ') {

				if (l4x_nr_cpus >= NR_CPUS) {
					LOG_printf("ERROR: vCPU%d out of bounds\n", l4x_nr_cpus);
					return 1;
				}

				pcpu = simple_strtoul(p, &q, 0);
				if (p == q) {
					LOG_printf("ERROR: Error parsing l4x_cpus_map option\n");
					return 1;
				}
				if (!l4x_cpu_check_pcpu(pcpu, max_cpus)) {
					LOG_printf("ERROR: pCPU%d not found\n", pcpu);
					return 1;
				}
				l4x_cpu_physmap[l4x_nr_cpus].phys_id = pcpu;
				for (i = 0; i < l4x_nr_cpus; ++i)
					overbooking |=
					   l4x_cpu_physmap[i].phys_id == pcpu;
				l4x_nr_cpus++;
				if (*q != ',')
					break;
				p = q + 1;
			}
		} else {
			unsigned p = 0;
			unsigned v;
			for (v = 0; v < NR_CPUS && v < l4x_nr_cpus; ++v) {
				while (p < max_cpus
				       && !l4x_cpu_check_pcpu(p, max_cpus))
					p++;
				if (p == max_cpus)
					break;
				l4x_cpu_physmap[v].phys_id = p++;
			}
			l4x_nr_cpus = v;

		}
	}
#ifndef CONFIG_L4_VCPU
	l4x_tamed_set_mapping(0, l4x_cpu_physmap_get_id(0));
#endif

#else /* UP */
	if (l4_error(l4_scheduler_info(l4re_env()->scheduler,
	                               &max_cpus, &cs)) == L4_EOK) {

		int p = 0;
		if (cs.map)
			p = find_first_bit(&cs.map, sizeof(cs.map) * 8);
		l4x_cpu_physmap[0].phys_id = p;
	}
#endif

	LOG_printf("CPU mapping (l:p)[%d]: ", l4x_nr_cpus);
	for (i = 0; i < l4x_nr_cpus; i++)
		LOG_printf("%u:%u%s", i, l4x_cpu_physmap[i].phys_id,
		           i == l4x_nr_cpus - 1 ? "\n" : ", ");

#if defined(CONFIG_SMP) && defined(CONFIG_ARM) && defined(CONFIG_L4_VCPU)
	if (overbooking) {
		LOG_printf("  On ARM it is not possible to place multiple\n"
		           "  logical CPUs on a single physical CPU as the TLS\n"
			   "  register handling cannot be virtualized.\n");
		// On a longer thought we'd need to find out on which vCPU
		// we're running. The utcb address should be available via
		// tls-reg-3 and then we'd need to find out the current tls
		// value for that vCPU, maybe by putting a page at the same
		// place as the utcb is in the linux server address space.
		// Or: We just do a system call.
		// But: Userspace also likes to do NOT use the kernel
		// provided tls code (e.g. Android) so this would not work
		// at all.
		enter_kdebug("Please read the message");
	}
#endif

#ifdef CONFIG_X86_LOCAL_APIC
	num_processors = l4x_nr_cpus;
#endif

	return 0;
}



#ifdef CONFIG_PCI
#ifdef CONFIG_X86_IO_APIC
void __init check_acpi_pci(void)
{}
#endif
#endif

/*
 * This is the panic blinking function, we misuse it to sleep forever.
 */
static long l4x_blink(int state)
{
	L4XV_V(f);

	WARN_ON(1);
	printk("panic: going to sleep forever, bye\n");
	L4XV_L(f);
	LOG_printf("panic: going to sleep forever, bye\n");
	l4_sleep_forever();
	return 0;
}

/**
 * Our custom pm_power_off implementation
 */
static void l4x_power_off(void)
{
	l4x_platform_shutdown(0);
}

static L4_CV void __init cpu0_startup(void *data)
{
	l4_cap_idx_t cpu0id = l4x_cpu_thread_get_cap(0);
	struct thread_info *ti;

	l4x_stack_set(current_stack_pointer, l4_utcb());

#ifdef CONFIG_X86
	load_percpu_segment(0);
#endif

	l4x_create_ugate(cpu0id, 0);
	l4lx_thread_pager_change(cpu0id, l4x_start_thread_id);

#ifdef CONFIG_ARM
	set_my_cpu_offset(0);
#endif
#ifdef CONFIG_L4_VCPU
	l4x_vcpu_init(l4x_vcpu_ptr[0]);
#else
	local_irq_disable();
#endif

	ti = current_thread_info();
	*ti = (struct thread_info) INIT_THREAD_INFO(init_task);

	panic_blink = l4x_blink;

#ifdef CONFIG_X86
	// todo: also do more ..._early_setup stuff here
	legacy_pic = &null_legacy_pic;

	setup_clear_cpu_cap(X86_FEATURE_SEP);
	setup_clear_cpu_cap(X86_FEATURE_TSC_ADJUST);
#endif

	/* set our custom power off function */
	pm_power_off = l4x_power_off;

	l4x_init_finished = true;
#ifdef CONFIG_X86_32
	i386_start_kernel();
#elif defined(CONFIG_X86_64)
	l4x_stack_set((unsigned long)per_cpu(irq_stack_union.irq_stack, 0),
	              l4_utcb());
	x86_64_start_kernel(NULL);
#else
	start_kernel();
#endif
}

enum { L4X_PATH_BUF_SIZE = 200 };
#if defined(CONFIG_BLK_DEV_INITRD) || defined(CONFIG_OF)
static char l4x_path_buf[L4X_PATH_BUF_SIZE];
#endif

#ifdef CONFIG_BLK_DEV_INITRD
static l4re_ds_t l4x_initrd_ds;
static unsigned long l4x_initrd_mem_start;

/**
 * Get the RAM disk from the file provider!
 *
 * \param filename    File name in the form {(nd)}/path/to/file
 * \param *rd_start   Start of ramdisk
 * \param *rd_end     End of ramdisk
 *
 * \return 0 on succes, != 0 otherwise
 */
static int fprov_load_initrd(const char *filename,
                             unsigned long *rd_start,
                             unsigned long *rd_end)
{
	int ret;
	l4re_ds_stats_t dsstat;

	if ((ret = l4x_query_and_get_ds(filename, "initrd", &l4x_initrd_ds,
	                                (void **)rd_start, &dsstat))) {
		LOG_printf("Failed to get initrd: %s(%d)\n",
		           l4sys_errtostr(ret), ret);
		*rd_start = *rd_end = 0;
		return -1;
	}

	LOG_printf("INITRD: Size of RAMdisk is %ldKiB\n", dsstat.size >> 10);

	l4x_initrd_mem_start = *rd_start;
	*rd_end = *rd_start + dsstat.size;
	printk("INITRD: %08lx - %08lx\n", *rd_start, *rd_end);

	if (dsstat.size * 2 > l4x_mainmem_size) {
		LOG_printf("WARNING: RAMdisk size of %ldMB probably too big\n"
		           "for %ldMB main memory. Sleeping a bit...\n",
		           dsstat.size >> 20, l4x_mainmem_size >> 20);
		l4_sleep(20000);
	}

	LOG_flush();
	return 0;
}

/**
 * Free InitRD memory.
 */
void l4x_free_initrd_mem(void)
{
	int r;

	if (!l4x_initrd_mem_start)
		return;

	printk("INITRD: Freeing memory.\n");

	if ((r = L4XV_FN_i(l4re_rm_detach((void *)l4x_initrd_mem_start)))) {
		pr_err("l4x: Error detaching from initrd mem (%d)!", r);
		return;
	}

	r = L4XV_FN_e(l4_task_release_cap(L4_BASE_TASK_CAP, l4x_initrd_ds));
	if (r) {
		pr_err("l4x: Error releasing initrd dataspace\n");
		return;
	}

	l4x_cap_free(l4x_initrd_ds);
}


void __init l4x_load_initrd(const char *command_line)
{
	const char *sa, *se;
	char param_str[] = "l4x_rd=";
	int i, b;

	sa = command_line;
	while (*sa) {
		for (i = 0, b = 1; param_str[i] && b; i++)
			b = sa[i] == param_str[i];
		if (b)
			break;
		sa++;
	}
	if (*sa) {
		sa += strlen(param_str);
		se = sa;

		while (*se && *se != ' ')
			se++;
		if (se - sa > L4X_PATH_BUF_SIZE - 1) {
			enter_kdebug("l4x_rd too big to store");
			return;
		}
		strncpy(l4x_path_buf, sa, se - sa);
		l4x_path_buf[se - sa] = 0;
	}

	if (*l4x_path_buf) {
		LOG_printf("Loading: %s\n", l4x_path_buf);

		if (fprov_load_initrd(l4x_path_buf,
		                      &initrd_start,
		                      &initrd_end)) {
			LOG_flush();
			LOG_printf("Couldn't load ramdisk :(\n");
			return;
		}

		initrd_below_start_ok = 1;

		LOG_printf("RAMdisk from %08lx to %08lx [%ldKiB]\n",
		           initrd_start, initrd_end,
		           (initrd_end - initrd_start) >> 10);
	}
}
#endif /* CONFIG_BLK_DEV_INITRD */

#ifdef CONFIG_OF
unsigned long __init l4x_load_dtb(const char *cmdline,
                                  unsigned long mainmem_offset)
{
	char *p, *e;
	int ret;
	l4re_ds_t ds;
	void *dtb;
	l4re_ds_stats_t dsstat;
	unsigned long phys;
	char *virt;

	if (!(p = strstr(cmdline, "l4x_dtb=")))
		return 0;

	p += 8;
	e = p;
	while (*e && !isspace(*e))
		++e;

	if (e - p > L4X_PATH_BUF_SIZE - 1) {
		pr_err("L4x: DTB path too long\n");
		return 0;
	}

	strncpy(l4x_path_buf, p, e - p);
	l4x_path_buf[e - p] = 0;

	if ((ret = l4x_query_and_get_ds(l4x_path_buf, "dtb", &ds,
	                                &dtb, &dsstat))) {
		pr_err("L4x: Failed to get '%s': %d\n", l4x_path_buf, ret);
		return 0;
	}

	if (mainmem_offset + dsstat.size >= l4x_mainmem_size) {
		pr_err("DTB: size mismatch\n");
		return 0;
	}

	virt = (char *)l4x_main_memory_start + mainmem_offset;

	memcpy(virt, dtb, dsstat.size);

	l4x_detach_and_free_ds(ds, dtb);

	phys = virt_to_phys((char *)l4x_main_memory_start + mainmem_offset);

	pr_info("DTB: virt=%p phys=%lx\n", virt, phys);

	return phys;
}
#endif


static void get_initial_cpu_capabilities(void)
{
#ifdef CONFIG_X86_32
	/* new_cpu_data is filled into boot_cpu_data in setup_arch */
	extern struct cpuinfo_x86 new_cpu_data;
	new_cpu_data.x86_capability[0] = l4util_cpu_capabilities();
#endif
}

#ifdef CONFIG_X86

#include <asm/l4x/ioport.h>

static u32 l4x_x86_kernel_ioports[65536 / sizeof(u32)];

int l4x_x86_handle_user_port_request(struct task_struct *task,
                                     unsigned nr, unsigned len)
{
	long e;
	unsigned char o;
	unsigned i;

	if (nr + len > 65536)
		return 0;

	for (i = 0; i < len; ++i)
		if (!test_bit(nr + i,
		              (volatile unsigned long *)l4x_x86_kernel_ioports))
			return 0;

	i = nr;
	do {
		o = l4_fpage_max_order(0, i, nr, nr + len, 0);
		e = L4XV_FN_e(l4_task_map(task->mm->context.task,
		                          L4RE_THIS_TASK_CAP,
		                          l4_iofpage(nr, o),
		                          l4_map_control(nr, 0, L4_MAP_ITEM_MAP)));
		if (e)
			return 0;

		i += 1 << o;
	} while (i < nr + len);

	return 1;
}

static void l4x_x86_register_ports(l4io_resource_t *res)
{
	unsigned i;

	if (res->type != L4IO_RESOURCE_PORT)
		return;

	if (res->start > res->end)
		return;

	for (i = res->start; i <= res->end; ++i)
		if (i < 65536)
			set_bit(i,
			        (volatile unsigned long *)l4x_x86_kernel_ioports);

}

static void l4x_x86_get_wallclock_noop(struct timespec *ts)
{
	ts->tv_sec = 0;
	ts->tv_nsec = 0;
}

static int l4x_x86_set_wallclock_noop(const struct timespec *ts)
{
	return -EINVAL;
}

static void l4x_clock_init(void)
{
	if (IS_ENABLED(CONFIG_L4_EXTERNAL_RTC)) {
		x86_platform.get_wallclock = l4x_x86_get_wallclock_noop;
		x86_platform.set_wallclock = l4x_x86_set_wallclock_noop;
	}
}
#endif

static void l4x_scan_hw_resources(void)
{
	l4io_device_handle_t dh = l4io_get_root_device();
	l4io_device_t dev;
	l4io_resource_handle_t reshandle;

	LOG_printf("Device scan:\n");
	while (1) {
		l4io_resource_t res;

		if (l4io_iterate_devices(&dh, &dev, &reshandle))
			break;

		LOG_printf("  Device: %s\n", dev.name);

		if (dev.num_resources == 0)
			continue;

		while (!l4io_lookup_resource(dh, L4IO_RESOURCE_ANY,
					     &reshandle, &res)) {
			const char *t = "undef";

			switch (res.type) {
				case L4VBUS_RESOURCE_IRQ:  t = "IRQ";  break;
				case L4VBUS_RESOURCE_MEM:  t = "MEM";  break;
				case L4VBUS_RESOURCE_PORT: t = "PORT"; break;
				case L4VBUS_RESOURCE_BUS:  t = "BUS";  break;
				case L4VBUS_RESOURCE_GPIO: t = "GPIO"; break;
				case L4VBUS_RESOURCE_DMA_DOMAIN:
				                           t = "DMAD"; break;
			};

			LOG_printf("    %s: %08lx - %08lx\n",
			           t, res.start, res.end);

#ifdef CONFIG_X86
			if (res.type == L4VBUS_RESOURCE_PORT) {
				l4x_x86_register_ports(&res);
				l4io_request_ioport(res.start,
				                    res.end - res.start + 1);
			}
#endif
		}
	}
	LOG_printf("Device scan done.\n");
}

#ifdef CONFIG_L4_SERVER
static struct l4x_svr_ops _l4x_svr_ops = {
	.cap_alloc = &l4x_cap_alloc,
	.cap_free  = &l4x_cap_free,
};
#endif

static int l4x_setup_dma(void)
{
	int r;
	l4_utcb_t *utcb = l4_utcb();
	l4_cap_idx_t vbus = l4re_env_get_cap("vbus");
	l4x_dma_space = L4_INVALID_CAP;

	if (!l4_is_valid_cap(vbus))
		return 0; /* no vBUS, no DMA */

	l4x_dma_space = l4re_util_cap_alloc();
	if (l4_is_invalid_cap(l4x_dma_space)) {
		/* this should never happen */
		LOG_printf("%s: fatal: Out of caps\n", __func__);
		l4x_exit_l4linux();
	}

	r = l4_error(l4_factory_create_u(l4re_env()->mem_alloc,
	                                 L4RE_PROTO_DMA_SPACE, l4x_dma_space,
	                                 utcb));
	if (r < 0) {
		LOG_printf("error: could not create DMA address space: %d\n"
		           "       DMA will not work!\n", r);
		goto err_release_cap;
	}

	r = l4vbus_assign_dma_domain(vbus, ~0U,
	                             L4VBUS_DMAD_BIND
	                             | L4VBUS_DMAD_L4RE_DMA_SPACE,
	                             l4x_dma_space);
	if (r < 0) {
		LOG_printf("error: could not assign DMA space to vBUS: %d\n"
		           "       DMA will not work!\n", r);
		goto err_release_dma_domain;
	}

	return 0;

err_release_dma_domain:
	l4_task_release_cap(L4RE_THIS_TASK_CAP, l4x_dma_space);

err_release_cap:
	l4re_util_cap_free(l4x_dma_space);
	l4x_dma_space = L4_INVALID_CAP;
	return r;
}

static void print_sect(const char *name, void *s, void *e)
{
	unsigned long sz = (unsigned long)e - (unsigned long)s;

	LOG_printf("%s %08lx - %08lx [%ldkB]\n",
	           name, (unsigned long)s, (unsigned long)e, sz >> 10);
}

static int l4x_alloc_anon_mem_to(l4_addr_t start, l4_addr_t size)
{
	l4re_ds_t ds;
	int e;

	if (l4_is_invalid_cap(ds = l4re_util_cap_alloc())) {
		LOG_printf("L4x: Cap alloc failed\n");
		return -L4_ENOMEM;
	}

	if ((e = l4re_ma_alloc(size, ds, L4RE_MA_PINNED))) {
		LOG_printf("L4x: Memory allocation for %ld Bytes failed.\n",
		           size);
		return e;
	}

	if ((e = l4re_rm_attach((void **)&start, size,
	                        L4RE_RM_EAGER_MAP, ds | L4_CAP_FPAGE_RW,
	                        0, L4_PAGESHIFT))) {
		LOG_printf("L4x: Cannot attach memory: %d\n", e);
		return e;
	}

	return 0;
}

static void l4x_fill_up_init_section(void)
{
	/* Due to alignments, the init section may have holes
	 * that are not backed with memory by the ELF file we're loading.
	 * However, the whole init-section is used, thus fill the
	 * hole up with memory. */

	l4_addr_t a = (l4_addr_t)&__init_begin;
	l4_addr_t hole_start = ~0UL;

	while (a < (l4_addr_t)&__init_end) {
		l4re_ds_t ds;
		l4_addr_t off, tmp = a;
		unsigned flags;
		unsigned long size = 1;

		int e = l4re_rm_find(&tmp, &size, &off, &flags, &ds);
		if (e == -L4_ENOENT) {
			if (hole_start == ~0UL)
				hole_start = a;

			size = L4_PAGESIZE;

		} else if (e != 0) {
			LOG_printf("L4x: Failed with %d to query memory "
			           "at %lx\n", e, a);
			break;
		} else if (hole_start != ~0UL)
				l4x_alloc_anon_mem_to(hole_start,
				                      a - hole_start);

		a += size;
	}

	if (hole_start != ~0UL)
		l4x_alloc_anon_mem_to(hole_start, a - hole_start);
}

#ifdef CONFIG_X86_64
static int l4lx_amd64_get_segment_info(void)
{
	int r = fiasco_amd64_segment_info(l4re_env()->main_thread,
	                                  &l4x_x86_fiasco_user_ds,
	                                  &l4x_x86_fiasco_user_cs,
	                                  &l4x_x86_fiasco_user32_cs,
	                                  l4_utcb());
	if (r)
		LOG_printf("L4x: Failed to query segment information (%d).\n",
		           r);

	return r;
}
#endif /* X86_64 */

int __ref L4_CV main(int argc, char **argv)
{
	l4lx_thread_t main_id;
	struct l4lx_thread_start_info_t si;
	extern char boot_command_line[];
	extern initcall_t __initcall_start[];
	unsigned i;
	char *p;

#ifdef CONFIG_L4_SERVER
	l4x_srv_init(&_l4x_svr_ops);
#endif

	LOG_printf("\033[34;1m======> L4Linux starting... <========\033[0m\n");
	LOG_printf("%s", linux_banner);
	LOG_printf("Binary name: %s\n", (*argv)?*argv:"NULL??");
#ifdef CONFIG_ARM
#ifdef CONFIG_AEABI
	LOG_printf("   This is an AEABI build.\n");
#else
	LOG_printf("   This is an OABI build.\n");
#endif
#endif

	if (l4x_re_resolve_name_noctx("jdb", 3, &l4x_jdb_cap))
		l4x_jdb_cap = L4_INVALID_CAP;

	argv++;
	p = boot_command_line;
	while (*argv) {
		i = strlen(*argv);
		if (p - boot_command_line + i >= COMMAND_LINE_SIZE) {
			LOG_printf("Command line too long!");
			enter_kdebug("Command line too long!");
		}
		strcpy(p, *argv);
		p += i;
		if (*++argv)
			*p++ = ' ';
	}
	LOG_printf("Linux kernel command line (%d args): %s\n",
	           argc - 1, boot_command_line);

	/* See if we find a showpfexc=1 or showghost=1 in the command line */
	if ((p = strstr(boot_command_line, "showpfexc=")))
		l4x_debug_show_exceptions = simple_strtoul(p+10, NULL, 0);
	if ((p = strstr(boot_command_line, "showghost=")))
		l4x_debug_show_ghost_regions = simple_strtoul(p+10, NULL, 0);

	if (l4x_check_setup(boot_command_line))
		return 1;

	if (l4x_cpu_virt_phys_map_init(boot_command_line))
		return 1;

	l4x_x86_utcb_save_orig_segment();

	LOG_printf("Image: %08lx - %08lx [%lu KiB].\n",
	           (unsigned long)_text, (unsigned long)_end,
	           (unsigned long)(_end - _text + 1) >> 10);

	print_sect("Areas: Text:    ", &_text, &_etext);
	print_sect("       RO-Data: ", &__start_rodata, &__end_rodata);
	print_sect("       Data:    ", &_sdata, &_edata);
	print_sect("       Init:    ", &__init_begin, &__init_end);
	print_sect("       BSS:     ", &__bss_start, &__bss_stop);

	l4x_fill_up_init_section();

	/* some things from head.S */
	get_initial_cpu_capabilities();

#if defined(CONFIG_X86) && defined(CONFIG_VGA_CONSOLE)
	/* map VGA range */
	if (l4io_request_iomem_region(0xa0000, 0xa0000, 0xc0000 - 0xa0000, 0)) {
		LOG_printf("Failed to map VGA area but CONFIG_VGA_CONSOLE=y.\n");
		return 0;
	}
	if (l4x_pagein(0xa0000, 0xc0000 - 0xa0000, 1))
		LOG_printf("Page-in of VGA failed.\n");
#endif

#ifdef CONFIG_X86_32
	{
		unsigned long gs, fs;
		asm volatile("mov %%gs, %0" : "=r"(gs));
		asm volatile("mov %%fs, %0" : "=r"(fs));
		LOG_printf("gs=%lx   fs=%lx\n", gs, fs);
	}

	asm("mov %%cs, %0 \n"
	    "mov %%ds, %1 \n"
	    : "=r" (l4x_x86_fiasco_user_cs), "=r" (l4x_x86_fiasco_user_ds));
#endif

#ifdef CONFIG_X86_64
	l4lx_amd64_get_segment_info();
#endif

	/* Init v2p list */
	l4x_v2p_init();

	l4lx_task_init();
	l4lx_thread_init();

	l4x_start_thread_id = l4re_env()->main_thread;

	l4_thread_control_start();
	l4_thread_control_commit(l4x_start_thread_id);
	l4x_start_thread_pager_id
		= l4_utcb_mr()->mr[L4_THREAD_CONTROL_MR_IDX_PAGER];

#ifndef CONFIG_L4_VCPU
	l4x_tamed_init();
	l4x_tamed_start(0);
	l4x_repnop_init();
#endif

	l4x_scan_hw_resources();

#ifdef CONFIG_X86
	/* Initialize GDT entry offset */
	l4x_x86_fiasco_gdt_entry_offset
	  = fiasco_gdt_get_entry_offset(l4re_env()->main_thread,
	                                l4_utcb());
	LOG_printf("l4x_x86_fiasco_gdt_entry_offset = %x\n",
	           l4x_x86_fiasco_gdt_entry_offset);

	l4x_v2p_add_item(0xa0000, (void *)0xa0000, 0xfffff - 0xa0000);

	l4x_clock_init();
#endif /* X86 */

#ifdef CONFIG_ARM
	l4x_setup_upage();
#endif
	l4x_setup_dma();

#if defined(CONFIG_X86_64) || defined(CONFIG_ARM)
	setup_module_area();
#endif

#ifdef CONFIG_ARM
	l4x_v2p_add_item(0, NULL, PAGE_SIZE);

	if (l4x_sanity_check_kuser_cmpxchg())
		return 1;
#endif

	/* fire up Linux server, will wait until start message */
	p = (char *)init_stack + sizeof(init_stack);
#ifdef CONFIG_X86_64
	p = (char *)((struct pt_regs *)p - 1);
#endif
	if (l4lx_thread_create(&main_id, cpu0_startup, 0, p,
	                       NULL, 0,
	                       l4re_util_cap_alloc(),
	                       CONFIG_L4_PRIO_SERVER_PROC,
	                       0, &l4x_vcpu_ptr[0],
	                       "cpu0", &si)) {
		LOG_printf("Failed to create vCPU0. Aborting.\n");
		return 1;
	}


	l4x_cpu_thread_set(0, main_id);

	LOG_printf("main thread will be " PRINTF_L4TASK_FORM "\n",
	           PRINTF_L4TASK_ARG(l4lx_thread_get_cap(main_id)));

	l4x_register_range(&__initcall_start, 0, 0, 1, 0,
	                   "section-with-init(-data)");
	l4x_register_range(&_sinittext, 0, 0, 1, 0,
	                   "section-with-init-text");
	l4x_register_range(&_sdata, 0, 0, 1, 0,
	                   "data");

#ifdef CONFIG_X86
	/* Needed for smp alternatives -- nobody will ever use this for a
	 * real phys-addr (hopefully), we cannot use real phys addresses as
	 * we would need to use pinned memory for the text-segment...
	 * Give one page more for text_poke */
	l4x_v2p_add_item((unsigned long)&_text, &_text,
	                 (unsigned long)&_etext - (unsigned long)&_text + PAGE_SIZE);
#endif
	/* Start main thread. */
	l4lx_thread_start(&si);

	LOG_printf("L4x: Main thread running, waiting...\n");

	l4x_server_loop();

	return 0;
}

#ifdef CONFIG_X86

#include <asm/kdebug.h>
#include <asm/syscalls.h>

#define PTREGSCALL0(name)                                       \
long ptregs_##name(void)                                        \
{                                                               \
	struct pt_regs *r = task_pt_regs(current);              \
	return sys_##name(r);                                   \
}

#define PTREGSCALL1(name, t1)                                   \
long ptregs_##name(void)                                        \
{                                                               \
	struct pt_regs *r = task_pt_regs(current);              \
	return sys_##name((t1)r->bx, r);                        \
}

#define PTREGSCALL2(name, t1, t2)                               \
long ptregs_##name(void)                                        \
{                                                               \
	struct pt_regs *r = task_pt_regs(current);              \
	return sys_##name((t1)r->bx, (t2)r->cx, r);             \
}

#define PTREGSCALL3(name, t1, t2, t3)                           \
long ptregs_##name(void)                                        \
{                                                               \
	struct pt_regs *r = task_pt_regs(current)               \
	return sys_##name((t1)r->bx, (t2)r->cx, (t3)r->dx, r);  \
}


#ifdef CONFIG_X86_32
#define RN(n) e##n
#else
#define RN(n) r##n
#endif

static void l4x_setup_die_utcb(l4_exc_regs_t *exc)
{
	struct pt_regs regs;
	unsigned long regs_addr;
	static char message[40];

	snprintf(message, sizeof(message), "Trap: %ld", exc->trapno);
	message[sizeof(message) - 1] = 0;

	memset(&regs, 0, sizeof(regs));
	utcb_exc_to_ptregs(exc, &regs);

	/* XXX: check stack boundaries, i.e. exc->esp & (THREAD_SIZE-1)
	 * >= THREAD_SIZE - sizeof(thread_struct) - sizeof(struct pt_regs)
	 * - ...)
	 */
	/* Copy pt_regs on the stack */
	exc->sp -= sizeof(struct pt_regs);
	*(struct pt_regs *)exc->sp = regs;
	regs_addr = exc->sp;

	/* Fill arguments in regs for die params */
#ifdef CONFIG_X86_32
	exc->ecx = exc->err;
	exc->edx = regs_addr;
	exc->eax = (unsigned long)message;
#else
	exc->rdx = exc->err;
	exc->rsi = regs_addr;
	exc->rdi = (unsigned long)message;
#endif

	exc->sp -= sizeof(unsigned long);
	*(unsigned long *)exc->sp = 0;
	/* Set PC to die function */
	exc->ip = (unsigned long)die;

	LOG_printf("Die message: %s\n", message);
}

asmlinkage static void l4x_do_intra_iret(struct pt_regs regs)
{
#ifdef CONFIG_X86_32
	asm volatile ("mov %%cs, %0" : "=r" (regs.cs));
	asm volatile
	("movl %0, %%esp	\t\n"
	 "popl %%ebx		\t\n"
	 "popl %%ecx		\t\n"
	 "popl %%edx		\t\n"
	 "popl %%esi		\t\n"
	 "popl %%edi		\t\n"
	 "popl %%ebp		\t\n"
	 "popl %%eax		\t\n"
	 "addl $8, %%esp	\t\n" /* keep ds, es */
	 "popl %%fs		\t\n"
	 "popl %%gs		\t\n"
	 "addl $4, %%esp	\t\n" /* keep orig_eax */
	 "iret			\t\n"
	 : : "r" (&regs));
#endif

	panic("Intra game zombie walking!");
}

static void l4x_setup_stack_for_traps(l4_exc_regs_t *exc, struct pt_regs *regs,
                                      void (*trap_func)(struct pt_regs *regs, long err))
{
	const int l4x_intra_regs_size
		= sizeof(struct pt_regs) - 2 * sizeof(unsigned long);

	/* Copy pt_regs on the stack but omit last to two dwords,
	 * an intra-priv exception/iret doesn't have those, and native
	 * seems to do it the same, with the hope that nobody touches
	 * there in the pt_regs */
	exc->sp -= l4x_intra_regs_size;
	memcpy((void *)exc->sp, regs, l4x_intra_regs_size);

	/* do_<exception> functions are regparm(3), arguments go in regs */
#ifdef CONFIG_X86_32
	exc->eax = exc->sp;
	exc->edx = exc->err;
#else
	exc->rdi = exc->sp;
	exc->rsi = exc->err;
#endif

	/* clear TF */
	exc->flags &= ~256;

	exc->sp -= sizeof(unsigned long);
	*(unsigned long *)exc->sp = 0; /* Return of l4x_do_intra_iret */
	exc->sp -= sizeof(unsigned long);
	*(unsigned long *)exc->sp = (unsigned long)l4x_do_intra_iret;

	/* Set PC to trap function */
	exc->ip = (unsigned long)trap_func;
}

static int l4x_handle_hlt_for_bugs_test(l4_exc_regs_t *exc)
{
	void *pc = (void *)l4_utcb_exc_pc(exc);

	if (*(unsigned int *)pc == 0xf4f4f4f4) {
		// check_bugs does 4 times hlt
		LOG_printf("Jumping over 4x 'hlt' at 0x%lx\n",
		           (unsigned long)pc);
		exc->ip += 4;
		return 0; // handled
	}

	return 1; // not for us
}

static int l4x_handle_hlt(l4_exc_regs_t *exc)
{
	extern void do_int3(struct pt_regs *regs, long err);
	struct pt_regs regs;

#ifdef CONFIG_KPROBES
	BUILD_BUG_ON(BREAKPOINT_INSTRUCTION != 0xf4);
#endif

	if (l4_utcb_exc_pc(exc) < PAGE_SIZE)
		return 1; /* Can not handle */

	/* check for kprobes break instruction */
	if (*(unsigned char *)l4_utcb_exc_pc(exc) != 0xf4)
		return 1; /* Not handled */

	utcb_exc_to_ptregs(exc, &regs);
	l4x_set_kernel_mode(&regs);

	/* Set after breakpoint instruction as for HLT pc is on the
	 * instruction and for INT3 after the instruction */
	regs.ip++;

	l4x_setup_stack_for_traps(exc, &regs, do_int3);
	return 0;
}

static int l4x_handle_int1(l4_exc_regs_t *exc)
{
	struct pt_regs regs;
	extern void do_debug(struct pt_regs *regs, long err);

	utcb_exc_to_ptregs(exc, &regs);
	l4x_setup_stack_for_traps(exc, &regs, do_debug);
	return 0;
}

static int l4x_handle_clisti(l4_exc_regs_t *exc)
{
	unsigned char opcode = *(unsigned char *)l4_utcb_exc_pc(exc);
	extern void exit(int);

	/* check for cli or sti instruction */
	if (opcode != 0xfa && opcode != 0xfb)
		return 1; /* not handled if not those instructions */

	/* If we trap those instructions it's most likely a configuration
	 * error and quite early in the boot-up phase, so just quit. */
	LOG_printf("Aborting L4Linux due to unexpected CLI/STI instructions"
	           " at %lx.\n", l4_utcb_exc_pc(exc));
	enter_kdebug("abort");
	exit(0);

	return 0;
}

static unsigned long l4x_handle_ioport_pending_pc;

static int l4x_handle_ioport(l4_exc_regs_t *exc)
{
	if (l4_utcb_exc_pc(exc) == l4x_handle_ioport_pending_pc) {
		l4x_setup_die_utcb(exc);
		return 0;
	}
	return 1;
}

#include <asm/perf_event.h>

static int l4x_handle_msr(l4_exc_regs_t *exc)
{
	void *pc = (void *)l4_utcb_exc_pc(exc);
	unsigned long reg = exc->RN(cx);

	/* wrmsr */
	if (*(unsigned short *)pc == 0x300f) {
		static unsigned long list[] = {
			MSR_IA32_SYSENTER_CS,
			MSR_IA32_SYSENTER_ESP,
			MSR_IA32_SYSENTER_EIP,
			MSR_AMD64_MCx_MASK(4),
			MSR_IA32_UCODE_REV,
		};
		int i = 0;

		for (; i < ARRAY_SIZE(list); ++i)
			if (reg == list[i])
				break;

		if (i == ARRAY_SIZE(list))
			LOG_printf("WARNING: Unknown wrmsr: %08lx at %p\n", reg, pc);

		exc->ip += 2;
		return 0; // handled
	}

	/* rdmsr */
	if (*(unsigned short *)pc == 0x320f) {

		if ((reg & 0xfffffff0) == 0xc0010000) {
			exc->RN(ax) = exc->RN(dx) = 0;
			exc->ip += 2;
			return 0; // handled
		}

		switch (reg) {
			case MSR_IA32_MISC_ENABLE:
			case MSR_K8_TSEG_ADDR:
			case MSR_AMD64_MCx_MASK(4):
			case MSR_AMD64_PATCH_LEVEL: /* == MSR_IA32_UCODE_REV */
			case MSR_ARCH_PERFMON_FIXED_CTR_CTRL:
			case MSR_K7_PERFCTR0:
			case MSR_P6_PERFCTR0:
			case MSR_P4_BPU_PERFCTR0:
			case MSR_F15H_PERF_CTR:
			case MSR_CORE_C3_RESIDENCY:
			case MSR_CORE_C6_RESIDENCY:
			case MSR_CORE_C7_RESIDENCY:
			case MSR_PKG_C2_RESIDENCY:
			case MSR_PKG_C6_RESIDENCY:
			case MSR_PKG_C7_RESIDENCY:
			case MSR_IA32_APERF:
			case MSR_IA32_MPERF:
			case MSR_SMI_COUNT:
			case MSR_PPERF:
			case MSR_IA32_VMX_PROCBASED_CTLS:
			case MSR_RAPL_POWER_UNIT:
				exc->RN(ax) = exc->RN(dx) = 0;
				break;
			case MSR_K7_CLK_CTL:
				exc->RN(ax) = 0x20000000;
				break;
			case MSR_AMD64_OSVW_ID_LENGTH:
			case MSR_AMD64_OSVW_STATUS:
				exc->RN(ax) = 0;
				break;
			case MSR_EFER:
				exc->RN(ax) = 0;
				break;
			default:
				LOG_printf("WARNING: Unknown rdmsr: "
				           "%08lx at %p\n", reg, pc);
		};

		exc->ip += 2;
		return 0; // handled
	}

	return 1; // not for us
}

static inline void l4x_print_exception(l4_cap_idx_t t, l4_exc_regs_t *exc)
{
	LOG_printf("EX: "l4util_idfmt": "
	           "pc = "l4_addr_fmt" "
	           "sp = "l4_addr_fmt" "
	           "trapno = 0x%lx err/pfa = 0x%lx%s\n",
	           l4util_idstr(t), exc->ip, exc->sp,
	           exc->trapno,
	           exc->trapno == 14 ? exc->pfa : exc->err,
	           exc->trapno == 14 ? (exc->err & 2) ? " w" : " r" : "");

	if (l4x_debug_show_exceptions >= 2
	    && !l4_utcb_exc_is_pf(exc)) {
		/* Lets assume we can do the following... */
		unsigned len = 72, i;
		unsigned long ip = exc->ip - 43;

		LOG_printf("Dump: ");
		for (i = 0; i < len; i++, ip++)
			if (ip == exc->ip)
				LOG_printf("<%02x> ", *(unsigned char *)ip);
			else
				LOG_printf("%02x ", *(unsigned char *)ip);

		LOG_printf(".\n");
	}
}

#ifdef CONFIG_L4_VCPU
static inline l4_exc_regs_t *cast_to_utcb_exc(l4_vcpu_regs_t *vcpu_regs)
{
	BUILD_BUG_ON(offsetof(l4_vcpu_regs_t, ds)      != offsetof(l4_exc_regs_t, ds));
	BUILD_BUG_ON(offsetof(l4_vcpu_regs_t, es)      != offsetof(l4_exc_regs_t, es));
	BUILD_BUG_ON(offsetof(l4_vcpu_regs_t, gs)      != offsetof(l4_exc_regs_t, gs));
	BUILD_BUG_ON(offsetof(l4_vcpu_regs_t, fs)      != offsetof(l4_exc_regs_t, fs));
#ifdef CONFIG_X86_64
	BUILD_BUG_ON(offsetof(l4_vcpu_regs_t, fs_base) != offsetof(l4_exc_regs_t, fs_base));
	BUILD_BUG_ON(offsetof(l4_vcpu_regs_t, gs_base) != offsetof(l4_exc_regs_t, gs_base));
	BUILD_BUG_ON(offsetof(l4_vcpu_regs_t, r15)     != offsetof(l4_exc_regs_t, r15));
	BUILD_BUG_ON(offsetof(l4_vcpu_regs_t, r14)     != offsetof(l4_exc_regs_t, r14));
	BUILD_BUG_ON(offsetof(l4_vcpu_regs_t, r13)     != offsetof(l4_exc_regs_t, r13));
	BUILD_BUG_ON(offsetof(l4_vcpu_regs_t, r12)     != offsetof(l4_exc_regs_t, r12));
	BUILD_BUG_ON(offsetof(l4_vcpu_regs_t, r11)     != offsetof(l4_exc_regs_t, r11));
	BUILD_BUG_ON(offsetof(l4_vcpu_regs_t, r10)     != offsetof(l4_exc_regs_t, r10));
	BUILD_BUG_ON(offsetof(l4_vcpu_regs_t, r9)      != offsetof(l4_exc_regs_t, r9));
	BUILD_BUG_ON(offsetof(l4_vcpu_regs_t, r8)      != offsetof(l4_exc_regs_t, r8));
#endif
	BUILD_BUG_ON(offsetof(l4_vcpu_regs_t, di)      != offsetof(l4_exc_regs_t, RN(di)));
	BUILD_BUG_ON(offsetof(l4_vcpu_regs_t, si)      != offsetof(l4_exc_regs_t, RN(si)));
	BUILD_BUG_ON(offsetof(l4_vcpu_regs_t, bp)      != offsetof(l4_exc_regs_t, RN(bp)));
	BUILD_BUG_ON(offsetof(l4_vcpu_regs_t, pfa)     != offsetof(l4_exc_regs_t, pfa));
	BUILD_BUG_ON(offsetof(l4_vcpu_regs_t, bx)      != offsetof(l4_exc_regs_t, RN(bx)));
	BUILD_BUG_ON(offsetof(l4_vcpu_regs_t, dx)      != offsetof(l4_exc_regs_t, RN(dx)));
	BUILD_BUG_ON(offsetof(l4_vcpu_regs_t, cx)      != offsetof(l4_exc_regs_t, RN(cx)));
	BUILD_BUG_ON(offsetof(l4_vcpu_regs_t, ax)      != offsetof(l4_exc_regs_t, RN(ax)));
	BUILD_BUG_ON(offsetof(l4_vcpu_regs_t, trapno)  != offsetof(l4_exc_regs_t, trapno));
	BUILD_BUG_ON(offsetof(l4_vcpu_regs_t, err)     != offsetof(l4_exc_regs_t, err));
	BUILD_BUG_ON(offsetof(l4_vcpu_regs_t, ip)      != offsetof(l4_exc_regs_t, ip));
	BUILD_BUG_ON(offsetof(l4_vcpu_regs_t, flags)   != offsetof(l4_exc_regs_t, flags));
	BUILD_BUG_ON(offsetof(l4_vcpu_regs_t, sp)      != offsetof(l4_exc_regs_t, sp));

	return (l4_exc_regs_t *)vcpu_regs;
}
#endif

#endif /* X86 */

#ifdef CONFIG_ARM

#include <asm/bug.h>

/* Redefinition fix */
#undef VM_EXEC
#include <asm/asm-offsets.h>

void l4x_arm_ret_from_exc(void);
asm(
".global l4x_arm_ret_from_exc                        \n"
"l4x_arm_ret_from_exc:                               \n"
"	ldr	r0, [sp, #" __stringify(S_PSR) "]    \n"
"	msr	cpsr_f, r0                           \n"
"	ldr	r0, [sp, #" __stringify(S_PC) "]     \n"
"	str	r0, [sp, #" __stringify(S_OLD_R0) "] \n"
"	ldr	lr, [sp, #" __stringify(S_LR) "]     \n"
"	ldmia	sp!, {r0 - r12}                      \n"
"	add	sp, sp, #(4 * 4)                     \n"
"	pop	{pc}                                 \n"
);

static void l4x_setup_die_utcb(l4_exc_regs_t *exc)
{
	struct pt_regs regs;
	static char message[40];

	snprintf(message, sizeof(message), "Boom!");
	message[sizeof(message) - 1] = 0;

	utcb_exc_to_ptregs(exc, &regs);
	regs.ARM_ORIG_r0 = 0;
	l4x_set_kernel_mode(&regs);

	/* Copy pt_regs on the stack */
	exc->sp -= sizeof(struct pt_regs);
	*(struct pt_regs *)exc->sp = regs;

	/* Put arguments for die into registers */
	exc->r[0] = (unsigned long)message;
	exc->r[1] = exc->sp;
	exc->r[2] = exc->err;

	/* Set PC to die function */
	exc->pc  = (unsigned long)die;
	exc->ulr = (unsigned long)l4x_arm_ret_from_exc;
}

static void l4x_setup_stack_for_traps(l4_exc_regs_t *exc,
                                      void (*trap_func)(struct pt_regs *regs))
{
	struct pt_regs *regs;

	exc->sp -= sizeof(*regs);

	regs = (struct pt_regs *)exc->sp;

	utcb_exc_to_ptregs(exc, regs);
	regs->ARM_ORIG_r0 = 0;
	l4x_set_kernel_mode(regs);

	exc->r[0] = (unsigned long)regs;
	exc->pc   = (unsigned long)trap_func;
	exc->ulr  = (unsigned long)l4x_arm_ret_from_exc;
}

static void l4x_arm_set_reg(l4_exc_regs_t *exc, int num, unsigned long val)
{
	if (num > 15) {
		LOG_printf("Invalid register: %d\n", num);
		return;
	}

	switch (num) {
		case 15: exc->pc = val;  break;
		case 14: exc->ulr = val; break;
		default: exc->r[num] = val;
	}
}

#ifdef CONFIG_VFP
#include <asm/generic/fpu.h>
#include <asm/vfp.h>
#define  __L4X_ARM_VFP_H__USE_REAL_FUNCS
#include "../../arm/vfp/vfpinstr.h"

#include <asm/l4x/fpu.h>

unsigned l4x_fmrx(unsigned long reg)
{
	switch (reg) {
		case FPEXC:   return l4x_fpu_get(smp_processor_id())->fpexc;
		case FPSCR:   return fmrx(cr1);
		case FPSID:   return fmrx(cr0);
		case MVFR0:   return fmrx(cr7);
		case MVFR1:   return fmrx(cr6);
		case FPINST:  return l4x_fpu_get(smp_processor_id())->fpinst;
		case FPINST2: return l4x_fpu_get(smp_processor_id())->fpinst2;
		default: printk("Invalid fmrx-reg: %ld\n", reg);
	}
	return 0;
}

void l4x_fmxr(unsigned long reg, unsigned long val)
{
	switch (reg) {
		case FPEXC:
			l4x_fpu_get(smp_processor_id())->fpexc = val;
			return;
		case FPSCR:
			fmxr(cr1, val);
			return;
		default: printk("Invalid fmxr-reg: %ld\n", reg);
	}
	return;
}
#endif

static int l4x_handle_arm_undef(l4_exc_regs_t *exc)
{
	asmlinkage void do_undefinstr(struct pt_regs *regs);

	unsigned long pc = exc->pc - 4;
	unsigned long op;

	if ((exc->err >> 26) != 0)
		return 1;

	BUG_ON(exc->cpsr & PSR_T_BIT);

	if (pc < (unsigned long)&_stext || pc > (unsigned long)&_etext)
		return 1; // not for us

	op = *(unsigned long *)pc;

	/* We should/could move that over to an undef-hook */
	if ((op & 0xff000000) == 0xee000000) {
		// always, mrc, mcr
		unsigned int reg;

		op &= 0x00ffffff;
		reg = (op >> 12) & 0xf;
		// see also cputype.h!
		if ((op & 0x00ff0fff) == 0x00100f30) {
			LOG_printf("Read Cache Type Register, to r%d\n", reg);
			// 32kb i/d cache
			l4x_arm_set_reg(exc, reg, read_cpuid_cachetype());
			return 0;
		} else if ((op & 0x00ff0fff) == 0x100f10) {
			// mrc     15, 0, xx, cr0, cr0, {0}
			LOG_printf("Read ID code register, to r%d\n", reg);
			l4x_arm_set_reg(exc, reg, read_cpuid_id());
			return 0;
		} else if ((op & 0x00ff0fff) == 0x00100f91) {
			// mrc     15, 0, xx, cr0, cr1, {4}
			LOG_printf("Read memory model feature reg0 to r%d\n", reg);
			l4x_arm_set_reg(exc, reg, 0);
			return 0;
		} else if ((op & 0x00ff0fff) == 0x00400f10) {
			// mcr     15, 2, xx, cr0, cr0, 0
			LOG_printf("Write of CSSELR\n");
			return 0;
		} else if ((op & 0x00ff0fff) == 0x00300f10) {
			// mrc     15, 1, xx, cr0, cr0, {0}
			LOG_printf("Read of CCSIDR to r%d\n", reg);
			l4x_arm_set_reg(exc, reg, 0);
			return 0;
		} else if (   (op & 0x00000f00) == 0xa00
		           || (op & 0x00000f00) == 0xb00) {
			LOG_printf("Copro10/11 access (FPU), invalid, will oops\n");
			return 1; // not for use, will oops
		}
		LOG_printf("Unknown MRC: %lx at %lx\n", op, pc);
	} else if ((op & 0x0fb00ff0) == 0x01000090)
		LOG_printf("'swp(b)' instruction at %08lx and faulting.\n"
		           "Linux built for the wrong ARM version?\n", pc);

	exc->pc = pc;
	l4x_setup_stack_for_traps(exc, do_undefinstr);
	return 0;
}

static inline void l4x_print_exception(l4_cap_idx_t t, l4_exc_regs_t *exc)
{
	LOG_printf("EX: "l4util_idfmt": "
	           "pc="l4_addr_fmt" "
	           "sp="l4_addr_fmt" "
	           "err=0x%lx lr=%lx\n",
	           l4util_idstr(t), exc->pc, exc->sp, exc->err, exc->ulr);

	if (l4x_debug_show_exceptions >= 2
	    && !l4_utcb_exc_is_pf(exc)
	    && (exc->pc & 3) == 0) {
		/* Lets assume we can do the following... */
		unsigned len = 72 >> 2, i;
		unsigned long pc = exc->pc - 44;

		LOG_printf("Dump: ");
		for (i = 0; i < len; i++, pc += sizeof(unsigned long))
			if (pc == exc->pc)
				LOG_printf("<%08lx> ", *(unsigned long *)pc);
			else
				LOG_printf("%08lx ", *(unsigned long *)pc);

		LOG_printf(".\n");
	}
}

#ifdef CONFIG_L4_VCPU
static inline l4_exc_regs_t *cast_to_utcb_exc(l4_vcpu_regs_t *vcpu_regs)
{
	BUILD_BUG_ON(offsetof(l4_vcpu_regs_t, pfa)   != offsetof(l4_exc_regs_t, pfa));
	BUILD_BUG_ON(offsetof(l4_vcpu_regs_t, err)   != offsetof(l4_exc_regs_t, err));
	BUILD_BUG_ON(offsetof(l4_vcpu_regs_t, r)     != offsetof(l4_exc_regs_t, r));
	BUILD_BUG_ON(offsetof(l4_vcpu_regs_t, sp)    != offsetof(l4_exc_regs_t, sp));
	BUILD_BUG_ON(offsetof(l4_vcpu_regs_t, lr)    != offsetof(l4_exc_regs_t, ulr));
	BUILD_BUG_ON(offsetof(l4_vcpu_regs_t, ip)    != offsetof(l4_exc_regs_t, pc));
	BUILD_BUG_ON(offsetof(l4_vcpu_regs_t, flags) != offsetof(l4_exc_regs_t, cpsr));

	return (l4_exc_regs_t *)(vcpu_regs);
}
#endif

#endif /* ARM */

struct l4x_exception_func_struct {
	u64   trap_mask;
	int   for_vcpu;
	int   (*f)(l4_exc_regs_t *exc);
};
static struct l4x_exception_func_struct l4x_exception_func_list[] = {
#ifdef CONFIG_X86
	{ .trap_mask = 0x2000, .for_vcpu = 1, .f = l4x_handle_hlt_for_bugs_test }, // before kprobes!
	{ .trap_mask = 0x2000, .for_vcpu = 1, .f = l4x_handle_hlt },
	{ .trap_mask = 0x0002, .for_vcpu = 0, .f = l4x_handle_int1 },
	{ .trap_mask = 0x2000, .for_vcpu = 1, .f = l4x_handle_msr },
	{ .trap_mask = 0x2000, .for_vcpu = 1, .f = l4x_handle_clisti },
	{ .trap_mask = 0x4000, .for_vcpu = 0, .f = l4x_handle_ioport },
	{ .trap_mask = 0x2000, .for_vcpu = 1, .f = l4x_handle_ioport },
#endif
#ifdef CONFIG_ARM
	{ .trap_mask = 1,   .for_vcpu = 1, .f = l4x_handle_arm_undef },
#endif
};
static const int l4x_exception_funcs = ARRAY_SIZE(l4x_exception_func_list);

static inline int l4x_handle_pagefault(unsigned long pfa, unsigned long ip,
                                       int rw)
{
	if (l4x_check_kern_region_noctx((void *)pfa, 1, rw)) {
		/* Not resolvable: Ooops */
		LOG_printf("Non-resolvable page fault at %lx, ip %lx.\n", pfa, ip);
		// will trigger an oops in caller
		return 0;
	}

	/* Forward PF to our pager */
	return l4x_forward_pf(pfa, ip, rw);
}

#ifdef CONFIG_L4_VCPU
// return true when exception handled successfully, false if not
int l4x_vcpu_handle_kernel_exc(l4_vcpu_regs_t *vr)
{
	int i;
	l4_exc_regs_t *exc = cast_to_utcb_exc(vr);

	// check handlers for this exception
	for (i = 0; i < l4x_exception_funcs; i++) {
		struct l4x_exception_func_struct *f
		  = &l4x_exception_func_list[i];
		if (f->for_vcpu
	            && ((1 << l4_utcb_exc_typeval(exc)) & f->trap_mask)
		    && !f->f(exc))
			break;
	}
	return i != l4x_exception_funcs;
}
#endif

static int l4x_default(l4_cap_idx_t *src_id, l4_msgtag_t *tag)
{
	l4_exc_regs_t exc;
	l4_umword_t pfa;
	l4_umword_t pc;

	pfa = l4_utcb_mr()->mr[0];
	pc  = l4_utcb_mr()->mr[1];

	memcpy(&exc, l4_utcb_exc(), sizeof(exc));

#if 0
	if (!l4_msgtag_is_exception(*tag)
	    && !l4_msgtag_is_io_page_fault(*tag)) {
		static unsigned long old_pf_addr = ~0UL, old_pf_pc = ~0UL;
		if (unlikely(old_pf_addr == (pfa & ~1) && old_pf_pc == pc)) {
			LOG_printf("Double page fault pfa=%08lx pc=%08lx\n",
			           pfa, pc);
			enter_kdebug("Double pagefault");
		}
		old_pf_addr = pfa & ~1;
		old_pf_pc   = pc;
	}
#endif

	if (l4_msgtag_is_exception(*tag)) {
		int i;

		if (l4x_debug_show_exceptions)
			l4x_print_exception(*src_id, &exc);

		// check handlers for this exception
		for (i = 0; i < l4x_exception_funcs; i++)
			if (((1 << l4_utcb_exc_typeval(&exc)) & l4x_exception_func_list[i].trap_mask)
			    && !l4x_exception_func_list[i].f(&exc))
				break;

		// no handler wanted to handle this exception
		if (i == l4x_exception_funcs) {
			if (l4_utcb_exc_is_pf(&exc)
			    && l4x_handle_pagefault(l4_utcb_exc_pfa(&exc),
			                            l4_utcb_exc_pc(&exc), 0)) {
				*tag = l4_msgtag(0, 0, 0, 0);
				pfa = pc = 0;
				return 0; // reply
			}

			l4x_setup_die_utcb(&exc);
		}

		*tag = l4_msgtag(0, L4_UTCB_EXCEPTION_REGS_SIZE, 0, 0);
		memcpy(l4_utcb_exc(), &exc, sizeof(exc));
		return 0; // reply

#ifdef CONFIG_X86
	} else if (l4_msgtag_is_io_page_fault(*tag)) {
		LOG_printf("Invalid IO-Port access at pc = "l4_addr_fmt
		           " port=0x%lx\n", pc, pfa >> 12);

		l4x_handle_ioport_pending_pc = pc;

		/* Make exception out of PF */
		*tag = l4_msgtag(-1, 1, 0, 0);
		l4_utcb_mr()->mr[0] = -1;
		return 0; // reply
#endif
	} else if (l4_msgtag_is_page_fault(*tag)) {
		if (l4x_debug_show_exceptions)
			LOG_printf("Page fault: addr = " l4_addr_fmt
			           " pc = " l4_addr_fmt " (%s%s)\n",
			           pfa, pc,
			           pfa & 2 ? "write" : "read",
			           pfa & 1 ? ", T" : "");

		if (l4x_handle_pagefault(pfa, pc, !!(pfa & 2))) {
			*tag = l4_msgtag(0, 0, 0, 0);
			return 0;
		}

		LOG_printf("Page fault (non-resolved): pfa=%lx pc=%lx\n", pfa, pc);
		/* Make exception out of PF */
		*tag = l4_msgtag(-1, 1, 0, 0);
		l4_utcb_mr()->mr[0] = -1;
		return 0; // reply
	}

	l4x_print_exception(*src_id, &exc);

	return 1; // no reply
}

enum {
	L4X_SERVER_EXIT = 1,
};

static void __noreturn l4x_server_loop(void)
{
	int do_wait = 1;
	l4_msgtag_t tag = (l4_msgtag_t){0};
	l4_umword_t src = 0;

	while (1) {

		while (do_wait)
			do_wait = l4_msgtag_has_error(tag = l4_ipc_wait(l4_utcb(), &src, L4_IPC_NEVER));

		switch (l4_msgtag_label(tag)) {
			case L4X_SERVER_EXIT:
				l4x_linux_main_exit(); // will not return anyway
				do_wait = 1;
				break;
			default:
				if (l4x_default(&src, &tag)) {
					do_wait = 1;
					continue; // do not reply
				}
		}

		tag = l4_ipc_reply_and_wait(l4_utcb(), tag,
		                            &src, L4_IPC_SEND_TIMEOUT_0);
		do_wait = l4_msgtag_has_error(tag);
	}
}

/**
 * Shutdown or reboot the platform
 *
 * @param reboot  If 0 shutdown the platform, restart otherwise
 *
 * This function looks for the 'pfc' platform control capability. If one is
 * found the corresponding shutdown/reboot action is triggered. If something
 * fails this function simply exits the L4Linux task.
 */
void l4x_platform_shutdown(unsigned reboot)
{
	l4_cap_idx_t pfc_cap;
	long e;

	/* Look for platform control capability 'pfc' */
	e = l4x_re_resolve_name("pfc", &pfc_cap);
	if (e || !l4_is_valid_cap(pfc_cap)) {
		pr_debug("No platform control cap 'pfc' found, "
		         "will not shutdown/restart platform.\n");
		l4x_exit_l4linux();
	}

	e = L4XV_FN_e(l4_platform_ctl_system_shutdown(pfc_cap, reboot));
	if (e) {
		pr_err("Error during shutdown/restart of the platform. "
		       "Exiting L4Linux.\n");
		l4x_exit_l4linux();
	}

	/* wait till the platform is powered down / restarted */
	l4_sleep_forever();
}

void __noreturn l4x_exit_l4linux(void)
{
	__cxa_finalize(0);

	if (l4_msgtag_has_error(l4_ipc_send(l4x_start_thread_id, l4_utcb(),
	                                    l4_msgtag(L4X_SERVER_EXIT, 0, 0, 0),
	                                    L4_IPC_NEVER)))
		LOG_printf("IPC ERROR l4x_exit_l4linux\n");
	l4_sleep_forever();
}

void __noreturn l4x_exit_l4linux_msg(const char *fmt, ...)
{
	va_list list;

	va_start(list, fmt);
	LOG_vprintf(fmt, list);
	va_end(list);
	l4x_exit_l4linux();
}


#ifdef CONFIG_PM
/* ---------------------------------------------------------------- */
/* swsusp stuff */
int arch_prepare_suspend(void)
{
	LOG_printf("%s\n", __func__);
	return 0;
}

void l4x_swsusp_before_resume(void)
{
	// make our AS readonly so that we see all PFs
#if 0
	l4_fpage_unmap(l4_fpage(0, 31, 0, 0),
	               L4_FP_REMAP_PAGE | L4_FP_ALL_SPACES);
#endif
}

void l4x_swsusp_after_resume(void)
{
	LOG_printf("%s\n", __func__);
}

#endif /* CONFIG_PM */


void exit(int code)
{
	__cxa_finalize(0);
	l4x_external_exit(code);
	LOG_printf("Still alive, going zombie???\n");
	l4_sleep_forever();
}

/* -------------------------------------------------- */

#ifdef CONFIG_ARM
int l4x_peek_upage(unsigned long addr,
                   unsigned long __user *datap,
                   int *ret)
{
	unsigned long tmp;

	if (addr >= UPAGE_USER_ADDRESS
	    && addr < UPAGE_USER_ADDRESS + PAGE_SIZE) {
		addr -= UPAGE_USER_ADDRESS;
		tmp = *(unsigned long *)(addr + upage_addr);
		*ret = put_user(tmp, datap);
		return 1;
	}
	return 0;
}
#endif

/* ----------------------------------------------------- */

void l4x_printk_func(char *buf, int len)
{
	outnstring(buf, len);
}

#ifndef CONFIG_L4_VCPU
#include <linux/hardirq.h>

/* ----------------------------------------------------- */
/* Prepare a thread for use as an IRQ thread. */
void l4x_prepare_irq_thread(unsigned long sp, unsigned _cpu)
{
	/* Stack setup */
#ifndef CONFIG_THREAD_INFO_IN_TASK
	struct thread_info *ti;
	ti = (struct thread_info *)(sp & ~(THREAD_SIZE - 1));
	*ti = (struct thread_info) INIT_THREAD_INFO(init_task);
#endif

	l4x_stack_set(sp, l4_utcb());
	barrier();

	/* Set pager */
	l4lx_thread_set_kernel_pager(l4x_cap_current_utcb(l4_utcb()));

#if defined(CONFIG_SMP) && defined(CONFIG_X86)
	l4x_load_percpu_gdt_descriptor(get_cpu_gdt_rw(_cpu));
#endif
#ifdef CONFIG_ARM
	set_my_cpu_offset(per_cpu_offset(_cpu));
#endif
}
#endif

/* ----------------------------------------------------- */
void l4x_show_process(struct task_struct *t)
{
#ifdef CONFIG_L4_VCPU
#ifdef CONFIG_X86
	printk("%2d: %s tsk st: %lx thrd flgs: %lx esp: %08lx\n",
	       t->pid, t->comm, t->state, task_thread_info(t)->flags,
	       t->thread.sp);
#endif
#ifdef CONFIG_ARM
	printk("%2d: %s tsk st: %lx thrd flgs: %lx esp: %08x\n",
	       t->pid, t->comm, t->state, task_thread_info(t)->flags,
	       task_thread_info(t)->cpu_context.sp);
#endif
#else
#ifdef CONFIG_X86
	printk("%2d: %s tsk st: %lx thrd flgs: %lx " PRINTF_L4TASK_FORM " esp: %08lx\n",
	       t->pid, t->comm, t->state, task_thread_info(t)->flags,
	       PRINTF_L4TASK_ARG(t->thread.l4x.user_thread_id),
	       t->thread.sp);
#endif
#ifdef CONFIG_ARM
	printk("%2d: %s tsk st: %lx thrd flgs: %lx " PRINTF_L4TASK_FORM " esp: %08x\n",
	       t->pid, t->comm, t->state, task_thread_info(t)->flags,
	       PRINTF_L4TASK_ARG(t->thread.l4x.user_thread_id),
	       task_thread_info(t)->cpu_context.sp);
#endif
#endif
}

void l4x_show_processes(void)
{
	struct task_struct *t;
	for_each_process(t) {
		if (t->pid >= 10)
			l4x_show_process(t);
	}
	printk("c");
	l4x_show_process(current);
}

void l4x_show_sigpending_processes(void)
{
	struct task_struct *t;
	printk("Processes with pending signals:\n");
	for_each_process(t) {
		if (signal_pending(t))
			l4x_show_process(t);
	}
	printk("Signal list done.\n");

}

/* Just a function we can call without defining the header files */
void kdb_ke(void)
{
	enter_kdebug("kdb_ke");
}

#ifdef CONFIG_L4_DEBUG_SEGFAULTS
#include <linux/fs.h>
void l4x_print_vm_area_maps(struct task_struct *p, unsigned long highlight)
{
	struct vm_area_struct *vma = p->mm->mmap;

	while (vma) {
		struct file *file = vma->vm_file;
		int flags = vma->vm_flags;

		printk("%08lx - %08lx %c%c%c%c",
		       vma->vm_start, vma->vm_end,
		       flags & VM_READ ? 'r' : '-',
		       flags & VM_WRITE ?  'w' : '-',
		       flags & VM_EXEC ?  'x' : '-',
		       flags & VM_MAYSHARE ?  's' : 'p');

		if (file) {
			char buf[40];
			int count = 0;
			char *s = buf;
			char *p = d_path(&file->f_path, s, sizeof(buf));

			printk(" %05lx", vma->vm_pgoff);

			if (!IS_ERR(p)) {
				while (s <= p) {
					char c = *p++;
					if (!c) {
						p = buf + count;
						count = s - buf;
						buf[count] = 0;
						break;
					} else if (!strchr("", c)) {
						*s++ = c;
					} else if (s + 4 > p) {
						break;
					} else {
						*s++ = '\\';
						*s++ = '0' + ((c & 0300) >> 6);
						*s++ = '0' + ((c & 070) >> 3);
						*s++ = '0' + (c & 07);
					}
				}
			}
			printk(" %s", count ? buf : "Unknown");
		}

		if (   highlight >= vma->vm_start
		    && highlight < vma->vm_end)
			printk(" <==== (0x%lx)", highlight - vma->vm_start);
		printk("\n");
		vma = vma->vm_next;
	}
}
#endif

#ifdef CONFIG_X86

#include <linux/clockchips.h>

struct clock_event_device *global_clock_event;

static int clock_init_l4_timer_shutdown(struct clock_event_device *evt)
{
	return 0;
}

static int clock_l4_next_event(unsigned long delta, struct clock_event_device *evt)
{
	return 0;
}

struct clock_event_device l4_clockevent = {
	.name			= "l4",
	.features		= CLOCK_EVT_FEAT_PERIODIC,
	.set_state_shutdown	= clock_init_l4_timer_shutdown,
	.set_next_event		= clock_l4_next_event,
	.shift			= 32,
	.irq			= 0,
	.mult			= 1,
};

void setup_pit_timer(void)
{
	l4_clockevent.cpumask = cpumask_of(0);
	clockevents_register_device(&l4_clockevent);
	global_clock_event = &l4_clockevent;
}
#endif

/* ----------------------------------------------------------------------- */
/* Export list, we could also put these in a separate file (like l4_ksyms.c) */

EXPORT_SYMBOL(l4x_vmalloc_memory_start);

char l4x_ipc_errstrings[0];
EXPORT_SYMBOL(l4x_ipc_errstrings);

#ifndef CONFIG_L4_VCPU
EXPORT_SYMBOL(l4x_prepare_irq_thread);
#endif

EXPORT_SYMBOL(l4lx_task_create);
EXPORT_SYMBOL(l4lx_task_get_new_task);
EXPORT_SYMBOL(l4lx_task_number_free);
EXPORT_SYMBOL(l4lx_task_delete_task);
EXPORT_SYMBOL(l4re_global_env);
EXPORT_SYMBOL(l4_utcb_wrap);
#ifdef CONFIG_X86_32
extern char __l4sys_invoke_direct[];
EXPORT_SYMBOL(__l4sys_invoke_direct);

#ifdef CONFIG_FUNCTION_TRACER
/* mcount is defined in assembly */
EXPORT_SYMBOL(mcount);
#endif
#endif /* IA32 */

#ifdef CONFIG_X86
#ifdef CONFIG_PREEMPT
EXPORT_SYMBOL(___preempt_schedule);
#ifdef CONFIG_CONTEXT_TRACKING
EXPORT_SYMBOL(___preempt_schedule_context);
#endif
#endif
#endif /* X86 */
