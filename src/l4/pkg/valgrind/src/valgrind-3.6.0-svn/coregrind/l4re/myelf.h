/*
 * This file is part of the Valgrind port to L4Re.
 *
 * (c) 2009-2010 Aaron Pohle <apohle@os.inf.tu-dresden.de>,
 *               Bjoern Doebel <doebel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universitaet Dresden (Germany)
 */
#pragma once
#include <elf.h>
#include <string.h>

#include "pub_core_basics.h"
#include "pub_l4re.h"
#include "pub_l4re_consts.h"
#include "pub_tool_libcbase.h"
#include "pub_core_libcprint.h"

typedef struct global
{
	Elf32_Ehdr *elf_hdr;
	char       *section_string_table;
	char       *symbol_string_table;
	Elf32_Sym  *symbol_section;
	unsigned    symbol_section_size;
} melf_global_elf_info;


static inline void hexdump(void *a, unsigned num)
{
	unsigned i = 0;
	for ( ; i < num; ++i) {
		VG_(printf)("%02x ", *((unsigned char*)a + i));
	}
}


static inline int elf_check_magic(Elf32_Ehdr *e)
{
	static const char elf_mag[4] = {ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3};

	if (VG_(memcmp)(&e->e_ident, elf_mag, 4)) {
		VG_(printf)("ident: "); hexdump(e, EI_NIDENT); VG_(printf)("\n");
		VG_(printf)("header mismatch\n");
		return 0;
	}

	return 1;
}


static inline void print_Ehdr_info(Elf32_Ehdr* e)
{
	VG_(printf)(" --- valid ELF file\n");
	VG_(printf)(" --- phoff: 0x%x\n", e->e_phoff);
	VG_(printf)(" --- phnum: %u\n", e->e_phnum);
	VG_(printf)(" --- shoff: 0x%x\n", e->e_shoff);
	VG_(printf)(" --- shnum: %u\n", e->e_shnum);
	VG_(printf)(" --- ehsize: 0x%x\n", e->e_ehsize);
	VG_(printf)(" --- string table section: %d\n", e->e_shstrndx);
}

/*****************************************************************************
 *                            Section lookup                                 *
 *****************************************************************************/

/*
 * Locate the section containing section name strings.
 */
int melf_locate_section_string_table(melf_global_elf_info *inf);

/*
 * Locate a section with a specific type.
 */
Elf32_Shdr *melf_find_section_by_type(melf_global_elf_info *inf, unsigned type);

/*
 * Locate a section with a specific name.
 */
Elf32_Shdr *melf_find_section_by_name(melf_global_elf_info *inf, char const *name);


/*****************************************************************************
 *                            Symbol lookup                                  *
 *****************************************************************************/

/*
 * Find the address of a symbol with a given name
 */
char *melf_find_symbol_by_name(melf_global_elf_info *elf, char *name);

/*
 * Display list of all symbols and their addresses
 */
void melf_show_symbols(melf_global_elf_info *elf);

/*
 * Parse ELF file (mmap()ped to address a) and return a melf_global_elf_info
 * descriptor for it.
 */
int melf_parse_elf(void *a, melf_global_elf_info *info);
