#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/mm.h>

#include <asm-generic/sections.h>
#include <asm/e820/api.h>

#include <asm/generic/setup.h>

char * __init l4x_x86_memory_setup(void)
{
	unsigned long mem_start, mem_size;
	unsigned long textbegin = (unsigned long)&_stext;

	l4x_setup_memory(boot_command_line, &mem_start, &mem_size);

	max_pfn_mapped = (mem_start + mem_size + ((1 << 12) - 1)) >> 12;

	e820_table->nr_entries = 0;

        /* minimum 2 pages required */
	e820__range_add(0, textbegin, E820_TYPE_RESERVED);

	if ((unsigned long)&_end > mem_start)
		printk("Uh, something looks strange.\n");
	e820__range_add(textbegin, (unsigned long)&_end - textbegin,
	                E820_TYPE_RESERVED_KERN);
	e820__range_add((unsigned long)&_end,
	                mem_start - (unsigned long)&_end, E820_TYPE_UNUSABLE);
	e820__range_add(mem_start, mem_size, E820_TYPE_RAM);

	e820__update_table(e820_table);

	return "L4Lx-Memory";
}
