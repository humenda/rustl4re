/*
 * This file is part of DDEKit.
 *
 * (c) 2013 Maksym Planeta <mcsim.planeta@gmail.com>
 *     economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/dde/ddekit/thread.h>
#include <l4/dde/ddekit/condvar.h>
#include <l4/dde/ddekit/panic.h>
#include <l4/dde/ddekit/assert.h>
#include <l4/dde/ddekit/memory.h>
#include <l4/dde/ddekit/block.h>
#include "config.h"
#include <l4/dde/ddekit/printf.h>

#include <l4/dde/dde.h>
#include <l4/log/log.h>
#include <l4/util/util.h>
#include <l4/sys/thread.h>
#include <l4/sys/debugger.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread-l4.h>

struct dde_disk
{
  char * name;
  void * private_data;
};

static struct dde_disk *disk_array;
static int dde_disk_count = 0;

void ddekit_add_disk(char *name, void *private_data)
{
  struct dde_disk * new_array;
  new_array = realloc (disk_array, sizeof (*disk_array) * (dde_disk_count + 1));
  if (!new_array)
    {
      /* Request failed */
      ddekit_printf ("No memory for new disk\n");
      return;
    }

  disk_array = new_array;

  disk_array[dde_disk_count].name = name;
  disk_array[dde_disk_count].private_data = private_data;
  ddekit_printf ("Added new disk %d name: %s data: %p\n", dde_disk_count,
                 disk_array[dde_disk_count].name, disk_array[dde_disk_count].private_data);
  dde_disk_count++;
}

void* ddekit_find_disk(const char* name)
{
  struct dde_disk *disk = NULL;
  int i;

  for (i = 0; i < dde_disk_count; ++i)
    {
      if (!strcmp(disk_array[i].name, name))
        {
          disk = &disk_array[i];
          break;
        }
    }

  if (!disk)
    {
      ddekit_printf("disk <%s> not found\n", name);
      return NULL;
    }

  return disk->private_data;
}
