/**
 * \file   ferret/examples/merge_mon/main.h
 * \brief  Global data structures
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
#ifndef __FERRET_EXAMPLES_MERGE_MON_MAIN_H_
#define __FERRET_EXAMPLES_MERGE_MON_MAIN_H_

#include <l4/ferret/types.h>

#include "main.h"
#include "buffer.h"


#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MAX_SENSORS 256
#define MAX_EVENT_SIZE 4096

typedef struct sensor_entry_s
{
    char         * name;
    uint16_t       major;
    uint16_t       minor;
    uint16_t       instance;
    void         * sensor;
    int            open;
    unsigned int   copied;
    uint64_t       last_lost;  // lost count last time we checked
    ferret_utime_t last_ts;
} sensor_entry_t;

extern sensor_entry_t sensors[MAX_SENSORS];
extern int sensor_count;
extern char local_ip[16];
extern unsigned short local_port;
extern int verbose;
extern int use_gui;
extern buf_t buf;

#endif
