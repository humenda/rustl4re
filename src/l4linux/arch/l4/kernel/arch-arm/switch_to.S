#include <linux/linkage.h>

#include <asm/asm-offsets.h>
#include <asm/thread_notify.h>
#include <asm/unwind.h>

/* r0: prev task, r1: prev ti  r2: next ti
 */
ENTRY(__switch_to)
 UNWIND(.fnstart        )
 UNWIND(.cantunwind     )
		add	ip, r1, #TI_CPU_SAVE
		stmia	ip!, {r4 - sl, fp, sp, lr}      @ Store most regs on stack
		mov	r5, r0
		add	r4, r2, #TI_CPU_SAVE
		ldr	r0, =thread_notify_head
		mov	r1, #THREAD_NOTIFY_SWITCH
		bl	atomic_notifier_call_chain
		mov	r0, r5
		ldmia	r4, {r4 - sl, fp, sp, pc}       @ Load all regs saved previously
 UNWIND(.fnend          )
ENDPROC(__switch_to)
