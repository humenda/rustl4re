/**
 * \file   ferret/server/sensor_directory/dir.h
 * \brief  Directory server, serves as well known instance and
 *         naming service for sensors
 *
 * \date   07/11/2005
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2005-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef __FERRET_SERVER_SENSOR_DIRECTORY_DIR_H_
#define __FERRET_SERVER_SENSOR_DIRECTORY_DIR_H_

#include <l4/l4vfs/tree_helper.h>

extern int verbose;
extern l4vfs_th_node_t * root;

#endif
