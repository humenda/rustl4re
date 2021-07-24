#pragma once

#include <asm/ptrace.h>

void l4x_x86_init_irq(void);
void l4x_x86_vcpu_handle_ipi(struct pt_regs *regs);
