/**
 * \internal
 * \file   ferret/include/sensors/__llist_init.h
 * \brief  locked list init functions.
 *
 * \date   2007-05-16
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2007-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#if FERRET_LLIST_MAGIC != ahK6eeNa
#error Do not directly include this file, use a proper wrapper!
#endif
#undef FERRET_LLIST_MAGIC

#include <l4/ferret/types.h>
//#include <l4/ferret/sensors/llist.h>
#include <l4/ferret/sensors/common.h>

#include <l4/sys/compiler.h>

EXTERN_C_BEGIN

int     PREFIX(init)       (PREFIX(t) * list, const char * config,
                            char ** data);
ssize_t PREFIX(size_config)(const char * config);
ssize_t PREFIX(size)       (const PREFIX(t) * list);

EXTERN_C_END
