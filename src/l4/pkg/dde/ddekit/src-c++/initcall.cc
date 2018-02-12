#include <l4/dde/ddekit/initcall.h>
#include <l4/dde/dde.h>
#include <l4/dde/ddekit/printf.h>
#include <l4/sys/kdebug.h>

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

namespace DDEKit
{
/* Stolen from pkg/crtn/lib/src/construction.c */
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


/* Stolen from pkg/crtn/lib/src/construction.c */
void construction()
{
	run_hooks_forward(__DDEKIT_CTOR_BEG__, "__DDEKIT_CTOR_BEG__");
}
}
