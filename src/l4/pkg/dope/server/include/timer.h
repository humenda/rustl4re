/*
 * \brief   Interface of the timer module of DOpE
 * \date    2002-11-13
 * \author  Norman Feske <nf2@inf.tu-dresden.de>
 */

/*
 * Copyright (C) 2002-2004  Norman Feske  <nf2@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the DOpE package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

#ifndef _DOPE_TIMER_H_
#define _DOPE_TIMER_H_

struct timer_services {
	u32     (*get_time) (void);
	u32     (*get_diff) (u32 time1,u32 time2);
	void    (*usleep)   (u32 num_usec);
};


#endif /* _DOPE_TIMER_H_ */

