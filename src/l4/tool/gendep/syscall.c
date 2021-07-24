/*
  syscall.c -- override open(2)

  (c) Han-Wen Nienhuys <hanwen@cs.uu.nl> 1998
  (c) Jork Loeser <jork.loeser@inf.tu-dresden.de> 2002
  (c) Michael Roitzsch <mroi@os.inf.tu-dresden.de> 2007
 */

#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <unistd.h>
/* Assume GNU as target platform. We need this in dlfcn.h */
#define __USE_GNU
#include <dlfcn.h>
#include "gendep.h"

#ifdef __ELF__
#  define OPEN    __open
#  define OPEN64  __open64
#  define FOPEN   __fopen
#  define FOPEN64 __fopen64
#  define UNLINK  __unlink
#else
#  define OPEN    open
#  define OPEN64  open64
#  define FOPEN   fopen
#  define FOPEN64 fopen64
#  define UNLINK  unlink
#endif

#define VERBOSE 0	/* set to 1 to log open/unlink calls */

/* ----------------------------------------------------------------------- */
/*
  This breaks if we hook gendep onto another library that overrides open(2).
  (zlibc comes to mind)
 */
static int real_open (const char *fn, int flags, int mode)
{
  if(VERBOSE) printf("real_open(%s)\n", fn);
  return (syscall(SYS_openat, AT_FDCWD, (fn), (flags), (mode)));
}

int OPEN(const char *fn, int flags, ...)
{
  int rv ;
  va_list p;
  va_start (p,flags);

  if(VERBOSE) printf("open(%s) called\n", fn);
  rv = real_open (fn, flags, va_arg (p, int));
  if (rv >=0)
    gendep__register_open (fn, flags);

  return rv;
}

/* ----------------------------------------------------------------------- */
typedef int(*open64_type)(const char*, int flag, int mode);

static int real_open64(const char*path, int flag, int mode){
  static open64_type f_open64;

  if(VERBOSE) printf("real_open64(%s)\n", path);
  if(f_open64==0){
    *(void**)(&f_open64) = dlsym(RTLD_NEXT, "open64");
    if(!f_open64){
      fprintf(stderr, "gendep: Cannot resolve open64()\n");
      errno=ENOENT;
      return 0;
    }
  }
  return f_open64(path, flag, mode);
}

int OPEN64(const char*path, int flags, ...){
  int f;
  va_list p;
  va_start (p,flags);

  if(VERBOSE) printf("open64(%s)\n", path);
  f = real_open64(path, flags, va_arg (p, int));
  if(f>=0){
    gendep__register_open(path, flags);
  }
  return f;
}

/* ----------------------------------------------------------------------- */
typedef FILE* (*fopen_type)(const char*, const char*);

static FILE* real_fopen(const char*path, const char*mode){
  static fopen_type f_fopen;

  if(VERBOSE) printf("real_fopen(%s)\n", path);
  if(f_fopen==0){
    *(void**)(&f_fopen) = dlsym(RTLD_NEXT, "fopen");
    if(!f_fopen){
      fprintf(stderr, "gendep: Cannot resolve fopen()\n");
      errno=ENOENT;
      return 0;
    }
  }
  return f_fopen(path, mode);
}

FILE* FOPEN(const char*path, const char*mode){
  FILE *f;
  int binmode;

  if(VERBOSE) printf("fopen(%s)\n", path);
  f = real_fopen(path, mode);
  if(f){
    if(strchr(mode, 'w') || strchr(mode, 'a')){
      binmode=O_WRONLY;
    } else {
      binmode=O_RDONLY;
    }
    gendep__register_open(path, binmode);
  }
  return f;
}

/* ----------------------------------------------------------------------- */
typedef FILE* (*fopen64_type)(const char*, const char*);

static FILE* real_fopen64(const char*path, const char*mode){
  static fopen64_type f_fopen64;

  if(VERBOSE) printf("real_fopen64(%s)\n", path);
  if(f_fopen64==0){
    *(void**)(&f_fopen64) = dlsym(RTLD_NEXT, "fopen64");
    if(!f_fopen64){
      fprintf(stderr, "gendep: Cannot resolve fopen64()\n");
      errno=ENOENT;
      return 0;
    }
  }
  return f_fopen64(path, mode);
}

FILE* FOPEN64(const char*path, const char*mode){
  FILE *f;
  int binmode;

  if(VERBOSE) printf("fopen64(%s)\n", path);
  f = real_fopen64(path, mode);
  if(f){
    if(strchr(mode, 'w') || strchr(mode, 'a')){
      binmode=O_WRONLY;
    } else {
      binmode=O_RDONLY;
    }
    gendep__register_open(path, binmode);
  }
  return f;
}

/* ----------------------------------------------------------------------- */
static int real_unlink (const char *fn)
{
  if(VERBOSE) printf("real_unlink(%s)\n", fn);
  return syscall(SYS_unlinkat, AT_FDCWD, (fn), 0);
}

int UNLINK(const char *fn)
{
  int rv ;

  if(VERBOSE) printf("unlink(%s)\n", fn);
  rv = real_unlink (fn);
  if (rv >=0)
    gendep__register_unlink (fn);

  return rv;
}

/* ----------------------------------------------------------------------- */
#ifdef __ELF__
int open (const char *fn, int flags, ...) __attribute__ ((alias ("__open")));
int open64 (const char *fn, int flags, ...) __attribute__ ((alias ("__open64")));
int unlink(const char *fn) __attribute__ ((alias ("__unlink")));
FILE *fopen (const char *path, const char *mode) __attribute__ ((alias ("__fopen")));
FILE *fopen64 (const char *path, const char *mode) __attribute__ ((alias ("__fopen64")));
#endif
