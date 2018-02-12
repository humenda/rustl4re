#ifndef __CONTXT_LIB_INTERNAL_H__
#define __CONTXT_LIB_INTERNAL_H__

#include <l4/sys/kdebug.h>
#include <l4/log/l4log.h>
#include <l4/semaphore/semaphore.h>

#include <l4/l4con/l4con-client.h>
#include <l4/l4con/l4contxt.h>

#include "contxt_macros.h"

#define SUP	0x00
#define SDOWN	0x01

#define SHOW	0x00
#define HIDE	0x02

#define CONTXT_MAXSIZE_EDITBUF    1024 /* max input buffer size */
#define CONTXT_LINEBUF_SIZE       256
#define CONTXT_KEYLIST_SIZE       15   /* key buffer size */

/* color definition */
extern l4con_pslim_color_t	bg_color;
extern l4con_pslim_color_t	fg_color;

#define OFS_LINES(x)	(((x)+sb_lines) % sb_lines)
#define BITX(x)		((x) * fn_x)
#define BITY(y)		((y) * fn_y)

#define CONTXT_TIMEOUT    10000

extern int vtc_cols, vtc_lines;
extern int sb_x, sb_y;		/* scrbuf position */
extern int sb_lines;		/* size of screen buffer */
extern int bob;			/* begin of buffer */
extern int fline;		/* first visible line */
extern int scrpos_y;		/* screen y position */
extern l4_uint32_t fn_x, fn_y;	/* font size x, y */

extern char       *vtc_scrbuf;
extern l4_uint8_t *editbuf;

extern l4_threadid_t evh_l4id;
extern l4_threadid_t vtc_l4id;

extern int __init;
extern int __cvisible;
extern int keylist_head, keylist_tail;
extern int keylist[CONTXT_KEYLIST_SIZE];
extern l4semaphore_t keysem;

extern void _scroll(int, int, int, int);
extern void _redraw(void);
extern void _cursor(int);
extern void _flush(l4_uint8_t*, int, int);

extern int  contxt_write(const char*, int);
extern void (*putstocon)(int, int, const char*, int);

#endif

