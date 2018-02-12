/*****************************************************************************/
/**
 * \file   loader/linux/dump-l4/l4env_dummies.c
 * \brief  Dummmy implementations for some unused L4Env functions
 *
 * \date   02/15/2002
 * \author Lars Reuther <reuther@os.inf.tu-dresden.de>
 */
/*****************************************************************************/
/*
 * $Log$
 * Revision 1.5  2004/08/16 15:49:54  reuther
 * - adapted to changes in l4rm
 *
 * Revision 1.4  2004/08/13 15:38:01  reuther
 * - adapted to changes of l4rm_lookup()
 *
 * Revision 1.3  2003/05/20 13:50:14  ra3
 * - merge dice branch into HEAD
 * - old flick branch can be found using -j pre-dice-merge
 *
 * Revision 1.2.2.1  2003/01/24 15:21:13  ra3
 * - changed from CORBA_* types to l4_* types
 * - some small IDL call fixes
 *
 * Revision 1.2  2002/11/25 01:07:47  reuther
 * - adapted to new L4 integer types
 * - adapted to l4sys/l4util/l4env/dm_generic include changes
 *
 * Revision 1.1  2002/05/02 09:41:12  adam
 * - screenshot program for DROPS console running under L4Linux
 * - type "dropsshot --help" for help, if that doesn't suffice beg me
 *   to write a manpage...
 *
 * Revision 1.1  2002/02/18 03:48:12  reuther
 * Emulate some l4env functions.
 *
 */

#include <l4/sys/types.h>
#include <l4/env/errno.h>
#include <l4/log/l4log.h>
#include <l4/l4rm/l4rm.h>
#include <l4/env/env.h>

//void 
//LOG_flush(void)
//{
//}

int
l4rm_lookup(const void * addr, l4_addr_t * map_addr, l4_size_t * map_size,
            l4dm_dataspace_t * ds, l4_offs_t * offset, l4_threadid_t * pager)
{
  return -L4_ENOTSUPP;
}

l4_threadid_t
l4env_get_default_dsm(void)
{
  return L4_INVALID_ID;
}
