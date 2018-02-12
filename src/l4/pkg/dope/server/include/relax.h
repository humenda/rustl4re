/*
 * \brief   Interface of relaxation module
 * \date    2004-08-20
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

#ifndef _DOPE_RELAX_H_
#define _DOPE_RELAX_H_

#define RELAX struct relax
RELAX {
	float speed;
	float dst;
	float curr;
	float accel;
};

struct relax_services {
	void (*set_duration) (RELAX *, float time);
	int  (*do_relax)     (RELAX *);
};


#endif /* _DOPE_RELAX_H_ */
