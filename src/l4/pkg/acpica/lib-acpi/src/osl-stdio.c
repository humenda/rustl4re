#include "acpi.h"
#include <stdio.h>
#include <stdarg.h>

/*
 * Debug print routines
 */

void ACPI_INTERNAL_VAR_XFACE
AcpiOsPrintf (const char  *format,...)
{
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
	return;
}

void
AcpiOsVprintf (const char  *format, va_list args)
{
	vprintf(format, args);
	return;
}
