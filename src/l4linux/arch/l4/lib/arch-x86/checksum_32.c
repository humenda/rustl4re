/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		IP/TCP/UDP checksumming routines
 *
 * Authors:	Jorge Cwik, <jorge@laser.satlink.net>
 *		Arnt Gulbrandsen, <agulbra@nvg.unit.no>
 *		Tom May, <ftom@netcom.com>
 *		Lots of code moved from tcp.c and ip.c; see those files
 *		for more names.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */

#include <net/checksum.h>
#include <asm/segment.h>
#include <asm/generic/memory.h>

/* #define DEBUG_PARTIAL_CSUM_FROM_USER */
/*
 * computes a partial checksum, e.g. for TCP/UDP fragments
 */

asmlinkage __wsum csum_partial(const void *buff, int len, __wsum sum) {
     /*
      * Experiments with ethernet and slip connections show that buff
      * is aligned on either a 2-byte or 4-byte boundary.  We get at
      * least a 2x speedup on 486 and Pentium if it is 4-byte aligned.
      * Fortunately, it is easy to convert 2-byte alignment to 4-byte
      * alignment for the unrolled loop.
      */
     long dummy1, dummy2;
     __asm__(
	"	testl $2, %%esi		\n" /* Check alignment. */
	"	jz 2f			\n" /* Jump if alignment is ok. */
	"	subl $2, %%ecx		\n" /* Alignment uses up two bytes. */
	"	jae 1f			\n" /* Jump if we had at least two bytes. */
	"	addl $2, %%ecx		\n" /* ecx was < 2.  Deal with it. */
	"	jmp 4f			\n"
	"1:				\n"
	"	movw (%%esi), %%bx	\n"
	"	addl $2, %%esi		\n"
	"	addw %%bx, %%ax		\n"
	"	adcl $0, %%eax		\n"
	"2:				\n"
	"	movl %%ecx, %%edx	\n"
	"	shrl $5, %%ecx		\n"
	"	jz 2f			\n"
	"	testl %%esi, %%esi	\n"
	"1:				\n"
	"	movl (%%esi), %%ebx	\n"
	"	adcl %%ebx, %%eax	\n"
	"	movl 4(%%esi), %%ebx	\n"
	"	adcl %%ebx, %%eax	\n"
	"	movl 8(%%esi), %%ebx	\n"
	"	adcl %%ebx, %%eax	\n"
	"	movl 12(%%esi), %%ebx	\n"
	"	adcl %%ebx, %%eax	\n"
	"	movl 16(%%esi), %%ebx	\n"
	"	adcl %%ebx, %%eax	\n"
	"	movl 20(%%esi), %%ebx	\n"
	"	adcl %%ebx, %%eax	\n"
	"	movl 24(%%esi), %%ebx	\n"
	"	adcl %%ebx, %%eax	\n"
	"	movl 28(%%esi), %%ebx	\n"
	"	adcl %%ebx, %%eax	\n"
	"	lea 32(%%esi), %%esi	\n"
	"	dec %%ecx		\n"
	"	jne 1b			\n"
	"	adcl $0, %%eax		\n"
	"2:				\n"
	"	movl %%edx, %%ecx	\n"
	"	andl $0x1c, %%edx	\n"
	"	je 4f			\n"
	"	shrl $2, %%edx		\n"
	"	testl %%esi, %%esi	\n"
	"3:				\n"
	"	adcl (%%esi), %%eax	\n"
	"	lea 4(%%esi), %%esi	\n"
	"	dec %%edx		\n"
	"	jne 3b			\n"
	"	adcl $0, %%eax		\n"
	"4:				\n"
	"	andl $3, %%ecx		\n"
	"	jz 7f			\n"
	"	cmpl $2, %%ecx		\n"
	"	jb 5f			\n"
	"	movw (%%esi),%%cx	\n"
	"	leal 2(%%esi),%%esi	\n"
	"	je 6f			\n"
	"	shll $16,%%ecx		\n"
	"5:				\n"
	"	movb (%%esi),%%cl	\n"
	"6:				\n"
	"	addl %%ecx,%%eax	\n"
	"	adcl $0, %%eax		\n"
	"7:				\n"
	     : "=a"(sum), "=c" (dummy1), "=S" (dummy2)
	     : "0"(sum), "1"(len), "2"(buff)
	     : "bx", "dx");
     return(sum);
}
EXPORT_SYMBOL(csum_partial);

