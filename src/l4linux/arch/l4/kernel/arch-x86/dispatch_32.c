
/* syscall args for 32bit */
#define S_ARG0 bx
#define S_ARG1 cx
#define S_ARG2 dx
#define S_ARG3 si
#define S_ARG4 di
#define S_ARG5 bp

static inline void l4x_print_regs(unsigned long err, unsigned long trapno,
                                  struct pt_regs *r)
{
	printk("ip: %02x:%08lx sp: %08lx err: %08lx trp: %08lx\n",
	       r->cs, r->ip, r->sp, err, trapno);
	printk("ax: %08lx bx: %08lx  cx: %08lx  dx: %08lx\n",
	       r->ax, r->bx, r->cx, r->dx);
	printk("di: %08lx si: %08lx  bp: %08lx  gs: %08x fs: %08x\n",
	       r->di, r->si, r->bp, r->gs, r->fs);
}

static inline bool l4x_is_linux_syscall(unsigned long err, unsigned long trapno,
                                        struct pt_regs *regs)
{
	/* int 0x80 is trap 0xd and err 0x402 (0x80 << 3 | 2) */
	return trapno == 0xd && err == 0x402;
}
