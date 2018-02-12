#include "myelf.h"
#include "pub_core_libcassert.h"

/*
 * This file is part of the Valgrind port to L4Re.
 *
 * (c) 2009-2010 Aaron Pohle <apohle@os.inf.tu-dresden.de>,
 *               Bjoern Doebel <doebel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universitaet Dresden (Germany)
 */

int melf_locate_section_string_table(melf_global_elf_info *inf)
{
	Elf32_Shdr *sec = (Elf32_Shdr*)((char*)inf->elf_hdr
	                                + inf->elf_hdr->e_shoff
	                                + inf->elf_hdr->e_shstrndx * inf->elf_hdr->e_shentsize);
	if (sec->sh_type != SHT_STRTAB) {
		VG_(printf)("no string table?\n");
		return -1;
	}

	inf->section_string_table = (char*)inf->elf_hdr + sec->sh_offset;
	return 0;
}


static inline void melf_validate_section_string_table(melf_global_elf_info *inf)
{
	if (!inf->section_string_table) {
		int r = melf_locate_section_string_table(inf);
		vg_assert(r == 0);
	}
}


Elf32_Shdr *melf_find_section_by_type(melf_global_elf_info *inf, unsigned type)
{
	vg_assert(inf);
	unsigned i = 0;

	Elf32_Shdr *sh = (Elf32_Shdr*)((char*)inf->elf_hdr + inf->elf_hdr->e_shoff);

	for ( ; i < inf->elf_hdr->e_shnum; ++i, ++sh) {
		if (sh->sh_type == type)
			return sh;
	}

	return NULL;
}


Elf32_Shdr *melf_find_section_by_name(melf_global_elf_info *inf, char const *name)
{
	vg_assert(inf);
	vg_assert(name);

	melf_validate_section_string_table(inf);

	unsigned i = 0;
	Elf32_Shdr *sh = (Elf32_Shdr*)((char*)inf->elf_hdr + inf->elf_hdr->e_shoff);

	for ( ; i < inf->elf_hdr->e_shnum; ++i, ++sh) {
		char *secname = inf->section_string_table + sh->sh_name;
		if (strcmp(secname, name) == 0)
			return sh;
	}
	
	return NULL;
}


char *melf_find_symbol_by_name(melf_global_elf_info *elf, char *n)
{
	vg_assert(elf);
	vg_assert(n);

	Elf32_Sym* sym = elf->symbol_section;

	while ((char*)sym < (char*)elf->symbol_section + elf->symbol_section_size) {
		if (strcmp(elf->symbol_string_table + sym->st_name, n) == 0)
			return (char*)sym->st_value;
		++sym;
	}
	
	return NULL;
}


void melf_show_symbols(melf_global_elf_info *elf)
{
	vg_assert(elf);
	Elf32_Sym* sym = elf->symbol_section;

	while ((char*)sym < (char*)elf->symbol_section + elf->symbol_section_size) {
		VG_(printf)("%p %s\n", (void*)sym->st_value, elf->symbol_string_table + sym->st_name);
		++sym;
	}
}


int melf_parse_elf(void *a, melf_global_elf_info *inf)
{
	unsigned i;
	Elf32_Ehdr *e = (Elf32_Ehdr *)a;

	if (!elf_check_magic(e))
		return -1;

	if (0) print_Ehdr_info(e);

	if (!inf) return -1;

	inf->elf_hdr = e;

	i = melf_locate_section_string_table(inf); 
	if (i)
		goto err;
	if (0) VG_(printf)(" --- section string table @ %p\n", inf->section_string_table);

	Elf32_Shdr *sec = melf_find_section_by_name(inf, ".strtab");
	if (!sec)
		goto err;
	inf->symbol_string_table = (char*)inf->elf_hdr + sec->sh_offset;
	if (0) VG_(printf)(" --- symbol string table @ %p\n", inf->symbol_string_table);

	sec = melf_find_section_by_type(inf, SHT_SYMTAB);
	if (!sec)
		goto err;
	inf->symbol_section      = (Elf32_Sym*)((char*)inf->elf_hdr + sec->sh_offset);
	inf->symbol_section_size = sec->sh_size;
	if (0)
		VG_(printf)(" --- symbol table @ %p, size %d %x\n", inf->symbol_section,
	           inf->symbol_section_size, inf->symbol_section_size);

	return 0;

err:
	return -2;
}
