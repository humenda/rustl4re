/**
 * \file   ferret/lib/sensor/ulist_init.c
 * \brief  ulist init functions.
 *
 * \date   2007-06-19
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2007-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/ferret/local_names.h>
#include <l4/ferret/sensors/ulist.h>
#include <l4/ferret/sensors/ulist_init.h>

#include <l4/ferret/sensors/__ulist_magic.h>
#define FERRET_LLIST_MAGIC ahK6eeNa
#include "__llist_init.c"
