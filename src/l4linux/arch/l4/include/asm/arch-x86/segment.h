#ifndef __ASM_L4__ARCH_I386__SEGMENT_H__
#define __ASM_L4__ARCH_I386__SEGMENT_H__

/* We include and modify the contents of segment.h */

#include <asm-x86/segment.h>

#ifndef __ASSEMBLY__

extern unsigned l4x_x86_fiasco_gdt_entry_offset;

#undef  GDT_ENTRY_TLS_MIN
#define GDT_ENTRY_TLS_MIN	l4x_x86_fiasco_gdt_entry_offset

extern unsigned l4x_x86_fiasco_user_ds;
extern unsigned l4x_x86_fiasco_user_cs;

#ifdef CONFIG_X86_64
extern unsigned l4x_x86_fiasco_user32_cs;

#ifdef CONFIG_L4_VCPU

#define L4X_SEG(vcpu, seg) (vcpu)->r.seg
#define L4X_SEG_CUR(seg)   L4X_SEG(l4x_current_vcpu(), seg)

#else /* L4_VCPU */

#include <linux/threads.h>

struct l4x_segment_user_state_t
{
	unsigned ds, es, gs, fs;
	unsigned long gs_base, fs_base;
};

extern struct l4x_segment_user_state_t l4x_current_user_segments[NR_CPUS];

#define l4x_current_useg()     (&l4x_current_user_segments[smp_processor_id()])

#define L4X_SEG(segstate, seg) (segstate)->seg
#define L4X_SEG_CUR(seg)       L4X_SEG(l4x_current_useg(), seg)

#endif /* L4_VCPU */

#undef loadsegment
#undef savesegment

#define loadsegment(seg, v) L4X_SEG_CUR(seg) = (v)
#define savesegment(seg, v) (v) = L4X_SEG_CUR(seg)


#undef __USER32_DS
#define __USER32_DS (l4x_x86_fiasco_user_ds)

#undef __USER32_CS
#define  __USER32_CS (l4x_x86_fiasco_user32_cs)

#endif /* X86_64 */

#undef __USER_DS
#define __USER_DS (l4x_x86_fiasco_user_ds)

#undef __USER_CS
#define  __USER_CS (l4x_x86_fiasco_user_cs)

#endif /* __ASSEMBLY__ */

#endif /* ! __ASM_L4__ARCH_I386__SEGMENT_H__ */
