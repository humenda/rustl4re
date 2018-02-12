/*
 * \brief   Error values that are exported by DOpE
 * \date    2004-08-06
 * \author  Norman Feske <nf2@inf.tu-dresden.de>
 */

/*
 * Copyright (C) 2002-2003  Norman Feske  <nf2@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the DOpE package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

#ifndef __DOPE_INCLUDE_DOPEDEF_H_
#define __DOPE_INCLUDE_DOPEDEF_H_


#define DOPE_ERR_PERM               -1  /* permission denied          */
#define DOPE_ERR_NOT_PRESENT        -2  /* no DOpE server to speak to */

#define DOPECMD_ERR_UNKNOWN_VAR    -11  /* variable does not exist               */
#define DOPECMD_ERR_INVALID_VAR    -12  /* variable became invalid               */
#define DOPECMD_ERR_INVALID_TYPE   -13  /* variable type does not exist          */
#define DOPECMD_ERR_ATTR_W_PERM    -14  /* attribute cannot be written           */
#define DOPECMD_ERR_ATTR_R_PERM    -15  /* attribute cannot be requested         */
#define DOPECMD_ERR_NO_SUCH_MEMBER -16  /* method or attribute does not exist    */
#define DOPECMD_ERR_NO_TAG         -17  /* expected tag but found something else */
#define DOPECMD_ERR_UNKNOWN_TAG    -18  /* tag does not exist                    */
#define DOPECMD_ERR_INVALID_ARG    -19  /* illegal argument value                */
#define DOPECMD_ERR_LEFT_PAR       -20  /* missing left parenthesis              */
#define DOPECMD_ERR_RIGHT_PAR      -21  /* missing right parenthesis             */
#define DOPECMD_ERR_TOO_MANY_ARGS  -22  /* too many method arguments             */
#define DOPECMD_ERR_MISSING_ARG    -23  /* mandatory argument is missing         */
#define DOPECMD_ERR_UNCOMPLETE     -24  /* unexpected end of command             */
#define DOPECMD_ERR_NO_SUCH_SCOPE  -25  /* variable scope could not be resolved  */
#define DOPECMD_ERR_ILLEGAL_CMD    -26  /* command could not be examined         */

#define DOPECMD_WARN_TRUNC_RET_STR  11  /* return string was truncated */

#ifdef __cplusplus

#include <l4/re/event>
#include <l4/sys/cxx/ipc_client>
#include <l4/sys/cxx/ipc_types>
#include <l4/sys/cxx/ipc_string>

namespace Dope {
  struct Dope : L4::Kobject_t<Dope, L4Re::Event, 0x1090>
  {
    L4_INLINE_RPC(long, c_cmd, (L4::Ipc::String<> cmd));
    L4_INLINE_RPC(long, c_cmd_req, (L4::Ipc::String<> cmd, L4::Ipc::String<char> *res));
    L4_INLINE_RPC(long, c_vscreen_get_fb, (L4::Ipc::String<> cmd,
                                           L4::Ipc::Out<L4::Cap<L4Re::Dataspace> > ds));
    L4_INLINE_RPC(long, c_get_keystate, (long key, long *res));

    typedef L4::Typeid::Rpcs<c_cmd_t, c_cmd_req_t, c_vscreen_get_fb_t, c_get_keystate_t> Rpcs;
  };
};
#endif


#endif /* __DOPE_INCLUDE_DOPEDEF_H_ */
