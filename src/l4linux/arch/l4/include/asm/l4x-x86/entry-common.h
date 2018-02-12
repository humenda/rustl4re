/* We use function from common.c in C, so let's have some header */

void syscall_return_slowpath(struct pt_regs *regs);
void prepare_exit_to_usermode(struct pt_regs *regs);
