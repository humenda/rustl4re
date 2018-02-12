/*
 * This file is part of DDEKit.
 *
 * (c) 2006-2012 Bjoern Doebel <doebel@os.inf.tu-dresden.de>
 *               Christian Helmuth <ch12@os.inf.tu-dresden.de>
 *               Thomas Friebel <tf13@os.inf.tu-dresden.de>
 *     economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/dde/ddekit/initcall.h>

#if (__GNUC__ == 3 && __GNUC_MINOR__ >= 3) || __GNUC__ >= 4
#define SECTION(x)	__attribute__((used, section( x )))
#else
#define SECTION(x)	__attribute__((section( x )))
#endif

#define BEG		{ (ctor_hook) ~1UL }
#define END		{ (ctor_hook)  0UL }

typedef void (*const ctor_hook)(void);

static ctor_hook const __DDEKIT_CTOR_BEG__[1] SECTION(".mark_beg_dde_ctors") = BEG;
static ctor_hook const __DDEKIT_CTOR_END__[1] SECTION(".mark_end_dde_ctors") = END;

static void run_hooks_forward(ctor_hook *list, const char *name)
{
#ifdef DEBUG
  outstring("list (forward) ");
  outstring(name);
  outstring(" @ ");
  outhex32((unsigned)list);
  outchar('\n');
#endif
  list++;
  while (*list)
    {
#ifdef DEBUG
      outstring("  calling ");
      outhex32((unsigned)*list);
      outchar('\n');
#endif
      (**list)();
      list++;
    }
}


void ddekit_do_initcalls(void) 
{
	run_hooks_forward(__DDEKIT_CTOR_BEG__, "__DDEKIT_CTOR_BEG__");
}
