#ifndef __ASM_L4__GENERIC__LOG_H__
#define __ASM_L4__GENERIC__LOG_H__

#include <linux/printk.h>

void l4x_printf(const char *fmt, ...)
   __attribute__((format(printf, 1, 2)));

void l4x_early_printk(const char *fmt, ...)
   __attribute__((format(printf, 1, 2)));

#define l4x_early_pr_err(fmt, ...) \
	l4x_early_printk(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)
#define l4x_early_pr_warning(fmt, ...) \
	l4x_early_printk(KERN_WARNING pr_fmt(fmt), ##__VA_ARGS__)
#define l4x_early_pr_warn l4x_early_pr_warning
#define l4x_early_pr_info(fmt, ...) \
	l4x_early_printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)

#endif /* ! __ASM_L4__GENERIC__LOG_H__ */
