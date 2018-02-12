/*
 * Architecture specific handling for tamed mode for i386.
 */
#ifndef __ASM_L4__L4X_I386__TAMED_H__
#define __ASM_L4__L4X_I386__TAMED_H__

#ifndef L4X_TAMED_LABEL
#error Only use from within tamed.c!
#endif

#ifdef CONFIG_X86_64
#ifndef L4_ENTER_KERNEL
#define L4_ENTER_KERNEL "syscall"
#endif
#endif

#ifdef CONFIG_X86_64
#define R "r"
#else
#define R "e"
#endif

static inline void l4x_tamed_sem_down(void)
{
#ifdef CONFIG_X86_32
	unsigned dummy1;
#endif
	unsigned dummy2, dummy3;
	unsigned cpu = smp_processor_id();
	l4_utcb_t *utcb = l4x_utcb_current();

	asm volatile
	  (
	   "1:                         \n\t"
	   "decl    0(%%" R "bx)       \n\t"        /* decrement counter */
	   "jge     2f                 \n\t"

#ifdef CONFIG_L4_DEBUG_TAMED_COUNT_INTERRUPT_DISABLE
	   "incl    cli_taken          \n\t"
#endif
	   "push    %%" R "dx          \n\t"
	   "push    %%" R "ax          \n\t"
	   "push    %%" R "si          \n\t"
	   "push    %%" R "bx          \n\t"

#ifdef CONFIG_X86_64
	   "xor     %%r8,%%r8          \n\t"        /* timeout never */
#else
	   "xor     %%ecx,%%ecx        \n\t"        /* timeout never */
#endif

	   L4_ENTER_KERNEL            "\n\t"

	   "test    $0x00010000, %%" R "ax  \n\t"        /* Check return tag */

	   "pop     %%" R "bx               \n\t"
	   "pop     %%" R "si               \n\t"
	   "pop     %%" R "ax               \n\t"
	   "pop     %%" R "dx               \n\t"

	   "jnz     2f                 \n\t"
	   "jmp     1b                 \n\t"

	   "2:                         \n\t"
	   : "=D" (dummy2), "=S" (dummy3)
#ifdef CONFIG_X86_32
	     , "=c" (dummy1)
#endif
	   : "a"  ((l4x_prio_current_utcb(utcb) << 20) | (1 << 16)),
	     "b"  (&tamed_per_nr(cli_lock, get_tamer_nr(cpu)).sem),
	     "D"  (utcb),
	     "d"  (tamed_per_nr(cli_sem_thread_id, get_tamer_nr(cpu)) | L4_SYSF_CALL),
             "S"  (l4x_cap_current_utcb(utcb))
	   : "memory", "cc"
#ifdef CONFIG_X86_64
             , "r8", "rcx", "r11", "r15"
#endif
          );
}

static inline void l4x_tamed_sem_up(void)
{
#ifdef CONFIG_X86_32
	unsigned dummy1;
#endif
	unsigned dummy2, dummy3, dummy4;
	unsigned cpu = smp_processor_id();
	l4_utcb_t *utcb = l4x_utcb_current();
	l4_msgtag_t rtag;
#ifdef CONFIG_X86_64
	register l4_umword_t to __asm__("r8") = 0;
#endif


	asm volatile
	  (
	   "incl    0(%%" R "bx)       \n\t"        /* increment counter */
	   "jg      2f                 \n\t"

	   L4_ENTER_KERNEL            "\n\t"

	   "2:                         \n\t"
	   : "=a" (rtag),
#ifdef CONFIG_X86_32
	     "=c" (dummy1),
#endif
	     "=D" (dummy2), "=S" (dummy3), "=d" (dummy4)
	   : "a"  ((l4x_prio_current_utcb(utcb) << 20) | (2 << 16)),
	     "b"  (&tamed_per_nr(cli_lock, get_tamer_nr(cpu)).sem),
	     "D"  (utcb),
	     "d"  (tamed_per_nr(cli_sem_thread_id, get_tamer_nr(cpu)) | L4_SYSF_CALL),
#ifdef CONFIG_X86_32
             "c"  (0),
#else
             "r"  (to),
#endif
	     "S"  (l4x_cap_current_utcb(utcb))
	   : "memory", "cc"
#ifdef CONFIG_X86_64
             , "rcx", "r11", "r15"
#endif
	   );

	if (unlikely(l4_ipc_error(rtag, l4_utcb())))
		outstring("l4x_tamed_sem_up ipc failed\n");
}

#undef R

#endif /* ! __ASM_L4__L4X_I386__TAMED_H__ */
