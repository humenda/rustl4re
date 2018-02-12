#include <stdio.h>
#include <string.h>

#include <l4/dm_mem/dm_mem.h>
#include <l4/env/errno.h>
#include <l4/l4con/l4contxt.h>
#include <l4/log/l4log.h>
#include <l4/names/libnames.h>
#include <l4/sys/kdebug.h>
#include <l4/thread/thread.h>
#include <l4/util/l4_macros.h>
#include "logcon-server.h"

#define LOG_BUFFERSIZE 81
#define LOG_NAMESERVER_NAME "stdlogV05"
#define LOG_COMMAND_LOG 0

char LOG_tag[9] = "logcon";
l4_ssize_t l4libc_heapsize = 16*1024;

static char message_buffer[LOG_BUFFERSIZE+5];
static volatile int init_status;
static char  init_buffer[200*(LOG_BUFFERSIZE+5)];
static char *init_buffer_beg = init_buffer;
static char *init_buffer_ptr = init_buffer;
static int   init_buffer_len;

//! the sender of the last request
l4_threadid_t msg_sender = L4_INVALID_ID;

extern int console_puts(const char *s);

static void
my_LOG_outstring(const char *s)
{
  console_puts(s);
}

static void*
msg_alloc(unsigned long size)
{
  if (size != LOG_BUFFERSIZE)
    return 0;
  return message_buffer;
}

void
log_outstring_component(CORBA_Object _dice_corba_obj,
                        int flush_flag,
                        const char* string,
                        CORBA_Server_Environment *_dice_corba_env)
{
  outstring(string);
  if (init_status == 0)
    {
      /* console still not initialized, use init_buffer as ring buffer */
      char c;
      for (; (c=*string++);)
	{
	  if (init_buffer_ptr >= init_buffer + sizeof(init_buffer))
	    init_buffer_ptr = init_buffer;
	  *init_buffer_ptr++ = c;
	  if (init_buffer_len < sizeof(init_buffer))
	    init_buffer_len++;
	  else
	    init_buffer_beg = init_buffer_ptr;
	}
      return;
    }

  /* wait until init buffer is written to text console */
  while (init_status == 1)
    l4thread_sleep(100);

  /* print to L4 console */
  printf("%s", string);
}

/** not supported */
int
log_channel_open_component(CORBA_Object _dice_corba_obj,
                           l4_snd_fpage_t page,
                           int channel,
                           CORBA_Server_Environment *_dice_corba_env)
{
  return -L4_EINVAL;
}

/** not supported */
int
log_channel_write_component(CORBA_Object _dice_corba_obj,
                            int channel,
                            unsigned int offset,
                            unsigned int size,
                            CORBA_Server_Environment *_dice_corba_env)
{
  return -L4_EINVAL;
}

/** not supported */
int
log_channel_flush_component(CORBA_Object _dice_corba_obj,
                            int channel,
                            CORBA_Server_Environment *_dice_corba_env)
{
  return -L4_EINVAL;
}

/** not supported */
int
log_channel_close_component(CORBA_Object _dice_corba_obj,
                            int channel,
                            CORBA_Server_Environment *_dice_corba_env)
{
  return -L4_EINVAL;
}

/**
 * Initialization thread. Wait for the console registration at names and copy
 * all previous log messages to our text console.
 */
static void
contxt_init_thread(void *data)
{
  for (;;)
    {
      if (init_status == 0)
	{
	  l4_threadid_t con_id;

	  if (names_query_name(CON_NAMES_STR, &con_id))
	    {
	      int err;

	      if ((err = contxt_init(2048, 10000)))
		printf("Error %d opening console\n", err);
	      else
		{
		  init_status = 1;
		  while (init_buffer_len--)
		    {
		      if (init_buffer_beg >= init_buffer + sizeof(init_buffer))
			init_buffer_beg = init_buffer;
		      putchar(*init_buffer_beg++);
		    }
		  init_status = 2;
		}
              l4thread_exit();
	    }
	}
      l4thread_sleep(50);
    }
}

/**
 * Deliver the text buffer to external applications. Useful to show and
 * save the log buffer from L4Linux.
 */
int
logcon_get_buffer_component(CORBA_Object _dice_corba_obj,
                            const l4_threadid_t *dm_id,
                            l4dm_dataspace_t *ds,
                            l4_size_t *size,
                            CORBA_Server_Environment *_dice_corba_env)
{
  /* a bit hacky: l4con/lib/src-specific */
  extern int sb_lines, sb_y, vtc_cols, bob;
  extern char *vtc_scrbuf;
  int x, y, error;
  void *addr;
  char *src, *dst;

  /* XXX always allocate the maximum size */
  *size = sb_lines * vtc_cols;

  /* open + attach a dataspace */
  if ((error = l4dm_mem_open(*dm_id, *size, 0, 0, "logcon buffer", ds)))
    {
      *size = 0;
      *ds   = L4DM_INVALID_DATASPACE;
      return -L4_ENOMEM;
    }
  if ((error = l4rm_attach(ds, *size, 0, 0, &addr)))
    {
      LOG("Could not attach dataspace");
      l4dm_close(ds);
      *size = 0;
      *ds   = L4DM_INVALID_DATASPACE;
      return -L4_ENOMEM;
    }

  /* convert the whole buffer to <char><char>...\n<char><char>... */
  dst = addr;
  for (y = bob; y != sb_y; y++)
    {
      if (y >= sb_lines)
        y = 0;

      src = vtc_scrbuf + y * vtc_cols;

      for (x = vtc_cols - 1; x >= 0 && src[x] == ' '; x--)
        ;

      memcpy(dst, src, x+1);
      dst += x + 1;
      *dst++ = '\n';
    }

  /* terminate */
  *dst = '\0';

  if ((error = l4dm_transfer(ds, *_dice_corba_obj)))
    {
      LOG("Could not transfer the dataspace to "l4util_idfmt,
          l4util_idstr(*_dice_corba_obj));
      l4dm_close(ds);
      return -L4_EINVAL;
    }

  l4rm_detach(addr);
  return 0;
}

int
main(int argc, char**argv)
{
  CORBA_Server_Environment env = dice_default_server_environment;

  LOG_outstring = my_LOG_outstring;

  if (!names_register(LOG_NAMESERVER_NAME))
    {
      printf("Cannot register at nameserver\n");
      return 1;
    }

  l4thread_create_named(contxt_init_thread, ".coninit",
                        0, L4THREAD_CREATE_ASYNC);

  env.malloc = msg_alloc;
  env.rcv_fpage.fpage = 0;

  strcpy(message_buffer+LOG_BUFFERSIZE,"...\n");
  logcon_server_loop (&env);
}
