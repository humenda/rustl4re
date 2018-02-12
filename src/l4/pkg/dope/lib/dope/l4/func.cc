/*** GENERAL INCLUDES ***/
#include <stdio.h>
#include <stdarg.h>

/*** LOCAL INCLUDES ***/
#include "dopelib.h"
#include "dopestd.h"
#include "sync.h"
#include "init.h"

#include <l4/dope/dopedef.h>
#include <l4/dope/vscreen.h>
#include <l4/re/dataspace>
#include <l4/re/util/cap_alloc>
#include <l4/re/env>

extern l4_cap_idx_t dope_server;

int dope_req(char *res, unsigned long res_max, const char *cmd)
{
  if (!cmd || l4_is_invalid_cap(dope_server))
    return -1;

  L4::Cap<Dope::Dope> svr(dope_server);
  L4::Ipc::String<char> r(res_max, res);
  int e = svr->c_cmd_req(cmd, &r);
  res[res_max] = 0;
  return e;
}


int dope_reqf(char *res, unsigned res_max, const char *format, ...)
{
  int ret;
  va_list list;
  static char cmdstr[1024];

  dopelib_mutex_lock(dopelib_cmdf_mutex);
  va_start(list, format);
  vsnprintf(cmdstr, 1024, format, list);
  va_end(list);
  ret = dope_req(res, res_max, cmdstr);
  dopelib_mutex_unlock(dopelib_cmdf_mutex);

  return ret;
}

int dope_cmd(const char *cmd)
{
  if (!cmd || l4_is_invalid_cap(dope_server))
    return -1;

  L4::Cap<Dope::Dope> svr(dope_server);
  return svr->c_cmd(cmd);
}

int dope_cmdf(const char *format, ...)
{
  int ret;
  va_list list;
  static char cmdstr[1024];

  dopelib_mutex_lock(dopelib_cmdf_mutex);
  va_start(list, format);
  vsnprintf(cmdstr, 1024, format, list);
  va_end(list);
  ret = dope_cmd(cmdstr);
  dopelib_mutex_unlock(dopelib_cmdf_mutex);

  return ret;
}

void *dope_vscr_get_fb(const char *s)
{
  L4::Cap<Dope::Dope> svr(dope_server);
  L4::Cap<L4Re::Dataspace> ds;
  ds = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();

  int r = svr->c_vscreen_get_fb(s, ds);

  if (r < 0)
    return 0;

  long sz = ds->size();
  if (sz < 0)
    return 0;

  void *addr = 0;
  if (L4Re::Env::env()->rm()->attach(&addr, sz, L4Re::Rm::Search_addr, ds))
    return 0;

  return addr;
}

#include <l4/util/util.h>
void vscr_server_waitsync(void *x)
{
  (void)x;
  // XXX: ohlala, not gooddy, the old code blocked on the server here, we
  // probably want something eventy here
  l4_sleep(100);
}

long dope_get_keystate(long keycode)
{
  L4::Cap<Dope::Dope> svr(dope_server);
  int r = svr->c_get_keystate(keycode, &keycode);
  if (r < 0)
    return 0;

  return keycode;
}

#if 0
char dope_get_ascii(long id, long keycode) {
	DICE_DECLARE_ENV(_env);
	return dope_manager_get_ascii_call(dope_server, keycode, &_env);
}
#endif
