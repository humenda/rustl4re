/* L4 includes */
#include <stdio.h>
#include <l4/thread/thread.h>
#include <l4/input/libinput.h>
#include <l4/l4con/stream-server.h>

#include "internal.h"
#include "evh.h"

int __shift = 0;	/* 1: shift key pressed */
int __ctrl  = 0;	/* 1: ctrl key pressed */
int __keyin = 0;	/* 1: at least one key was read */

l4_threadid_t evh_l4id;	/* L4 thread id of key event handler */

static l4_threadid_t btn_l4id; /* L4 thread id for the key repeat thread */

static void evh_loop(void *data);
static void btn_repeat(void *data);
static unsigned key_code, key_repeat, key_pending;

l4semaphore_t keysem = L4SEMAPHORE_LOCKED;

static void
_add_key(int code)
{
  int new_top = (keylist_head + 1) % CONTXT_KEYLIST_SIZE;

  if (new_top == keylist_tail)
    {
      if (__keyin)
	LOG("key buffer overrun!");
      return;
    }

  keylist[keylist_head] = code;
  keylist_head = new_top;
  l4semaphore_up(&keysem);
}

static inline void
_add_key_if_not_too_busy(int code)
{
  if (keysem.counter < 2)
    _add_key(code);
}

static void
_key_scroll(int dir, int lines)
{
  int cv = __cvisible;
  if (cv)
    _cursor(HIDE);
  _scroll(0, vtc_lines, dir, lines);
  if (cv)
    _cursor(SHOW);
}

/******************************************************************************
 * evh_init                                                                   *
 *                                                                            *
 * event handler initialization                                               *
 ******************************************************************************/
int
evh_init(void)
{
  l4thread_t evh_tid;
  l4thread_t btn_tid;
  
  /* start thread */
  if ((evh_tid = l4thread_create_long(L4THREAD_INVALID_ID, evh_loop, 
				      ".kbd", L4THREAD_INVALID_SP,
				      L4THREAD_DEFAULT_SIZE,
				      L4THREAD_DEFAULT_PRIO, 
				      0, L4THREAD_CREATE_SYNC)) < 0)
    return evh_tid;

  evh_l4id = l4thread_l4_id(evh_tid);

  if ((btn_tid = l4thread_create_long(L4THREAD_INVALID_ID, btn_repeat,
				      ".kbdrep", L4THREAD_INVALID_SP,
				      L4THREAD_DEFAULT_SIZE,
				      L4THREAD_DEFAULT_PRIO,
				      0, L4THREAD_CREATE_SYNC)) < 0)
    return btn_tid;

  btn_l4id = l4thread_l4_id(btn_tid);

  return 0;
}

static void
touch_repeat(unsigned code, unsigned repeat)
{
  l4_msgdope_t result;
  l4_umword_t dummy;
  int error;

  error = l4_ipc_call(btn_l4id, 
			   L4_IPC_SHORT_MSG, code, repeat,
			   L4_IPC_SHORT_MSG, &dummy, &dummy,
			   L4_IPC_SEND_TIMEOUT_0, &result);
  if (error)
    {
      key_pending = 1;
      key_code = code;
      key_repeat = repeat;
    }
}

/******************************************************************************
 * stream_io_server_push                                                      *
 *                                                                            *
 * in: request	      ... Flick request structure                             *
 *     event          ... incoming event                                      *
 * out: _ev           ... Flick exception (unused)                            *
 * ret:   0           ... success                                             *
 *                                                                            *
 * handle incoming events                                                     *
 *****************************************************************************/
