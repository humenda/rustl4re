
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>

#include "koptions.h"

static struct {
  const char *s;
  unsigned int flag;
} boolean_opts[] = {
  { " -wait",              L4_kernel_options::F_wait              },
  { " -serial_esc",        L4_kernel_options::F_serial_esc        },
  { " -noserial",          L4_kernel_options::F_noserial          },
  { " -noscreen",          L4_kernel_options::F_noscreen          },
  { " -esc",               L4_kernel_options::F_esc               },
  { " -nojdb",             L4_kernel_options::F_nojdb             },
  { " -nokdb",             L4_kernel_options::F_nokdb             },
  { " -nohlt",             L4_kernel_options::F_nohlt             },
  { " -apic",              L4_kernel_options::F_apic              },
  { " -loadcnt",           L4_kernel_options::F_loadcnt           },
  { " -watchdog",          L4_kernel_options::F_watchdog          },
  { " -irq0",              L4_kernel_options::F_irq0              },
  { " -nosfn",             L4_kernel_options::F_nosfn             },
  { " -jdb_never_stop",    L4_kernel_options::F_jdb_never_stop    },
};

static struct {
  const char *s;
  unsigned int flag;
  unsigned int offset;
} unsigned_opts[] = {
  { " -kmemsize",     L4_kernel_options::F_kmemsize,
                      offsetof(L4_kernel_options::Options, kmemsize) },
  { " -tbuf_entries", L4_kernel_options::F_tbuf_entries,
                      offsetof(L4_kernel_options::Options, tbuf_entries) },
  { " -out_buf",      L4_kernel_options::F_out_buf,
                      offsetof(L4_kernel_options::Options, out_buf) },
};

#define MEMBERSIZE(type, member) sizeof(((type *)0)->member)

static struct {
  const char *s;
  unsigned int flag;
  unsigned int size;
  unsigned int offset;
} string_opts[] = {
  { " -jdb_cmd",      L4_kernel_options::F_jdb_cmd,
                      MEMBERSIZE(L4_kernel_options::Options, jdb_cmd),
                      offsetof(L4_kernel_options::Options, jdb_cmd) },
};

static void kcmdline_show(L4_kernel_options::Options *lko)
{
  printf("Location: %016lx  Size: %zd Bytes\n",
         (unsigned long)lko, sizeof(*lko));
  printf("Flags: %08x\n", lko->flags);
  for (unsigned i = 0; i < sizeof(boolean_opts) / sizeof(boolean_opts[0]); ++i)
    {
      if (lko->flags & boolean_opts[i].flag)
        printf("  %s\n", boolean_opts[i].s);
    }

  if (lko->flags & L4_kernel_options::F_kmemsize)
    printf("Kmemsize: %dKiB\n", lko->kmemsize);
  if (lko->flags & L4_kernel_options::F_tbuf_entries)
    printf("Tbuf entries: %d\n", lko->tbuf_entries);
  if (lko->flags & L4_kernel_options::F_out_buf)
    printf("Out bufs: %d\n", lko->out_buf);

  if (lko->jdb_cmd[0])
    printf("JDB command: %s\n", lko->jdb_cmd);
}

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

void kcmdline_parse(char const *cmdline,
                    L4_kernel_options::Options *lko)
{
  if (0)
    printf("Kernel command-line: %s\n", cmdline);

  // boolean options
  for (unsigned i = 0; i < ARRAY_SIZE(boolean_opts); ++i)
    if (strstr(cmdline, boolean_opts[i].s))
      lko->flags |= boolean_opts[i].flag;

  // integer options
  for (unsigned i = 0; i < ARRAY_SIZE(unsigned_opts); ++i)
    {
      char *c;
      unsigned len = strlen(unsigned_opts[i].s);
      if ((c = strstr(cmdline, unsigned_opts[i].s))
          && (c[len] == ' ' || c[len] == '='))
        {
          lko->flags |= unsigned_opts[i].flag;
          *(unsigned *)((char *)lko + unsigned_opts[i].offset)
            = strtol(c + len + 1, 0, 0);
        }
    }

  // string options
  for (unsigned i = 0; i < ARRAY_SIZE(string_opts); ++i)
    {
      char *c;
      unsigned len = strlen(string_opts[i].s);
      if ((c = strstr(cmdline, string_opts[i].s))
          && (c[len] == ' ' || c[len] == '='))
        {
          char *dst = (char *)lko + string_opts[i].offset;
          lko->flags |= string_opts[i].flag;
          c += len + 1;
          while (*c && *c != ' '
                 && (dst < (char *)lko + string_opts[i].offset +
                            string_opts[i].size - 1))
            *dst++ = *c++;
          *dst = 0;
        }
    }

  // warnings
  if (strstr(cmdline, "-comport")
      || strstr(cmdline, "-comspeed")
      || strstr(cmdline, "-comirq"))
    printf("WARNING: Command line options -comport, -comspeed and -comirq\n"
           "         have been moved to bootstrap and are shared beetween\n"
           "         Fiasco and bootstrap now. Please remove them from\n"
           "         your Fiasco command line, they do not have an effect\n"
           "         there.\n");

  if (0)
    kcmdline_show(lko);
}
