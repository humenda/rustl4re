/**
 * \file   ferret/include/maj_min.h
 * \brief  Collection of well known major / minor numbers for sensors.
 *
 * \date   2006-04-04
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2006-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef __FERRET_INCLUDE_MAJ_MIN_H_
#define __FERRET_INCLUDE_MAJ_MIN_H_

// Fiasco's tracebuffer
#define FERRET_TBUF_MAJOR    0
#define FERRET_TBUF_MINOR    0
#define FERRET_TBUF_INSTANCE 0

// L4Linux kernel sensors
#define FERRET_L4LX_MAJOR    1
// in-kernel minors
#define FERRET_L4LX_LIST_MINOR          0       // the global event list
#define FERRET_L4LX_SYSCALLCOUNT_MINOR  2       // system call histogram
#define FERRET_L4LX_SYSCALLPROF_MINOR   3       // profiling histogram
// minors for kprobe modules
#define FERRET_L4LX_FORKINST_MINOR      1000
#define FERRET_L4LX_FORKINST_MINOR2     1001
#define FERRET_L4LX_NETINST_MINOR       1004
// minors for atomic sequences (e.g., in tamer thread)
#define FERRET_L4LX_ATOMIC_BEGIN        2000
#define FERRET_L4LX_ATOMIC_END1         2001
#define FERRET_L4LX_ATOMIC_END2         2002
// minors for CLI / STI debugging in L4Linux
#define FERRET_L4LX_CLI_BEGIN           2010
#define FERRET_L4LX_CLI_END1            2011
#define FERRET_L4LX_CLI_END2            2012
#define FERRET_L4LX_STI_BEGIN           2013
#define FERRET_L4LX_STI_END             2014
// minors for tcp debugging
#define FERRET_L4LX_TCP_RETRANSMIT      3000
#define FERRET_L4LX_TCP_RETRANS_BUG     3001

// L4Linux userland sensor
#define FERRET_L4LXU_MAJOR   2
#define FERRET_L4LXU_MINOR   0

// Names
#define FERRET_NAMES_MAJOR  20

// dice_trace sensor
#define FERRET_DICE_MAJOR  100
#define FERRET_DICE_MINOR    0

// L4env sensor
#define FERRET_L4ENV_MAJOR 102
#define FERRET_L4ENV_MINOR   0

// dope experiments
#define FERRET_DOPE_MAJOR  104
#define FERRET_DOPE_MINOR    0

// input experiments
#define FERRET_INPUT_MAJOR 105
#define FERRET_INPUT_MINOR   0

// l4io experiments
#define FERRET_L4IO_MAJOR 106
#define FERRET_L4IO_MINOR   0

// function call trace
#define FERRET_FUNC_TRACE_MAJOR       200
#define FERRET_FUNC_TRACE_MINOR_ENTER   1
#define FERRET_FUNC_TRACE_MINOR_EXIT    2

/********************************************************************/

// some special events for controlling monitors

#define FERRET_MONCON_MAJOR   50000  // for events and sensors
#define FERRET_MONCON_MINOR   50001  // minor for sensor

// minor for events, avoid collisions for easy debugging
#define FERRET_MONCON_START   50010  // start recording now
#define FERRET_MONCON_STOP    50011  // stop recording now
#define FERRET_MONCON_NETSEND 50012  // send data now over tcp, details are
                                     // defined in the event
#define FERRET_MONCON_SERSEND 50013  // send data now over serial line,
                                     // uuencoded
#define FERRET_MONCON_CLEAR   50014  // clear buffer from recorded data
#define FERRET_MONCON_STATS   50015  // dump some stats in the monitor
#define FERRET_MONCON_PING    50016  // encourage the monitor to show a sign
                                     // of life, maybe by log output to check
                                     // connection

// special events for signaling loss of events from source

#define FERRET_EVLOSS_MAJOR   51000
#define FERRET_EVLOSS_MINOR   51000

#endif
