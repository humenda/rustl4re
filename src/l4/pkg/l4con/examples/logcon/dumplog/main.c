
#include <stdio.h>
#include <l4/sys/syscalls.h>
#include <l4/names/libnames.h>
#include <l4/log/l4log.h>
#include <l4/dm_generic/consts.h>
#include <l4/dm_generic/dm_generic.h>
#include <l4/util/l4_macros.h>
#include "logcon-client.h"

#define LOG_NAMESERVER_NAME "stdlogV05"

typedef char l4_page_t[L4_PAGESIZE];
static  l4_page_t map_area __attribute__((aligned(L4_PAGESIZE)));
static  l4_addr_t map_addr = (l4_addr_t)map_area;
static  char buffer[L4_SUPERPAGESIZE];

int main(int argc, const char **argv)
{
  DICE_DECLARE_ENV(env);
  l4dm_dataspace_t ds;
  l4_threadid_t logcon_server;
  l4_threadid_t dm_id = L4DM_DEFAULT_DSM;
  l4_size_t size;
  l4_addr_t fpage_addr;
  l4_size_t fpage_size;
  l4_offs_t offs;
  int error;

  if (!names_waitfor_name(LOG_NAMESERVER_NAME, &logcon_server, 10))
    {
      printf("logserver not found\n");
      return 1;
    }

  if ((error = logcon_get_buffer_call(&logcon_server, &dm_id,
                                      &ds, &size, &env)))
    {
      printf("Error %d retrieving dataspace\n", error);
      return 1;
    }

  for (offs = 0; offs < sizeof(buffer) && offs < size; offs += L4_PAGESIZE)
    {
      l4_fpage_unmap(l4_fpage(map_addr, L4_LOG2_PAGESIZE, 0, 0),
                     L4_FP_FLUSH_PAGE | L4_FP_ALL_SPACES);
      if ((error = l4dm_map_pages(&ds, offs, L4_PAGESIZE,
                                  map_addr, L4_LOG2_PAGESIZE, 0,
                                  L4DM_RW, &fpage_addr, &fpage_size)))
        {
          printf("Error %d requesting ds %d offs %08lx at ds_manager "
                  l4util_idfmt"\n", error, ds.id, offs,
                  l4util_idstr(ds.manager));
          return 1;
        }
      /* copy from mapped 'L4' page into our buffer */
      memcpy(buffer + offs, map_area, L4_PAGESIZE);
    }

  printf("%s\n", buffer);

  l4dm_close(&ds);
  return 0;
}
