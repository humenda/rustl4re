/**
 * \file	con/examples/evdemo/evdemo.c
 * \brief	event distribution demonstration
 *
 * \date	2001
 * \author	Christian Helmuth <fm3@os.inf.tu-dresden.de>
 */

/* (c) 2003 'Technische Universitaet Dresden'
 * This file is part of the con package, which is distributed under
 * the terms of the GNU General Public License 2. Please see the
 * COPYING file for details. */

/* L4 includes */
#include <l4/thread/thread.h>
#include <l4/sys/kdebug.h>
#include <l4/util/util.h>
#include <l4/util/l4_macros.h>
#include <l4/input/libinput.h>

#include <l4/l4con/l4con.h>
#include <l4/l4con/l4con-client.h>
#include <l4/l4con/stream-server.h>

#include <l4/names/libnames.h>
#include <l4/log/l4log.h>

#define PROGTAG		"_evdemo"

#define MY_SBUF_SIZE	4096

/* OSKit includes */
#include <stdio.h>

/* local includes */
#include "util.h"

/* internal prototypes */
void ev_loop(void);

/* global vars */
l4_threadid_t my_l4id,			/* it's me */
	ev_l4id,			/* my event handler */
	con_l4id,			/* con at names */
	vc_l4id;			/* partner VC */

/******************************************************************************
 * stream_io - IDL server function                                            *
 ******************************************************************************/

/* grabbed from evtest.c */
char *keys[KEY_MAX + 1] = { "Reserved", "Esc", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "Minus", "Equal", "Backspace",
"Tab", "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "LeftBrace", "RightBrace", "Enter", "LeftControl", "A", "S", "D", "F", "G",
"H", "J", "K", "L", "Semicolon", "Apostrophe", "Grave", "LeftShift", "BackSlash", "Z", "X", "C", "V", "B", "N", "M", "Comma", "Dot",
"Slash", "RightShift", "KPAsterisk", "LeftAlt", "Space", "CapsLock", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10",
"NumLock", "ScrollLock", "KP7", "KP8", "KP9", "KPMinus", "KP4", "KP5", "KP6", "KPPlus", "KP1", "KP2", "KP3", "KP0", "KPDot", "103rd",
"F13", "102nd", "F11", "F12", "F14", "F15", "F16", "F17", "F18", "F19", "F20", "KPEnter", "RightCtrl", "KPSlash", "SysRq",
"RightAlt", "LineFeed", "Home", "Up", "PageUp", "Left", "Right", "End", "Down", "PageDown", "Insert", "Delete", "Macro", "Mute",
"VolumeDown", "VolumeUp", "Power", "KPEqual", "KPPlusMinus", "Pause", "F21", "F22", "F23", "F24", "KPComma", "LeftMeta", "RightMeta",
"Compose", "Stop", "Again", "Props", "Undo", "Front", "Copy", "Open", "Paste", "Find", "Cut", "Help", "Menu", "Calc", "Setup",
"Sleep", "WakeUp", "File", "SendFile", "DeleteFile", "X-fer", "Prog1", "Prog2", "WWW", "MSDOS", "Coffee", "Direction",
"CycleWindows", "Mail", "Bookmarks", "Computer", "Back", "Forward", "CloseCD", "EjectCD", "EjectCloseCD", "NextSong", "PlayPause",
"PreviousSong", "StopCD", "Record", "Rewind", "Phone", "ISOKey", "Config", "HomePage", "Refresh", "Exit", "Move", "Edit", "ScrollUp",
"ScrollDown", "KPLeftParenthesis", "KPRightParenthesis",
"International1", "International2", "International3", "International4", "International5",
"International6", "International7", "International8", "International9",
"Language1", "Language2", "Language3", "Language4", "Language5", "Language6", "Language7", "Language8", "Language9",
NULL, 
"PlayCD", "PauseCD", "Prog3", "Prog4", "Suspend", "Close",
NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
"Btn0", "Btn1", "Btn2", "Btn3", "Btn4", "Btn5", "Btn6", "Btn7", "Btn8", "Btn9",
NULL, NULL,  NULL, NULL, NULL, NULL,
"LeftBtn", "RightBtn", "MiddleBtn", "SideBtn", "ExtraBtn", "ForwardBtn", "BackBtn",
NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
"Trigger", "ThumbBtn", "ThumbBtn2", "TopBtn", "TopBtn2", "PinkieBtn",
"BaseBtn", "BaseBtn2", "BaseBtn3", "BaseBtn4", "BaseBtn5", "BaseBtn6",
NULL, NULL, NULL, NULL,
"BtnA", "BtnB", "BtnC", "BtnX", "BtnY", "BtnZ", "BtnTL", "BtnTR", "BtnTL2", "BtnTR2", "BtnSelect", "BtnStart", "BtnMode",
NULL, NULL, NULL,
"ToolPen", "ToolRubber", "ToolBrush", "ToolPencil", "ToolAirbrush", "ToolFinger", "ToolMouse", "ToolLens", NULL, NULL,
"Touch", "Stylus", "Stylus2" };

