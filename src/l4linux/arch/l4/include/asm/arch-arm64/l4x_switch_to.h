/* SPDX-License-Identifier: GPL-2.0 */
#pragma once

#include <linux/thread_info.h>

void l4x_switch_to(struct task_struct *, struct task_struct *);
