#ifndef __ASM_L4__ARCH_I386__TIMER_H__
#define __ASM_L4__ARCH_I386__TIMER_H__

#include <asm-x86/timer.h>

#ifdef CONFIG_L4_EXTERNAL_RTC
extern struct init_timer_opts timer_l4rtc_init;
#endif

#endif /* ! __ASM_L4__ARCH_I386__TIMER_H__ */
