/*
 * \brief   DOpE module structure
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

#ifndef _DOPE_MODULE_H_
#define _DOPE_MODULE_H_

struct module_info {
	char    *type;      /* kind of module */
	char    *name;      /* name and version */
	char    *conflict;  /* lowest version to which the module is compatibe with */
	char    *uses;      /* required modules */
};

struct module {
	struct module_info  *info;
	int                (*init)   (void);
	int                (*deinit) (void);
	void                *services;
};


#endif /* _DOPE_MODULE_H_ */

