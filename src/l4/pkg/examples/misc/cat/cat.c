/**
 * \file
 * \brief  A cat.
 *
 * \date
 * \author Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 */
/*
 * (c) 2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <stdio.h>

int main(int argc, char **argv)
{
  int i;
  FILE *fp;
  char buf[1024];
  size_t r;

  for (i = 1; i < argc; ++i)
    {
      fp = fopen(argv[i], "r");
      if (!fp)
        {
          perror(argv[i]);
          continue;
        }

      while ((r = fread(buf, 1, sizeof(buf), fp)))
        fwrite(buf, 1, r, stdout);

      fclose(fp);
    }

  return 0;
}
