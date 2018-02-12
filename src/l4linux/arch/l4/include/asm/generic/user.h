#ifndef __INCLUDE__ASM_L4__GENERIC__USER_H__
#define __INCLUDE__ASM_L4__GENERIC__USER_H__

#include <linux/bug.h>
#include <linux/kernel.h>
#include <linux/threads.h>

#include <l4/sys/consts.h>
#include <l4/sys/utcb.h>

/* Capability layout in a user process */
enum {
  L4LX_USER_CAP_TASK       = L4_BASE_TASK_CAP,     // 1
  L4LX_USER_CAP_FACTORY    = L4_BASE_FACTORY_CAP,  // 2
  L4LX_USER_CAP_THREAD     = L4_BASE_THREAD_CAP,   // 3
  L4LX_USER_CAP_PAGER      = L4_BASE_PAGER_CAP,    // 4
  L4LX_USER_CAP_LOG        = L4_BASE_LOG_CAP,      // 5
  L4LX_USER_CAP_MA         = 7 << L4_CAP_SHIFT,

  L4LX_USER_CAP_PAGER_BASE = 0x10 << L4_CAP_SHIFT,


  L4LX_KERN_CAP_HYBRID_BASE = 0x2000 << L4_CAP_SHIFT,
};

/* Address space layout in a user process */
enum {
	L4X_USER_KIP_ADDR = 0xbfdee000,
	L4X_USER_KIP_SIZE = 12,

	L4X_USER_UTCB_ADDR      = 0xbfdf0000,
	L4X_USER_UTCB_MAX_SIZE  = 0x10000,
	L4X_USER_UTCB_NR_PAGES
		= (NR_CPUS * L4_UTCB_OFFSET + L4_PAGESIZE - 1) / L4_PAGESIZE,
	L4X_USER_UTCB_NR_PAGES_MOST_SIGNIFICANT_BIT
		= 31 - __builtin_clz(L4X_USER_UTCB_NR_PAGES),
	L4X_USER_UTCB_SIZE
		= (((L4X_USER_UTCB_NR_PAGES
		     & (L4X_USER_UTCB_NR_PAGES - 1)) == 0) ? 0 : 1)
		  + L4X_USER_UTCB_NR_PAGES_MOST_SIGNIFICANT_BIT + L4_PAGESHIFT,
};

#ifndef CONFIG_L4_VCPU
static inline l4_cap_idx_t l4x_user_pager_cap(unsigned cpu)
{
	return cpu ? (L4LX_USER_CAP_PAGER_BASE + (cpu << L4_CAP_SHIFT))
	           : L4LX_USER_CAP_PAGER;
}

static inline l4_utcb_t *l4x_user_utcb_addr(unsigned cpu)
{
	BUILD_BUG_ON(NR_CPUS * L4_UTCB_OFFSET > L4X_USER_UTCB_MAX_SIZE);
	return (l4_utcb_t *)((unsigned long)L4X_USER_UTCB_ADDR
	                     + L4_UTCB_OFFSET * cpu);
}
#endif /* ! CONFIG_L4_VCPU */
#endif /* ! __INCLUDE__ASM_L4__GENERIC__USER_H__ */
