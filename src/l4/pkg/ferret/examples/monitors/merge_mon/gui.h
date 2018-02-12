/**
 * \file   ferret/examples/merge_mon/gui.h
 * \brief  Some GUI related functions
 *
 * \date   13/12/2005
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2005-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef __FERRET_EXAMPLES_MERGE_MON_GUI_H_
#define __FERRET_EXAMPLES_MERGE_MON_GUI_H_

extern long app_id;
extern int global_exit;

int gui_init(void);
void gui_update(const buf_t * buf);

#endif
