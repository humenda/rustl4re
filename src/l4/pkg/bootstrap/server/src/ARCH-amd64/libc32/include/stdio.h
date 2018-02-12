#ifndef _STDIO_H
#define _STDIO_H

#include <cdefs.h>
#include <stddef.h>

__BEGIN_DECLS

int putchar(int c);
int puts(const char *s);
int printf(const char *format, ...) __attribute__((format(printf,1,2)));
int sprintf(char *str, const char *format, ...) __attribute__((format(printf,2,3)));
int snprintf(char *str, size_t size, const char *format, ...) __attribute__((format(printf,3,4)));
int asprintf(char **ptr, const char* format, ...) __attribute__((format(printf,2,3)));

#if 0
int scanf(const char *format, ...) __attribute__((format(scanf,1,2)));
int sscanf(const char *str, const char *format, ...) __attribute__((format(scanf,2,3)));
#endif

#include <stdarg.h>

int vprintf(const char *format, va_list ap) __attribute__((format(printf,1,0)));
int vsprintf(char *str, const char *format, va_list ap) __attribute__((format(printf,2,0)));
int vsnprintf(char *str, size_t size, const char *format, va_list ap) __attribute__((format(printf,3,0)));


typedef int FILE;

int vscanf(const char *format, va_list ap) __attribute__((format(scanf,1,0)));
int vsscanf(const char *str, const char *format, va_list ap) __attribute__((format(scanf,2,0)));

int sscanf(const char *str, const char *format, ...);

__END_DECLS

#endif
