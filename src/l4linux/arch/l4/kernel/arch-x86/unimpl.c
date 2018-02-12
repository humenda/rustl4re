#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/ioport.h>

/* Just to make the linker happy */

asmlinkage int sys_vm86(struct pt_regs *regs)
{
	printk("sys_vm86() called\n");
	return -EPERM;
}
asmlinkage int sys_vm86old(struct pt_regs *regs)
{
	printk("sys_vm86old() called\n");
	return -EPERM;
}

