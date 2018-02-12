/*
 * \brief   DOpElib misc internal interface
 * \date    2005-11-19
 * \author  Norman Feske <nf2@inf.tu-dresden.de>
 */

/*
 * Copyright (C) 2005  Norman Feske  <nf2@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the DOpE package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

#ifndef _DOPELIB_MISC_H_
#define _DOPELIB_MISC_H_


/*****************************
 *** IMPLEMENTED IN MAIN.C ***
 *****************************/

/*** UTILITY: CONVERT CALLBACK FUNCTION TO BIND ARGUMENT STRING ***/
extern char *dopelib_callback_to_bindarg(void (*callback)(dope_event *,void *),
                                         void *arg,
                                         char *dst_buf, int dst_len);

#endif /* _DOPELIB_MISC_H_ */
