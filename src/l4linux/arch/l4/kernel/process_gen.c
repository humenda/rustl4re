
#include <linux/errno.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>
#include <linux/ratelimit.h>

#include <asm/generic/mmu_context.h>
#include <asm/generic/task.h>
#include <asm/generic/hybrid.h>
#include <asm/generic/user.h>

#include <asm/api/macros.h>

#include <asm/mmu_context.h>
#include <asm/l4lxapi/task.h>
#include <asm/l4lxapi/thread.h>

#include <l4/log/log.h>
#include <l4/sys/ipc_gate.h>
#include <l4/sys/ipc.h>
#include <l4/sys/task.h>

void l4x_init_new_context(struct task_struct *tsk, struct mm_struct *mm)
{
	mm->context.task = L4_INVALID_CAP;
}

void l4x_destroy_context(struct mm_struct *mm)
{
	l4_cap_idx_t task_id;

	if (!mm || !mm->context.task ||
	    l4_is_invalid_cap(task_id = mm->context.task))
		return;

	if (l4lx_task_delete_task(task_id))
		do_exit(9);

	mm->context.task = L4_INVALID_CAP;

	l4lx_task_number_free(task_id);
}

#ifndef CONFIG_L4_VCPU
void l4x_init_thread_struct(struct task_struct *p)
{
	int i;
	for (i = 0; i < NR_CPUS; i++)
		p->thread.l4x.user_thread_ids[i] = L4_INVALID_CAP;
	p->thread.l4x.user_thread_id = L4_INVALID_CAP;
	p->thread.l4x.threads_up = 0;
}
#endif

void l4x_exit_thread(void)
{
#ifndef CONFIG_L4_VCPU
	int i;

	if (unlikely(current->thread.l4x.is_hybrid)) {
		l4_cap_idx_t hybgate;
		l4_msgtag_t tag;
		l4_umword_t o = 0;

		hybgate = L4LX_KERN_CAP_HYBRID_BASE
		           + (current->pid << L4_CAP_SHIFT);

		tag = l4_ipc_gate_get_infos(hybgate, &o);
		if (l4_error(tag))
			printk("hybrid: Could not get gate info, leaking mem.\n");
		else
			kfree((void *)o);

		tag = l4_task_unmap(L4_BASE_TASK_CAP,
		                    l4_obj_fpage(hybgate, 0, L4_FPAGE_RWX),
		                    L4_FP_ALL_SPACES);
		if (l4_error(tag))
			printk("hybrid: Delete of gate failed.\n");
	}

	for (i = 0; i < NR_CPUS; i++) {
		l4_cap_idx_t thread_id = current->thread.l4x.user_thread_ids[i];

		/* check if we were a non-user thread (i.e., have no
		   user-space partner) */
		if (unlikely(l4_is_invalid_cap(thread_id) || !thread_id))
			continue;

#ifdef DEBUG
		LOG_printf("exit_thread: trying to delete %s(%d, " PRINTF_L4TASK_FORM ")\n",
		           current->comm, current->pid, PRINTF_L4TASK_ARG(thread_id));
#endif

		/* If task_delete fails we don't free the task number so that it
		 * won't be used again. */

		if (likely(!l4lx_task_delete_thread(thread_id))) {
			l4x_hybrid_remove(current);
			current->thread.l4x.user_thread_ids[i] = L4_INVALID_CAP;
			l4lx_task_number_free(thread_id);
			current->thread.l4x.started = 0;
		} else
			printk("%s: failed to delete task " PRINTF_L4TASK_FORM "\n",
			       __func__, PRINTF_L4TASK_ARG(thread_id));

	}
#endif

#ifdef CONFIG_X86_DS
	ds_exit_thread(current);
#endif
}

void l4x_evict_tasks(struct task_struct *exclude)
{
	struct task_struct *p;
	int cnt = 0;

	rcu_read_lock();
	for_each_process(p) {
		l4_cap_idx_t t;
		struct mm_struct *mm;

		if (p == exclude)
			continue;

		task_lock(p);
		mm = p->mm;
		if (!mm) {
			task_unlock(p);
			continue;
		}

		t = READ_ONCE(mm->context.task);

		if (l4_is_invalid_cap(t)) {
			task_unlock(p);
			continue;
		}

		if (down_read_trylock(&mm->mmap_sem)) {
			struct vm_area_struct *vma;
			for (vma = mm->mmap; vma; vma = vma->vm_next)
				if (vma->vm_flags & VM_LOCKED) {
					t = L4_INVALID_CAP;
					break;
				}
			up_read(&mm->mmap_sem);
			if (!vma)
				if (cmpxchg(&mm->context.task, t, L4_INVALID_CAP) != t)
					t = L4_INVALID_CAP;
		} else
			t = L4_INVALID_CAP;

		task_unlock(p);

		if (!l4_is_invalid_cap(t)) {
			l4lx_task_delete_task(t);
			l4lx_task_number_free(t);

			if (++cnt > 10)
				break;
		}
	}
	rcu_read_unlock();

	if (cnt == 0)
		pr_info_ratelimited("l4x-evict: Found no process to free.\n");
}
