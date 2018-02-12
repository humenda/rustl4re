/**
 * \file	con/server/src/config.h
 * \brief	con configuration macros
 *
 * \date	2001
 * \author	Christian Helmuth <ch12@os.inf.tu-dresden.de>
 *		Frank Mehnert <fm3@os.inf.tu-dresden.de> */
/*
 * (c) 2001-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

/* vc */
#define CONFIG_MAX_VC		20		/* number of virtual consoles */
#define CONFIG_MAX_SBUF_SIZE	(4*256*256)	/* max string buffer */

/* Time between calls to our periodic jobs. */
#define REQUEST_TIMEOUT_DELTA	50000

#define CONFIG_MAX_CLIENTS      4
