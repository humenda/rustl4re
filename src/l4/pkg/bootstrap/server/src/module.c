/*
 * (c) 2008-2009 Frank Mehnert <fm3@os.inf.tu-dresden.de>,
 *               Torsten Frenzel <frenzel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <stdio.h>
#include "module.h"

void
print_module_name(const char *name, const char *alt_name)
{
  const char *c1, *c2;

  if (!name)
    {
      printf("%s", alt_name);
      return;
    }

  c2 = name;
  while (*c2 != '\0' && *c2 != ' ')
    c2++;
  c1 = c2;
  if (c1 > name)
    c1--;
  while (c1 > name && c2-c1 < 56)
    c1--;

  printf("%.*s", (unsigned)(c2-c1), c1);
}
