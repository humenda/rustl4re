#include <sys/types.h>
#include <sys/systm.h>
#include <machine/stdarg.h>

#include <l4/dde/ddekit/panic.h>

const char *panicstr;

void bsd_panic(const char *fmt, ...) {
	va_list va;

	panicstr=fmt;

	va_start(va, fmt);
	vprintf(fmt, va);
	va_end(va);
	printf("\n");

	ddekit_panic("FreeBSD PANIC");
}

