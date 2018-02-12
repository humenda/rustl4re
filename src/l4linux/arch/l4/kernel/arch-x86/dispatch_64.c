#include <asm/syscall.h>
#include <asm/proto.h>

/* syscall args for 64bit */
#define S_ARG0 di
#define S_ARG1 si
#define S_ARG2 dx
#define S_ARG3 r10
#define S_ARG4 r8
#define S_ARG5 r9

void native_load_gs_index(unsigned selector)
{
	loadsegment(gs, selector);
}

static inline void l4x_print_regs(unsigned long err, unsigned long trapno,
                                  struct pt_regs *r)
{
	printk("ip: %02lx:%08lx sp: %08lx err: %08lx trp: %08lx\n",
	       r->cs, r->ip, r->sp, err, trapno);
	printk("ax: %08lx bx: %08lx  cx: %08lx  dx: %08lx\n",
	       r->ax, r->bx, r->cx, r->dx);
	printk("di: %08lx si: %08lx  bp: %08lx\n",
	       r->di, r->si, r->bp);
}

static inline bool l4x_is_linux_syscall(unsigned long err, unsigned long trapno,
                                        struct pt_regs *regs)
{
	return trapno == 0xfd;
}

#ifdef CONFIG_IA32_EMULATION
static inline bool l4x_is_compat_syscall(unsigned long err, unsigned long trapno,
                                         struct pt_regs *regs)
{
	/* int 0x80 is trap 0xd and err 0x402 (0x80 << 3 | 2) */
	/* QEMU has a bug and reports 0x802! */
	return trapno == 0xd && (err == 0x802 || err == 0x402);
}

static inline void dispatch_compat_system_call(struct task_struct *p,
                                               struct pt_regs *regsp)
{
	unsigned int syscall;
	int show_syscalls = 0;

#ifdef CONFIG_L4_VCPU
	local_irq_enable();
#endif

	regsp->orig_ax = syscall = regsp->ax;
	regsp->ax = -ENOSYS;

	if (show_syscalls)
		printk("Syscall %3d for %s(%d at %p): args: %lx,%lx,%lx\n",
		       syscall, p->comm, p->pid, (void *)regsp->ip,
		       regsp->bx, regsp->cx, regsp->dx);

	if (show_syscalls && syscall == 11) {
		struct filename *fn;
		printk("execve: pid: %d(%s): ", p->pid, p->comm);
		fn = getname((char *)regsp->bx);
		printk("%s\n", IS_ERR(fn) ? "UNKNOWN" : fn->name);
		putname(fn);
	}

	if (show_syscalls && syscall == 120)
		printk("Syscall %3d for %s(%d at %p): arg1 = %lx ebp=%lx\n",
		       syscall, p->comm, p->pid, (void *)regsp->ip,
		       regsp->bx, regsp->bp);

	if (show_syscalls && syscall == 21)
		printk("Syscall %3d mount for %s(%d at %p): %lx %lx %lx %lx %lx %lx\n",
		       syscall, p->comm, p->pid, (void *)regsp->ip,
		       regsp->bx, regsp->cx, regsp->dx,
		       regsp->si, regsp->di, regsp->bp);
	if (show_syscalls && syscall == 5) {
		struct filename *fn = getname((char *)regsp->bx);
		printk("open: pid: %d(%s): %s (%lx)\n",
		       current->pid, current->comm,
		       IS_ERR(fn) ? "UNKNOWN" : fn->name, regsp->bx);
		if (!IS_ERR(fn))
			putname(fn);
	}
	if (show_syscalls && syscall == 240) {
		printk("futex: %d(%s): addr=%lx op=%u val=%x addr2=%lx val3=%x\n",
		       current->pid, current->comm,
		       (unsigned long)(u32)regsp->bx, (u32)regsp->cx,
		       (u32)regsp->dx, (unsigned long)(u32)regsp->di,
		       (u32)regsp->bp);
	}

	if (unlikely(syscall >= IA32_NR_syscalls)) {
		printk("Syscall %3d for %s(%d at %p): arg1 = %lx\n",
		       syscall, p->comm, p->pid, (void *)regsp->ip,
		       regsp->bx);
		l4x_print_regs(p->thread.error_code, p->thread.trap_nr, regsp);
	}

	do_syscall_32_irqs_on(regsp);

	if (show_syscalls)
		printk("Syscall %3d for %s(%d at %p): return %lx/%ld\n",
		       syscall, p->comm, p->pid, (void *)regsp->ip,
		       regsp->ax, regsp->ax);
#ifdef CONFIG_L4_VCPU
	local_irq_disable();
#endif
}
#endif
