#include <sys/mman.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <elf.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#ifndef __FreeBSD__
#include <endian.h>
#endif

template< typename T >
T host_to(int be, long long v, T &t)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
  bool rot = be;
#else
  bool rot = !be;
#endif
  if (rot)
    {
      T _v = v;
      for (unsigned i = 0; i < sizeof(T); ++i)
	((unsigned char*)&t)[i] = ((unsigned char *)&_v)[sizeof(T)-i-1];
    }
  else
    t = v;

  return t;
}

#define Elf(x) Elf32_ ## x
#include "elf-patcher.h"

#undef Elf
#define Elf(x) Elf64_ ## x
#include "elf-patcher.h"

static unsigned long long
get_param(const char *opt, int argc, const char *argv[])
{
  size_t l = strlen(opt);
  int i;
  for (i = 2; i < argc; ++i)
    {
      if (strncmp(opt, argv[i], l) == 0)
	{
	  if (strlen(argv[i]) >= l + 2)
	    return strtoll(argv[i] + l + 1, NULL, 0);
	  else
	    {
	      fprintf(stderr, "error parsing argument '%s'\n", opt);
	      return ~0ULL;
	    }
	}
    }
  return ~0ULL;
}

static int
check_elf(void *_elf, const char *name)
{
  Elf32_Ehdr *elf = (Elf32_Ehdr*)_elf;
  if (memcmp(elf->e_ident, ELFMAG, sizeof(ELFMAG)-1) != 0)
    {
      fprintf(stderr, "'%s' is not an ELF binary\n", name);
      return 1;
    }
  return 0;
}

static int
patch_phdrs(void *_elf, int argc, const char *argv[])
{
  Elf32_Ehdr *elf = (Elf32_Ehdr*)_elf;
  unsigned long long stack_addr, stack_size, kip_addr;

  stack_addr = get_param("--stack_addr", argc, argv);
  stack_size = get_param("--stack_size", argc, argv);
  kip_addr   = get_param("--kip_addr"  , argc, argv);

  if (stack_addr == ~0ULL || stack_size == ~0ULL || kip_addr == ~0ULL)
    return 1;

  if (elf->e_ident[EI_CLASS] == ELFCLASS64)
    return Elf64_patch_phdrs(elf, stack_addr, stack_size, kip_addr);
  else if (elf->e_ident[EI_CLASS] == ELFCLASS32)
    return Elf32_patch_phdrs(elf, stack_addr, stack_size, kip_addr);
  else
    {
      fprintf(stderr, "invalid elf class\n");
      return 1;
    }
}

static int
patch_shdrs(void *_elf, int argc, const char *argv[])
{
  Elf32_Ehdr *elf = (Elf32_Ehdr*)_elf;
  unsigned long long min_align = get_param("--min-section-align", argc, argv);

  if (min_align == ~0ULL)
    return 1;

  if (elf->e_ident[EI_CLASS] == ELFCLASS64)
    return Elf64_patch_shdrs(elf, min_align);
  else if (elf->e_ident[EI_CLASS] == ELFCLASS32)
    return Elf32_patch_shdrs(elf, min_align);
  else
    {
      fprintf(stderr, "invalid elf class\n");
      return 1;
    }
}

int main(int argc, const char *argv[])
{
  int victim;
  struct stat victim_sb;
  void *elf_addr;

  if (argc < 2)
    {
      fprintf(stderr,"usage: %s <elf-binary> [--stack_addr=<addr> --stack_size=<size> --kip_addr=<addr>] [--min-section-align=value]\n", argv[0]);
      return 1;
    }

  victim = open(argv[1], O_RDWR);
  if (victim < 0)
    {
      fprintf(stderr, "could not open '%s':", argv[1]);
      perror("");
      return 1;
    }

  if (fstat(victim, &victim_sb) == -1)
    {
      fprintf(stderr, "could not get size of '%s':", argv[1]);
      perror("");
      return 1;
    }

  elf_addr = mmap(NULL, victim_sb.st_size, PROT_READ | PROT_WRITE,
                  MAP_SHARED, victim, 0);

  if (elf_addr == MAP_FAILED)
    {
      fprintf(stderr, "could not mmap '%s':", argv[1]);
      perror("");
      return 1;
    }

  if (check_elf(elf_addr, argv[1]))
    return 1;

  patch_phdrs(elf_addr, argc, argv);
  patch_shdrs(elf_addr, argc, argv);

  close(victim);
  return 0;
}
