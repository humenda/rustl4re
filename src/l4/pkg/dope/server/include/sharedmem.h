/*
 * \brief   Interface of shared memory abstraction of DOpE
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

#ifndef _DOPE_SHAREDMEM_H_
#define _DOPE_SHAREDMEM_H_

#include "thread.h"

#define SHAREDMEM struct shared_memory
struct shared_memory;

struct sharedmem_services {
	SHAREDMEM *(*alloc)       (long size);
	void       (*destroy)     (SHAREDMEM *sm);
	void      *(*get_address) (SHAREDMEM *sm);
	void       (*get_ident)   (SHAREDMEM *sm, char *dst_ident_buf);
	s32        (*share)       (SHAREDMEM *sm, THREAD *dst_thread);
};


#endif /* _DOPE_SHAREDMEM_H_ */
