#include <valgrind.h>
#include <pub_tool_basics.h>
#include <pub_tool_basics_asm.h>
#include <pub_tool_libcbase.h>
#include <pub_tool_libcprint.h>

#include <l4re_helper.h>
#include <l4/re/env.h>

#include "pub_l4re.h"

void *client_env = 0;
unsigned int client_env_size = 0;

/*************************************
 * Stuff needed for linking Valgrind *
 * against C++-Libs                  *
 * ***********************************/

__extension__ typedef int __guard __attribute__ ((mode(__DI__)));
int __cxa_guard_acquire(__guard * g);
int __cxa_guard_acquire(__guard * g)
{
    return !*(char *) (g);
}

void __cxa_guard_release(__guard * g);
void __cxa_guard_release(__guard * g)
{
    *(char *) g = 1;
}

void __cxa_pure_virtual(void);
void __cxa_pure_virtual(void)
{
    VG_(unimplemented)("unimplemented function __cxa_pure_virtual()");
    while (1);
}

void __gxx_personality_v0(void);
void __gxx_personality_v0(void)
{
    VG_(unimplemented)("unimplemented function __gxx_personality_v0()");
    while (1);
}

void __cxa_call_unexpected(void *ei);
void __cxa_call_unexpected(void *ei)
{
    VG_(unimplemented)("unimplemented function __cxa_call_unexpected()");
    while (1);
}

void _Unwind_Resume(void);
void _Unwind_Resume(void)
{
    VG_(unimplemented)("unimplemented function _Unwind_Resume()");
    while (1);
}


void _Unwind_DeleteException(void);
void _Unwind_DeleteException(void)
{
    VG_(unimplemented)("unimplemented function _Unwind_DeleteException()");
    while (1);
}

void _Unwind_RaiseException(void);
void _Unwind_RaiseException(void)
{
    VG_(unimplemented)("unimplemented function _Unwind_RaiseException()");
    while (1);
}


void _Unwind_Resume_or_Rethrow(void);
void _Unwind_Resume_or_Rethrow(void)
{
    VG_(unimplemented)("unimplemented function _Unwind_Resume_or_Rethrow()");
    while (1);
}


void __stack_chk_fail(void);
void __stack_chk_fail(void)
{
    VG_(unimplemented)("unimplemented function __stack_chk_fail()");
    while (1);
}

/*************************************
 * Functions needed for linking some *
 * L4Re-Libs with Valgrind           *
 *************************************/

// for libc_backends/lib/file/mmap.cc
// for l4util/lib/src/micros2l4to.c
// for l4util/lib/src/sleep.c
int printf(const char *format, ...);
int printf(const char *format, ...)
{

    int n;
    va_list args;

    va_start(args, format);
    n = VG_(vprintf) (format, args);
    va_end(args);

    return n;
}

#include <pub_tool_mallocfree.h>
// for libc_backends/libc_be_file.h
void free(void *ptr);
void free(void *ptr)
{
    VG_(free) (ptr);
}

#define size_t int
// for l4re/lib/src/log.cc
// for l4re/lib/src/namespace.cc
// Not required anymore since we handle symbols in uclibc different
#if 0
size_t strlen(const char *s);
size_t strlen(const char *s)
{
    return VG_(strlen) (s);
}
#endif

#include <pub_tool_mallocfree.h>
// for libc_backends/libc_be_file.h
void *malloc(size_t size);
void *malloc(size_t size)
{
    return VG_(malloc) ("l4re", size);
}

void *calloc(size_t nmemb, size_t size);
void *calloc(size_t nmemb, size_t size)
{
    return VG_(calloc) ("l4re", nmemb, size);
}
