#include <backtrace.h>
#include <l4/libbacktrace/backtrace_util.h>

#include <stdio.h>

#include <cxxabi.h>
#include <stdlib.h>
#include <errno.h>

static char *demangle(const char *function)
{
  int status = -1;
  char *demfunc = 0;
  demfunc = __cxa_demangle(function, 0, 0, &status);
  if (status == 0)
    return demfunc;
  return NULL;
}

static int
bt_cb_one(void *vdata, uintptr_t pc,
          const char *filename, int lineno, const char *function)
{
  int *cnt = (int *)vdata;

  char *demfunc = demangle(function);

  printf("#%d 0x%08lx: %s%s", *cnt, (unsigned long)pc,
                              function ? (demfunc ? demfunc : function)
                                       : "unknown",
                              function && !demfunc ? "()" : "");
  if (filename || lineno)
    printf(" at %s:%d", filename, lineno);
  printf("\n");
  (*cnt)++;
  free(demfunc);
  return 0;
}

static void
bt_err(void *vdata, const char *msg, int errnum)
{
  int *cnt = (int *)vdata;
  printf("#%d backtrace-error: %s (%d)\n", *cnt, msg, errnum);
}

void libbacktrace_do_local_backtrace(void)
{
  struct backtrace_state *state;

  state = backtrace_create_state(program_invocation_name, 0, bt_err, NULL);

  if (!state)
    {
      printf("L4Re: Backtrace state allocation failed\n");
      return;
    }

  int cnt = 0;
  int r = backtrace_full(state, 0, bt_cb_one, bt_err, &cnt);

  if (r)
    {
      printf("backtrace result: %d\n", r);
      backtrace_print(state, 0, stdout);
    }
}
