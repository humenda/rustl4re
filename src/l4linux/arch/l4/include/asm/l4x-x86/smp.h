#pragma once

void smp_call_function_single_interrupt(struct pt_regs *regs);
void smp_call_function_interrupt(struct pt_regs *regs);
void smp_reschedule_interrupt(struct pt_regs *regs);
