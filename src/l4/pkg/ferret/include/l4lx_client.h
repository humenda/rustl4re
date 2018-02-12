/**
 * \file   ferret/include/l4lx_client.h
 * \brief  Helper function for L4Linux user mode clients.
 *
 * \date   2006-04-03
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2006-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef __FERRET_INCLUDE_L4LX_CLIENT_H_
#define __FERRET_INCLUDE_L4LX_CLIENT_H_

#include <l4/sys/compiler.h>
#include <l4/ferret/sensors/list_producer.h>

EXTERN_C_BEGIN

/**< process-global pointer to l4Linux user land sensor, init */
extern ferret_list_local_t * ferret_l4lx_user;

/**
 * @brief Setup the global userland list ferret_l4lx_user for L4Linux
 *        clients.
 *
 * @param  alloc function pointer to memory allocating function
 */
L4_CV void ferret_list_l4lx_user_init(void);

EXTERN_C_END

#endif
