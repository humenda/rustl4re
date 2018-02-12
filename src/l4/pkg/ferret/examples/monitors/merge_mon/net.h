/**
 * \file   ferret/examples/merge_mon/net.h
 * \brief  Network transfer stuff
 *
 * \date   17/01/2006
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2006-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef __FERRET_EXAMPLES_MERGE_MON_NET_H_
#define __FERRET_EXAMPLES_MERGE_MON_NET_H_

extern l4semaphore_t net_sem;
extern int net_connected;

int net_init(const char * ip);

#endif
