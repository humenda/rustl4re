/*
 * Some interface to the l4fb driver.
 */
#ifndef __ASM_L4__GENERIC__L4FB_H__
#define __ASM_L4__GENERIC__L4FB_H__

#include <l4/sys/types.h>
//#include <l4/dm_generic/dm_generic.h>

enum {
	L4FB_REFRESH_DISABLED,
	L4FB_REFRESH_ENABLED,
};

/**
 * Enable or disable screen refreshing.
 *
 * Status: Use L4FB_REFRESH_DISABLED or L4FB_REFRESH_ENABLED
 */
void l4fb_refresh_status_set(int status);

/**
 * Return thread ID of the con server.
 */
//l4_threadid_t l4fb_con_con_id_get(void);

/**
 * Return thread ID of the VC.
 */
//l4_threadid_t l4fb_con_vc_id_get(void);

/**
 * Return ID of the dataspace.
 */
//l4dm_dataspace_t l4fb_con_ds_id_get(void);

/**
 * Type for input event hook function.
 */
typedef int (*l4fb_input_event_hook_function_type)(long type, long code);

/**
 * Register hook function for input events.
 */
void l4fb_input_event_hook_register(l4fb_input_event_hook_function_type f);

#endif /* ! __ASM_L4__GENERIC__L4FB_H__ */
