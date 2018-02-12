/**
 * \file   ferret/examples/merge_mon/buffer.h
 * \brief  Buffer data structures
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
#ifndef __FERRET_EXAMPLES_MERGE_MON_BUFFER_H_
#define __FERRET_EXAMPLES_MERGE_MON_BUFFER_H_

#include <sys/types.h>

#include <l4/ferret/types.h>
#include <pthread-l4.h>

typedef struct
{
    unsigned char * start;      // buffer's starting address
    ssize_t         size;       // full size of the buffer
    unsigned char * write_pos;  // current buffer position for writing
    unsigned char * dump_pos;   // current buffer position for dumping
    pthread_mutex_t lock;       // lock protecting buffer access
    ferret_utime_t  start_ts;   // for filtering events, start timestamp
    ferret_utime_t  stop_ts;    // for filtering events, stop timestamp
    int             full;       // flag whether buffer is full
    int             cyclic;     // auto-clear buffer if it is full
} buf_t;

int buf_fill(buf_t * buf, int max_size, int interval, int verbose);
void buf_set_start_ts(buf_t * buf);
void buf_set_stop_ts(buf_t * buf);
void buf_clear(buf_t * buf);
void buf_set_offline(buf_t * buf);
void buf_set_online(buf_t * buf);
void buf_set_cyclic(buf_t * buf);


#endif
