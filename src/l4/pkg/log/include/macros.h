/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU Lesser General Public License 2.1.
 * Please see the COPYING-LGPL-2.1 file for details.
 */
#pragma once

#include <l4/log/log.h>
#include <l4/sys/kdebug.h>
#include <l4/sys/compiler.h>

#ifndef L4BID_RELEASE_MODE
/* format_check: just for the compiler to check the format & args */
__BEGIN_DECLS
L4_CV void LOG_format_check(const char*format,...)
  __attribute__((format(printf,1,2)));

__END_DECLS

#define __LINE_STR__ L4_stringify(__LINE__)

#define LOG(a...) do {				\
  if(0)LOG_format_check(a);			\
  LOG_log(__func__, a);				\
} while(0)

#define LOGl(a...) do {				\
  if(0)LOG_format_check(a);			\
  LOG_logl(__FILE__,__LINE__,__func__, a);	\
} while(0)

#define LOG_Enter(a...) do {			\
  if(0)LOG_format_check("called " a);		\
  LOG_log(__func__, "called " a);		\
} while(0)

#define LOGk(a...) do {				\
  if(0)LOG_format_check(a);			\
  LOG_logk(a);					\
} while(0)

#define LOGd_Enter(doit, msg...) do { if(doit) LOG_Enter(msg); } while (0)
#define LOGd(doit, msg...)       do { if(doit) LOG(msg);       } while (0)
#define LOGdl(doit,msg...)       do { if(doit) LOGl(msg);      } while (0)
#define LOGdk(doit,msg...)       do { if(doit) LOGk(msg);      } while (0)
#define LOG_Error(msg...)        LOGl("Error: " msg)

#define LOG_logk(format...)      do {                                     \
                                   char buf[35];                          \
                                   LOG_snprintf(buf,sizeof(buf),format);  \
                                   LOG_outstring_fiasco_tbuf(buf);        \
                                 } while (0)

#else
#define LOG(a...)
#define LOGl(a...)
#define LOGL(a...)
#define LOGk(a...)
#define LOG_Enter(a...)
#define LOGd_Enter(doit, msg...)
#define LOGd(doit, msg...)
#define LOGdl(doit,msg...)
#define LOGdL(doit,msg...)
#define LOGdk(doit,msg...)
#define LOG_Error(msg...)
#endif

__BEGIN_DECLS
void _exit(int status) __attribute__ ((__noreturn__));
__END_DECLS

/* print message and enter kernel debugger */
#ifndef Panic
# ifdef L4BID_RELEASE_MODE
#  define Panic(args...) do                                      \
                           {                                     \
			     LOG(args);                          \
			     LOG_flush();                        \
			     _exit(-1);                          \
			   }                                     \
                         while (1)
# else
#  define Panic(args...) do                                      \
                           {                                     \
                             LOG(args);                          \
                             LOG_flush();                        \
                             enter_kdebug("PANIC, 'g' for exit");\
                             _exit(-1);                          \
                           }                                     \
                         while (1)
# endif
#endif

/* assertion */
#ifndef Assert
#  define Assert(expr) do                                        \
                         {                                       \
                           if (!(expr))                          \
                             {                                   \
                               LOG_printf(#expr "\n");           \
                               Panic("Assertion failed");        \
                             }                                   \
                         }                                       \
                       while (0)
#endif

/* enter kernel debugger */
#ifndef Kdebug
#  define Kdebug(args...)  do                                    \
                             {                                   \
                               LOG(args);                        \
                               LOG_flush();                      \
                               enter_kdebug("KD");               \
                             }                                   \
                           while (0)
#endif
