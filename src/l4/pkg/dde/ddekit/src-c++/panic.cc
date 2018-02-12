#include <l4/dde/ddekit/panic.h>
#include <l4/dde/ddekit/printf.h>

#include <l4/sys/kdebug.h>
#include <stdarg.h>

void DDEKit::panic(const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
    outstring("\033[31;1m");
	DDEKit::vprintf(fmt, va);
    outstring("\033[0m");
	va_end(va);
	DDEKit::printf("\n");

	while (1)
		enter_kdebug("ddekit_panic()");
}

void DDEKit::debug(const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	DDEKit::vprintf(fmt, va);
	va_end(va);
	DDEKit::printf("\n");

	enter_kdebug("ddekit_debug()");
}

