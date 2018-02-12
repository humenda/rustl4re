#pragma once

void l4x_init_new_context(struct task_struct *tsk, struct mm_struct *mm);
void l4x_destroy_context(struct mm_struct *mm);