void 
stream_io_push_component(CORBA_Object _dice_corba_obj,
    const stream_io_input_event_t *event,
    CORBA_Server_Environment *_dice_corba_env)
{
  stream_io_input_event_t const *input_ev = event;

  switch(input_ev->type)
    {
    case EV_KEY:
      switch(input_ev->code)
	{
	case KEY_RIGHTSHIFT:
	case KEY_LEFTSHIFT:
	  if (input_ev->value)
	    __shift = 1;
	  else
	    __shift = 0;
	  touch_repeat(input_ev->code, 0);
	  return;
	case KEY_LEFTCTRL:
	case KEY_RIGHTCTRL:
	  if (input_ev->value)
	    __ctrl  = 1;
	  else
	    __ctrl  = 0;
	  touch_repeat(input_ev->code, 0);
	  return;
	case KEY_UP:
	  if(input_ev->value && __shift)
	    {
	      _key_scroll(SDOWN, 1);
	      touch_repeat(input_ev->code, input_ev->value);
	      return;
	    }
	  break;
	case KEY_PAGEUP:
	  if(input_ev->value && __shift)
	    {
	      _key_scroll(SDOWN, 20);
	      touch_repeat(input_ev->code, input_ev->value);
	      return;
	    }
	  break;
	case KEY_DOWN:
	  if(input_ev->value && __shift)
	    {
	      _key_scroll(SUP, 1);
	      touch_repeat(input_ev->code, input_ev->value);
	      return;
	    }
	  break;
	case KEY_PAGEDOWN:
	  if(input_ev->value && __shift)
	    {
	      _key_scroll(SUP, 20);
	      touch_repeat(input_ev->code, input_ev->value);
	      return;
	    }
	  break;
	}
      /* ignore mouse buttons */
      if(input_ev->code>=BTN_MOUSE && input_ev->code<=BTN_TASK)
	break;
      if(input_ev->value)
	{
	  int gap = OFS_LINES(sb_y-fline);
	  if (gap)
	    _key_scroll(SUP, gap);
	  _add_key(input_ev->code);
	}
      touch_repeat(input_ev->code, input_ev->value);
      break;
    
    case EV_CON:
      switch(input_ev->code) 
	{
	case EV_CON_REDRAW:
	  if (__init)
	    _redraw();
	  break;
	}
    }
}

/******************************************************************************
 * evh_loop                                                                   *
 *                                                                            *
 * evh - IDL server loop                                                      *
 ******************************************************************************/
static void
evh_loop(void *data)
{
  l4thread_started(NULL);
  stream_io_server_loop(data);
}

static void
btn_repeat(void *data)
{
  l4thread_started(NULL);
  
  for (;;)
    {
      l4_umword_t code, new_code, repeat, new_repeat;
      l4_msgdope_t result;
      int error;

      error = l4_ipc_receive(evh_l4id, 
				  L4_IPC_SHORT_MSG, &code, &repeat,
				  L4_IPC_NEVER, &result);
      error = l4_ipc_send   (evh_l4id,
				  L4_IPC_SHORT_MSG, 0, 0,
				  L4_IPC_NEVER, &result);
      if (!repeat)
	continue;

      for (;;)
	{
	  /* wait for around 250ms */
	  error = l4_ipc_receive(evh_l4id,
				      L4_IPC_SHORT_MSG, &new_code, &new_repeat,
				      l4_ipc_timeout(0,0,976,8), &result);
	  if (error == L4_IPC_RETIMEOUT && !key_pending)
	    {
	      /* no new key in the meantime -- start repeat.
	       * wait for round 30ms */
	      for (;;)
		{
		  if (__shift)
		    {
		      switch (code)
			{
			case KEY_UP:
			  _key_scroll(SDOWN, 1);
			  break;
			case KEY_PAGEUP:
			  _key_scroll(SDOWN, 20);
			  break;
			case KEY_DOWN:
			  _key_scroll(SUP, 1);
			  break;
			case KEY_PAGEDOWN:
			  _key_scroll(SUP, 20);
			  break;
			default:
			  _add_key_if_not_too_busy(code);
			}
		    }
		  else
		    {
		      /* send key */
		      _add_key_if_not_too_busy(code);
		    }

		  /* wait for key up or other key down */
		  error = l4_ipc_receive(evh_l4id,
					      L4_IPC_SHORT_MSG, 
					        &new_code, &repeat,
					      l4_ipc_timeout(0,0,546,6),
					      &result);
		  if (error != L4_IPC_RETIMEOUT || key_pending)
		    {
		      /* new key or key_up received -- break repeat.
		       * tricky: fall through until next send */
		      break;
		    }
		}
	    }

	  if (error == 0)
	    {
	      code   = new_code;
    	      repeat = new_repeat;

	      /* new key or key_up received -- only reply, do not repeat */
	      error = l4_ipc_send(evh_l4id,
				       L4_IPC_SHORT_MSG, 0, 0,
				       L4_IPC_NEVER, &result);
	      if (repeat)
		continue;

	      break;
	    }
	  else if (key_pending)
	    {
	      code   = key_code;
	      repeat = key_repeat;
	      key_pending = 0;

	      if (repeat)
		continue;

	      break;
	    }
	  else
	    {
	      /* ??? */
	      LOG("btn_repeat: ipc error %02x", error);
	      break;
	    }
	}
    }
}

