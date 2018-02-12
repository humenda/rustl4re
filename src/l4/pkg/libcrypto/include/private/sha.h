/*
 * \brief   header of sha.c
 * \date    2006-03-28
 * \author  Bernhard Kauer <kauer@tudos.org>
 */
/*
 * Copyright (C) 2006  Bernhard Kauer <kauer@tudos.org>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the OSLO package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

#ifndef _SHA_H_
#define _SHA_H_


struct Context
{
  unsigned int index;
  unsigned long blocks;
  unsigned char buffer[64+4];
  unsigned char hash[20];
};

#ifdef SHA1_PROTOTYPES
void sha1_init(struct Context *ctx);
void sha1(struct Context *ctx, unsigned char* value, unsigned count);
void sha1_finish(struct Context *ctx);
#endif


#endif