/*
 * copy from ds while checksumming, otherwise like csum_partial
 */

__wsum csum_partial_copy(const void *src, void *dst, 
			 int len, __wsum sum) {
     long dummy1, dummy2, dummy3;
     __asm__(
	"	testl $2, %%edi		\n" /* Check alignment. */
	"	jz 2f			\n" /* Jump if alignment is ok. */
	"	subl $2, %%ecx		\n" /* Alignment uses up two bytes. */
	"	jae 1f			\n" /* Jump if we had at least two bytes. */
	"	addl $2, %%ecx		\n" /* ecx was < 2.  Deal with it. */
	"	jmp 4f			\n"
	"1:	movw (%%esi), %%bx	\n"
	"	addl $2, %%esi		\n"
	"	movw %%bx, (%%edi)	\n"
	"	addl $2, %%edi		\n"
	"	addw %%bx, %%ax		\n"
	"	adcl $0, %%eax		\n"
	"2:				\n"
	"	movl %%ecx, %%edx	\n"
	"	shrl $5, %%ecx		\n"
	"	jz 2f			\n"
	"	testl %%esi, %%esi	\n"
	"1:	movl (%%esi), %%ebx	\n"
	"	adcl %%ebx, %%eax	\n"
	"	movl %%ebx, (%%edi)	\n"
	"				\n"
	"	movl 4(%%esi), %%ebx	\n"
	"	adcl %%ebx, %%eax	\n"
	"	movl %%ebx, 4(%%edi)	\n"
	"				\n"
	"	movl 8(%%esi), %%ebx	\n"
	"	adcl %%ebx, %%eax	\n"
	"	movl %%ebx, 8(%%edi)	\n"
	"				\n"
	"	movl 12(%%esi), %%ebx	\n"
	"	adcl %%ebx, %%eax	\n"
	"	movl %%ebx, 12(%%edi)	\n"
	"				\n"
	"	movl 16(%%esi), %%ebx	\n"
	"	adcl %%ebx, %%eax	\n"
	"	movl %%ebx, 16(%%edi)	\n"
	"				\n"
	"	movl 20(%%esi), %%ebx	\n"
	"	adcl %%ebx, %%eax	\n"
	"	movl %%ebx, 20(%%edi)	\n"
	"				\n"
	"	movl 24(%%esi), %%ebx	\n"
	"	adcl %%ebx, %%eax	\n"
	"	movl %%ebx, 24(%%edi)	\n"
	"				\n"
	"	movl 28(%%esi), %%ebx	\n"
	"	adcl %%ebx, %%eax	\n"
	"	movl %%ebx, 28(%%edi)	\n"
	"				\n"
	"	lea 32(%%esi), %%esi	\n"
	"	lea 32(%%edi), %%edi	\n"
	"	dec %%ecx		\n"
	"	jne 1b			\n"
	"	adcl $0, %%eax		\n"
	"2:	movl %%edx, %%ecx	\n"
	"	andl $28, %%edx		\n"
	"	je 4f			\n"
	"	shrl $2, %%edx		\n"
	"	testl %%esi, %%esi	\n"
	"3:	movl (%%esi), %%ebx	\n"
	"	adcl %%ebx, %%eax	\n"
	"	movl %%ebx, (%%edi)	\n"
	"	lea 4(%%esi), %%esi	\n"
	"	lea 4(%%edi), %%edi	\n"
	"	dec %%edx		\n"
	"	jne 3b			\n"
	"	adcl $0, %%eax		\n"
	"4:	andl $3, %%ecx		\n"
	"	jz 7f			\n"
	"	cmpl $2, %%ecx		\n"
	"	jb 5f			\n"
	"	movw (%%esi), %%cx	\n"
	"	leal 2(%%esi), %%esi	\n"
	"	movw %%cx, (%%edi)	\n"
	"	leal 2(%%edi), %%edi	\n"
	"	je 6f			\n"
	"	shll $16,%%ecx		\n"
	"5:	movb (%%esi), %%cl	\n"
	"	movb %%cl, (%%edi)	\n"
	"6:	addl %%ecx, %%eax	\n"
	"	adcl $0, %%eax		\n"
	"7:				\n"
	     : "=a" (sum), "=c"(dummy1), "=S"(dummy2), "=D" (dummy3)
	     : "0"(sum), "1"(len), "2"(src), "3" (dst)
	     : "bx", "dx" );
     return(sum);
}
EXPORT_SYMBOL(csum_partial_copy);
