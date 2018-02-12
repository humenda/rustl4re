/*!
 * \file	l4con/include/l4con.h
 * \brief	console protocol definitions.
 *
 * \date	2001
 * \author	Christian Helmuth <ch12@os.inf.tu-dresden.de>
 *
 * These macros are used as parameters for the IDL functions. */
/*
 * (c) 2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#ifndef _L4CON_L4CON_H
#define _L4CON_L4CON_H

#include <l4/sys/ipc.h>

/*!\name Query names for this
 */
//@{
#define CON_NAMES_STR	"con"		//!< names string
//@}

/*****************************************************************************/
/*!\name Param macros for virtual framebuffer use
 */
/*****************************************************************************/
//@{
#define CON_NOVFB	0x00   /*!< dont use vfb */
#define CON_VFB		0x01   /*!< use vfb */
//@}

/******************************************************************************/
/*!\name Param macros for smode/gmode
 *
 * Virtual console modes
 ******************************************************************************/
//@{
#define	CON_CLOSED	0x00	//!< console closed
#define	CON_IN		0x01	//!< console in INPUT mode
#define	CON_OUT		0x02	//!< console in OUTPUT mode
#define	CON_INOUT	0x03	//!< 0x01 & 0x02 INOUT mode
#define	CON_OPENING	0x04	//!< requested but not opened
#define	CON_CLOSING	0x08	//!< close requested but not closed
#define CON_MASTER	0x10	//!< console is master
//@}

/******************************************************************************/
/*!\name Param macros for pslim_*
 *
 * pSLIM specific
 ******************************************************************************/
//@{
#include <l4/l4con/l4con_pslim.h>
//@}

/******************************************************************************/
/*!\name Param macros for ev_*
 *
 * EV specific
 ******************************************************************************/
//@{
#include <l4/l4con/l4con_ev.h>
//@}
/******************************************************************************/
/*!\name Param macros for graph_smode/gmode
 *
 * virtual console graphics modes
 ******************************************************************************/
//@{
#define	GRAPH_RESMASK	0xf0	//!< resolution of the frame buffer
#define	GRAPH_BPPMASK	0x0f	//!< bit per pixel value

#define	GRAPH_BPP_16	0x05	//!< 16 bpp graphics

#define	GRAPH_BPP_1	0x01	//!<  1 bpp graphics
#define	GRAPH_BPP_4	0x02	//!<  4 bpp graphics
#define	GRAPH_BPP_8	0x03	//!<  8 bpp graphics
#define	GRAPH_BPP_15	0x04	//!< 15 bpp graphics
#define	GRAPH_BPP_24	0x06	//!< 24 bpp graphics
#define	GRAPH_BPP_32	0x07	//!< 24 bpp word aligned

#define	GRAPH_RES_640	0x10	//!<  640 x  480 pixel

#define	GRAPH_RES_800	0x20	//!<  800 x  600 pixel
#define	GRAPH_RES_1024	0x30	//!< 1024 x  768 pixel
#define	GRAPH_RES_1152	0x40	//!< 1152 x  864 pixel
#define	GRAPH_RES_1280	0x50	//!< 1280 x 1024 pixel
#define	GRAPH_RES_1600	0x60	//!< 1600 x 1200 pixel

#define	GRAPH_RES_768	0x70	//!<  768 x  576 pixel - TV

#define L4CON_FAST_COPY		0x00000001
#define L4CON_STREAM_CSCS_YV12	0x00000002
#define L4CON_STREAM_CSCS_YUY2	0x00000004
#define L4CON_POST_DIRTY	0x00000008
#define L4CON_FAST_FILL		0x00000010

//@}

/******************************************************************************/
/*!\name Error code macros
 ******************************************************************************/
//@{
#define	CON_EPROTO	0x00ff	//!< wrong protocol type
#define	CON_EUNKNOWN	0x00fe	//!< unknown command
#define	CON_EFREE	0x00fd	//!< no free console
#define	CON_ETHREAD	0x00fc	//!< no thread (con_msg_t) declared
#define	CON_EPARAM	0x00fb	//!< error in param (con_cmd_t)
#define	CON_EXPARAM	0x00fa	//!< error in xparam
#define	CON_EPERM	0x00f9	//!< operation not permitted (in current mode)
#define	CON_ENOMEM	0x00f8	//!< not enough memory available
#define	CON_ESCALE	0x00f7	//!< cscs.scale == 0

#define	CON_ENOTIMPL	0x1000	//!< function not implemented yet
//@}


#ifdef __cplusplus

#include <l4/sys/capability>
#include <l4/re/console>
#include <l4/sys/cxx/ipc_iface>

class L4con : public L4::Kobject_t<L4con, L4Re::Console, 0x4100>
{
public:
  L4_INLINE_RPC(long, close, ());

  L4_INLINE_RPC(long, pslim_fill, (int x, int y, int w, int h,
                                   l4con_pslim_color_t color));

  L4_INLINE_RPC(long, pslim_copy, (int x, int y, int w, int h,
                                   l4_int16_t dx, l4_int16_t dy));

  L4_INLINE_RPC(long, puts, (short x, short y, l4con_pslim_color_t fg_color,
                             l4con_pslim_color_t bg_color,
                             L4::Ipc::String<char> text));

  L4_INLINE_RPC(long, puts_scale, (short x, short y,
                                   l4con_pslim_color_t fg_color,
                                   l4con_pslim_color_t bg_color,
                                   short scale_x, short scale_y,
                                   L4::Ipc::String<char> text));
  L4_INLINE_RPC(long, get_font_size, (short *w, short *h));


  typedef L4::Typeid::Rpcs<close_t, pslim_fill_t, pslim_copy_t,
                           puts_t, puts_scale_t, get_font_size_t> Rpcs;
};

#endif

#endif /* !_L4CON_L4CON_H */
