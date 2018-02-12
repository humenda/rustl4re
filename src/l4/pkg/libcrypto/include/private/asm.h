/*
 * \brief   ASM inline helper routines.
 * \date    2006-03-28
 * \author  Bernhard Kauer <kauer@tudos.org>
 */
/*
 * Copyright (C) 2006  Bernhard Kauer <kauer@tudos.org>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the OSLO package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */


#ifndef _ASM_H_
#define _ASM_H_

void reboot(void) __attribute__((noreturn));
void do_skinit(void) __attribute__((noreturn));
void jmp_multiboot(void * mbi, unsigned int entry) __attribute__((noreturn));
void jmp_kernel(unsigned cs, unsigned stack) __attribute__((noreturn));
void jmp_kernel16(unsigned cs, unsigned stack) __attribute__((noreturn));

static inline
unsigned int
ntohl(unsigned int v)
{
  unsigned int res;
  asm volatile("bswap %%eax": "=a"(res): "a"(v));
  return res;
}


static inline
unsigned long long
rdtsc(void)
{
  unsigned long long res;

  // =A does not work in 64 bit mode
  asm volatile ("rdtsc" : "=A"(res));
  return res;
}


static inline
unsigned int
cpuid_eax(unsigned value)
{
  unsigned int res;
  asm volatile ("cpuid" :  "=a"(res) : "a"(value) : "ebx", "ecx", "edx");
  return res;
}


static inline
unsigned int
cpuid_ecx(unsigned value)
{
  unsigned int res, dummy;
  asm volatile ("cpuid" :  "=c"(res), "=a"(dummy): "a"(value) : "ebx", "edx");
  return res;
}


static inline
unsigned int
cpuid_edx(unsigned value)
{
  unsigned int res, dummy;
  asm volatile ("cpuid" :  "=d"(res), "=a"(dummy): "a"(value) : "ebx", "ecx");
  return res;
}


static inline
unsigned long long
rdmsr(unsigned int addr)
{
  unsigned long long res;

  // =A does not work in 64 bit mode
  asm volatile ("rdmsr" : "=A"(res) : "c"(addr));
  return res;
}


static inline
void
wrmsr(unsigned int addr, unsigned long long value)
{
  // =A does not work in 64 bit mode
  asm volatile ("wrmsr" :: "c"(addr), "A"(value));
}

#endif
