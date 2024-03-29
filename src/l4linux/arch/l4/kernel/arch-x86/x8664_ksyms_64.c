#error remove this
/* Exports for assembly files.
   All C exports should go in the respective C files. */

#include <linux/export.h>
#include <linux/spinlock_types.h>
#include <linux/smp.h>

#include <net/checksum.h>

#include <asm/processor.h>
#include <asm/pgtable.h>
#include <asm/uaccess.h>
#include <asm/desc.h>
#include <asm/ftrace.h>

#ifdef CONFIG_FUNCTION_TRACER
/* mcount and __fentry__ are defined in assembly */
#ifdef CC_USING_FENTRY
EXPORT_SYMBOL(__fentry__);
#else
EXPORT_SYMBOL(mcount);
#endif
#endif

#ifndef CONFIG_L4
EXPORT_SYMBOL(__get_user_1);
EXPORT_SYMBOL(__get_user_2);
EXPORT_SYMBOL(__get_user_4);
EXPORT_SYMBOL(__get_user_8);
EXPORT_SYMBOL(__put_user_1);
EXPORT_SYMBOL(__put_user_2);
EXPORT_SYMBOL(__put_user_4);
EXPORT_SYMBOL(__put_user_8);

EXPORT_SYMBOL(copy_user_generic_string);
EXPORT_SYMBOL(copy_user_generic_unrolled);
EXPORT_SYMBOL(copy_user_enhanced_fast_string);
#endif /* L4 */
EXPORT_SYMBOL(__copy_user_nocache);
#ifndef CONFIG_L4
EXPORT_SYMBOL(_copy_from_user);
EXPORT_SYMBOL(_copy_to_user);

EXPORT_SYMBOL_GPL(memcpy_mcsafe);
#endif /* L4 */

EXPORT_SYMBOL(copy_page);
EXPORT_SYMBOL(clear_page);

#ifndef CONFIG_L4
EXPORT_SYMBOL(csum_partial);
#endif /* L4 */

EXPORT_SYMBOL(__sw_hweight32);
EXPORT_SYMBOL(__sw_hweight64);

/*
 * Export string functions. We normally rely on gcc builtin for most of these,
 * but gcc sometimes decides not to inline them.
 */
#undef memcpy
#undef memset
#undef memmove

extern void *__memset(void *, int, __kernel_size_t);
extern void *__memcpy(void *, const void *, __kernel_size_t);
extern void *__memmove(void *, const void *, __kernel_size_t);
extern void *memset(void *, int, __kernel_size_t);
extern void *memcpy(void *, const void *, __kernel_size_t);
extern void *memmove(void *, const void *, __kernel_size_t);

EXPORT_SYMBOL(__memset);
EXPORT_SYMBOL(__memcpy);
EXPORT_SYMBOL(__memmove);

EXPORT_SYMBOL(memset);
#ifndef CONFIG_L4
EXPORT_SYMBOL(memcpy);
#endif
EXPORT_SYMBOL(memmove);

#ifndef CONFIG_DEBUG_VIRTUAL
EXPORT_SYMBOL(phys_base);
#endif
#ifndef CONFIG_L4
EXPORT_SYMBOL(empty_zero_page);
#ifndef CONFIG_PARAVIRT
EXPORT_SYMBOL(native_load_gs_index);
#endif

#ifdef CONFIG_PREEMPTION
EXPORT_SYMBOL(___preempt_schedule);
EXPORT_SYMBOL(___preempt_schedule_notrace);
#endif
#endif /* L4 */
