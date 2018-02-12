/*
 * Early_printk implementation, skeleton taken from x86_64 version.
 */
#include <linux/console.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>

#include <asm/generic/vcpu.h>
#include <l4/re/env.h>
#include <l4/sys/vcon.h>

static void vcon_outchar(char c)
{
	L4XV_FN_v(l4_vcon_write(l4re_env()->log, &c, 1));
}

static void early_kdb_write(struct console *con, const char *s, unsigned n)
{
	while (*s && n-- > 0) {
		vcon_outchar(*s);
		if (*s == '\n')
			vcon_outchar('\r');
		s++;
	}
}

static struct console early_kdb_console = {
	.name =		"earlykdb",
	.write =	early_kdb_write,
	.flags =	CON_PRINTBUFFER,
	.index =	-1,
};

static int __init setup_early_printk(char *buf)
{
	if (!buf)
		return 0;

	if (early_console)
		return 0;

	if (strstr(buf, "keep"))
		early_kdb_console.flags &= ~CON_BOOT;
	else
		early_kdb_console.flags |= CON_BOOT;

	early_console = &early_kdb_console;
	register_console(early_console);
	return 0;
}

early_param("earlyprintk", setup_early_printk);
