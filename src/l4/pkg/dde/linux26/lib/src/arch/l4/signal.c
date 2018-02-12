/*
 * This file is part of DDE/Linux2.6.
 *
 * (c) 2006-2012 Bjoern Doebel <doebel@os.inf.tu-dresden.de>
 *               Christian Helmuth <ch12@os.inf.tu-dresden.de>
 *     economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "local.h"

/******************************************************************************
 ** Dummy signal implementation.                                             **
 ** DDE does not provide its own signal implementation. To make it compile,  **
 ** we provide dummy versions of signalling functions here. If later on      **
 ** someone *REALLY* wants to use signals in the DDE context, he might       **
 ** erase this file and use something like the L4 signalling library for     **
 ** such purposes.                                                           **
*******************************************************************************/

int sigprocmask(int how, sigset_t *set, sigset_t *oldset)
{
	return 0;
}

void flush_signals(struct task_struct *t)
{
}

int do_sigaction(int sig, struct k_sigaction *act, struct k_sigaction *oact)
{
	return 0;
}
