/**
 * \file   ferret/lib/gcc-instrument/instrument.c
 * \brief  Support functions for gcc-function-level instrumentation
 *
 * \date   2007-10-09
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2007-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
// fixme: provide explicit init call with function pointers for
//        myself() and malloc()

#include <stdlib.h>

#include <l4/log/l4log.h>

#include <l4/ferret/gcc_instrument.h>

#include <l4/ferret/client.h>
#include <l4/ferret/types.h>
#include <l4/ferret/sensors/list_producer.h>
#include <l4/ferret/sensors/list_producer_wrap.h>

/*
 * declarations
 */
void __cyg_profile_func_enter(void *func_address, void *call_site)
    __attribute__ ((no_instrument_function));

void __cyg_profile_func_exit (void *func_address, void *call_site)
    __attribute__ ((no_instrument_function));

void main_constructor(void)
    __attribute__ ((no_instrument_function, constructor));

void main_destructor(void)
    __attribute__ ((no_instrument_function, destructor));

/*
 * implementations
 */
ferret_list_local_t * __func_trace_list = NULL;
l4_cap_idx_t (*ferret_gcc_instrument_myself_func)(void) = NULL;

void __cyg_profile_func_enter(void *func_address, void *callsite)
{
    if (__func_trace_list)
        ferret_list_post_1t2w(__func_trace_list,
                              FERRET_FUNC_TRACE_MAJOR,
                              FERRET_FUNC_TRACE_MINOR_ENTER, 0,
                              l4_myself(), (int)func_address, (int)callsite);
}

void __cyg_profile_func_exit(void *func_address, void *callsite)
{
    if (__func_trace_list)
        ferret_list_post_1t2w(__func_trace_list,
                              FERRET_FUNC_TRACE_MAJOR,
                              FERRET_FUNC_TRACE_MINOR_EXIT, 0,
                              l4_myself(), (int)func_address, (int)callsite);
}


/*
void main_constructor(void)
{
    int ret;

    ret = ferret_create(FERRET_FUNC_TRACE_MAJOR, FERRET_FUNC_TRACE_MINOR_EXIT,
                        0, FERRET_LIST, FERRET_SUPERPAGES, "64:65000",
                        __func_trace_list, &malloc);
    if (ret)
    {
        LOG("Error creating sensor: ret = %d", ret);
        exit(1);
    }
}

void main_destructor(void)
{
    // fixme: close specific sensor here
}
*/

/* Set two function pointers and create sensor.
 */
void ferret_gcc_instrument_init(void *(*alloc)(size_t s),
                                l4_cap_idx_t (*myself)(void))
{
    int ret;
    ferret_gcc_instrument_myself_func = myself;

    // smaller event size, we know what will be inside
    ret = ferret_create(FERRET_FUNC_TRACE_MAJOR, FERRET_FUNC_TRACE_MINOR_EXIT,
                        0, FERRET_LIST, FERRET_SUPERPAGES, "32:65000",
                        __func_trace_list, alloc);
    if (ret)
    {
        LOG("Error creating sensor: ret = %d", ret);
        exit(1);
    }
}
