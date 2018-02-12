/*
 * \brief   General widget data structures
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

#ifndef _DOPE_WIDGET_DATA_H_
#define _DOPE_WIDGET_DATA_H_


/*** widget property and state flags ***/

#define WID_FLAGS_STATE      0x0001   /* widget is selected                  */
#define WID_FLAGS_MFOCUS     0x0002   /* widget has mouse focus              */
#define WID_FLAGS_KFOCUS     0x0004   /* widget has keyboard focus           */
#define WID_FLAGS_EVFORWARD  0x0008   /* pass unhandled events to the parent */
#define WID_FLAGS_HIGHLIGHT  0x0010   /* widget reacts on mouse focus        */
#define WID_FLAGS_EDITABLE   0x0020   /* widget interprets key strokes       */
#define WID_FLAGS_CONCEALING 0x0040   /* widget covers its area completely   */
#define WID_FLAGS_SELECTABLE 0x0080   /* widget is selectable via keyboard   */
#define WID_FLAGS_TAKEFOCUS  0x0100   /* widget can receive keyboard focus   */
#define WID_FLAGS_GRABFOCUS  0x0200   /* prevent keyboard focus to switch    */

/*** widget update flags ***/

#define WID_UPDATE_SIZE     0x0001
#define WID_UPDATE_MINMAX   0x0002
#define WID_UPDATE_NEWCHILD 0x0004
#define WID_UPDATE_REFRESH  0x0008

struct binding;
struct binding {
	unsigned long    msg;       /* associated message  */
	char            *bind_ident;/* action event string */
	struct binding  *next;      /* next binding        */
	int              ev_value;
	s16              ev_type;   /* event type          */
	char             ev_has_value;
};


struct new_binding;
struct new_binding {
	int    app_id;       /* application to receive the event */
	char  *name;         /* name of binding                  */
	char **args;         /* arguments to pass with the event */
	char **cond_names;   /* conditions to be checked         */
	char **cond_values;  /* desired values of the conditions */
	struct new_binding *next;
};


struct widget_data {
	long    x, y, w, h;         /* current relative position and size  */
	long    min_w, min_h;       /* minimal size                        */
	long    max_w, max_h;       /* maximal size                        */
	long    flags;              /* state flags                         */
	long    update;             /* update flags                        */
	void    *context;           /* associated data                     */
	WIDGET  *parent;            /* parent in widget hierarchy          */
	WIDGET  *next;              /* next widget in a connected list     */
	WIDGET  *prev;              /* previous widget in a connected list */
	void    (*click) (void *);  /* event handle routine                */
	long    ref_cnt;            /* reference counter                   */
	s32     app_id;             /* application that owns the widget    */
	struct binding *bindings;   /* event bindings                      */
	struct new_binding *new_bindings;
};


#endif /* _DOPE_WIDGET_DATA_H_ */