char *pressed = "pressed";
char *released = "released";

/******************************************************************************
 * stream_io_server_push                                                      *
 *                                                                            *
 * in:  request	      ... Flick request structure                             *
 *      event         ... struct describing the event                         *
 * out: _ev           ... Flick exception (unused)                            *
 *                                                                            *
 * push event into con event stream                                           *
 ******************************************************************************/
void 
stream_io_push_component(CORBA_Object _dice_corba_obj,
    const stream_io_input_event_t *event,
    CORBA_Server_Environment *_dice_corba_env)
{
	struct l4input *input_ev = (struct l4input*) event;

	static unsigned short code;
	static unsigned char down = 0;

	static char *value_str;		/* `pressed' or `released' */
	static char *code_str;		/* key description */

	if (input_ev->type != EV_KEY) return;

	/* filter autorepeated keys out --- needed if h/w doesn't support 
	 * disabling (look in server/src/ev.c) */
	if (down && input_ev->value)
		if (code == input_ev->code) return;

	code_str = keys[input_ev->code];
	code = input_ev->code;

	if (input_ev->value) {
		value_str = pressed;
		down = 1;
	}
	else {
		value_str = released;
		down = 0;
	}

	printf("You %s the %s key.\n", value_str, code_str);
}

/******************************************************************************
 * ev_loop                                                                    *
 *                                                                            *
 * event reception loop                                                       *
 ******************************************************************************/
void ev_loop()
{
  l4thread_started(NULL);
  stream_io_server_loop(NULL);
}

/******************************************************************************
 * main                                                                       *
 *                                                                            *
 * Main function                                                              *
 ******************************************************************************/
int main(int argc, char *argv[])
{
	CORBA_Environment _env = dice_default_environment;

	/* init */
	my_l4id = l4thread_l4_id( l4thread_myself() );

	printf("How are you? I'm running as "l4util_idfmt"\n", 
	       l4util_idstr(my_l4id));

	/* ask for 'con' (timeout = 5000 ms) */
	if (names_waitfor_name(CON_NAMES_STR, &con_l4id, 50000) == 0) {
		printf("PANIC: %s not registered at names", CON_NAMES_STR);
		enter_kdebug("panic");
	}

	if (con_if_openqry_call(&con_l4id, MY_SBUF_SIZE, 0, 0, 
			   L4THREAD_DEFAULT_PRIO,
			   &vc_l4id, 
			   CON_VFB,
			   &_env))
		enter_kdebug("Ouch, open vc failed");
	printf("Hey, openqry okay\n");

	/* let's start the event handler */
	ev_l4id = l4thread_l4_id( l4thread_create((l4thread_fn_t) ev_loop,
						  NULL,
						  L4THREAD_CREATE_SYNC) );

	if (con_vc_smode_call(&vc_l4id, CON_INOUT, &ev_l4id, &_env))
		enter_kdebug("Ouch, setup vc failed");
	printf("Cool, smode okay\n");

	/* do something ... */
	for(;;) {}

	if (con_vc_close_call(&vc_l4id, &_env))
		enter_kdebug("Ouch, close vc failed?!");
	printf("Finally closed vc\n");

	printf("Going to bed ...\n");
	l4_sleep(-1);

	exit(0);
}
