
#include <linux/console.h>

#include <asm/generic/log.h>
#include <asm/generic/vcpu.h>

#include <l4/log/log.h>

void l4x_printf(const char *fmt, ...)
{
	va_list list;
	va_start(list, fmt);
	L4XV_FN_v(LOG_vprintf(fmt, list));
	va_end(list);
}
EXPORT_SYMBOL(l4x_printf);

void l4x_early_printk(const char *fmt, ...)
{
	va_list list;
	va_start(list, fmt);
	if (!console_drivers) {
		/* print a marker here because the same output will
		 * come later again through printk buffering */
		l4x_printf("l4x-early-printk: ");
		L4XV_FN_v(LOG_vprintf(fmt, list));
	}
	vprintk(fmt, list);
	va_end(list);
}
