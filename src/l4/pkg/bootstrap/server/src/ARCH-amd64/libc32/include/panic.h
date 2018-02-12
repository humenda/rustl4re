#ifndef PANIC_H
#define PANIC_H

#include <cdefs.h>

__BEGIN_DECLS

void panic (const char *format, ...) __attribute__ ((__noreturn__));

__END_DECLS

#endif
