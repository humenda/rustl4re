/*
 * \brief   Local fixed template instanciations
 * \date    2006-09-21
 * \author  Norman Feske <norman.feske@genode-labs.com>
 *
 * See also 'instances.h'.
 */

/*
 * Copyright (C) 2006-2009
 * Genode Labs, Feske & Helmuth Systementwicklung GbR
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LOCAL_INSTANCES_H_
#define _LOCAL_INSTANCES_H_
#if 0
#include "canvas_rgb565.h"


/****************************
 ** Template instantiation **
 ****************************/

#ifndef DOPE_VERSION
#include "browser_window.h"
template class Browser_window<Pixel_rgb565>;
#endif
#endif

#endif /* _INSTANCES_H_ */
