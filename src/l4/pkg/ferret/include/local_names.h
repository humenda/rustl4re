/**
 * \file   ferret/include/local_names.h
 * \brief  Manage local names for kernel objects.
 *
 * \date   2007-06-20
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2005-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef __FERRET_INCLUDE_LOCAL_NAMES_H_
#define __FERRET_INCLUDE_LOCAL_NAMES_H_

#include <l4/sys/linkage.h>

/**
 * @brief Reserve one name entry for a new kernel object
 *
 * @return index of local name assigned (1 .. MAX_LOCAL_NAMES)
 *         - 0 on failure
 */
L4_CV int ferret_local_names_reserve(void);

/**
 * @brief Free one entry in local name table for kernel objects
 *
 * @param index index to free
 */
L4_CV void ferret_local_names_dispose(int index);

#endif
