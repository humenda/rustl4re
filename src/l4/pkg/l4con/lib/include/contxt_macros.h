#ifndef _CONTXT_MACROS_H
#define _CONTXT_MACROS_H

/* L4 includes */
#include <l4/sys/kdebug.h>

/* print error message and panic */
#define PANIC(format, args...)                          \
  do {                                                  \
    LOGl(format"\n", ## args);                              \
    enter_kdebug("panic");                              \
  } while(0)

#endif /* !_CONTXT_MACROS_H */
