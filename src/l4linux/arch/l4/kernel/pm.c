
#include <linux/suspend.h>
#include <linux/syscore_ops.h>
#include <linux/interrupt.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/irq.h>
#include <asm/cacheflush.h>
#include <asm/generic/stack_id.h>
#include <asm/generic/irq.h>
#include <asm/generic/tamed.h>
#include <asm/generic/vmalloc.h>
#include <asm/generic/log.h>
#include <asm/generic/memory.h>
#include <asm/generic/vcpu.h>
#include <asm/generic/util.h>

#include <asm/l4lxapi/task.h>
#include <asm/l4lxapi/memory.h>

#include <l4/sys/irq.h>
#include <l4/sys/platform_control.h>
#include <l4/log/log.h>

static LIST_HEAD(wakeup_srcs);

struct wakeup_src {
	struct list_head list;
	int irq;
	l4_cap_idx_t irqcap;
};

static DEFINE_SPINLOCK(list_lock);

enum {
	IRQ_TYPE_USER_SELECTED,
	IRQ_TYPE_ALL_IRQS,
	IRQ_TYPE_LX_INFRA,
	IRQ_TYPE_TYPES,
};

static char *wakeup_irq_types[] = {
	[IRQ_TYPE_USER_SELECTED] = "selected",
	[IRQ_TYPE_ALL_IRQS]      = "all",
	[IRQ_TYPE_LX_INFRA]      = "lx+selected",
};


static int suspend_irq_type = IRQ_TYPE_LX_INFRA;
static int suspend_possible;

#ifndef CONFIG_L4_VCPU
static void l4x_global_wait_save(void)
{}

static void l4x_global_saved_event_inject(void)
{}
#endif


static l4_cap_idx_t get_int_cap(int irq)
{
	if (irq < NR_IRQS_HW) {
		struct l4x_irq_desc_private *p = irq_get_chip_data(irq);
		return p ? p->c.irq_cap : L4_INVALID_CAP;
	}

	if (irq < NR_IRQS)
		return l4x_have_irqcap(irq, 0);

	return L4_INVALID_CAP;
}

static int l4x_wakeup_source_register(int irq)
{
	struct wakeup_src *s, *tmp;
	unsigned long flags;
	l4_cap_idx_t cap;

	list_for_each_entry_safe(s, tmp, &wakeup_srcs, list)
		if (irq == s->irq)
			return -EEXIST;

	cap = get_int_cap(irq);
	if (l4_is_invalid_cap(cap))
		return -ENOENT;

	s = kmalloc(sizeof(*s), GFP_KERNEL);
	if (!s)
		return -ENOMEM;

	s->irq    = irq;
	s->irqcap = cap;

	spin_lock_irqsave(&list_lock, flags);
	list_add(&s->list, &wakeup_srcs);
	spin_unlock_irqrestore(&list_lock, flags);

	return 0;
}

static int l4x_wakeup_source_unregister(int irq)
{
	struct wakeup_src *s, *tmp;
	unsigned long flags;
	int r = -EINVAL;

	list_for_each_entry_safe(s, tmp, &wakeup_srcs, list)
		if (irq == s->irq) {
			spin_lock_irqsave(&list_lock, flags);
			list_del(&s->list);
			spin_unlock_irqrestore(&list_lock, flags);
			r = 0;
			break;
		}

	return r;
}

static int l4x_pm_plat_prepare(void)
{
	return 0;
}

static void attach_to_irq(int irq, l4_cap_idx_t cap, l4_cap_idx_t t)
{
	int ret = L4XV_FN_e(l4_rcv_ep_bind_thread(cap, t, irq << 2));
	if (ret)
		printk("Failed to attach wakeup source %d/%lx: %d\n",
				irq, cap, ret);
	else
		printk("Attached to wakeup source %d(%lx)\n", irq, cap);
	L4XV_FN_v(l4_irq_unmask(cap));
}

static void detach_from_irq(int irq, l4_cap_idx_t cap)
{
	int ret = L4XV_FN_e(l4_irq_detach(cap));
	if (ret)
		printk("Failed to detach wakeup source %d: %d\n", irq, ret);
	else
		printk("Detached from wakeup source %d(%lx)\n", irq, cap);
}

static void loop_over_irqs(int doattach)
{
	int i;
	struct wakeup_src *s;
	l4_cap_idx_t t = l4x_cpu_thread_get_cap(smp_processor_id());

	if (suspend_irq_type == IRQ_TYPE_ALL_IRQS) {
		for (i = 0; i < NR_IRQS; ++i) {
			struct irq_desc *desc = irq_to_desc(i);
			l4_cap_idx_t cap = get_int_cap(i);
			if (!desc || l4_is_invalid_cap(cap))
				continue;

			if (desc->action
			    && desc->action->flags & __IRQF_TIMER)
				continue;

			if (doattach)
				attach_to_irq(i, cap, t);
			else
				detach_from_irq(i, cap);
		}
	} else {
		if (suspend_irq_type == IRQ_TYPE_LX_INFRA) {
			for (i = 0; i < NR_IRQS; ++i) {
				struct irq_data *data = irq_get_irq_data(i);
				struct irq_desc *desc;
				l4_cap_idx_t cap = get_int_cap(i);

				if (l4_is_invalid_cap(cap))
					continue;

				if (!irqd_is_wakeup_set(data))
					continue;

				desc = irq_to_desc(i);
				if (desc && desc->action &&
				    (desc->action->flags & IRQF_NO_SUSPEND))
					// still attached
					continue;

				if (doattach)
					attach_to_irq(i, cap, t);
				else
					detach_from_irq(i, cap);
			}
		}

		/* Do this always */
		list_for_each_entry(s, &wakeup_srcs, list) {
			struct irq_data *data = irq_get_irq_data(s->irq);
			if (suspend_irq_type == IRQ_TYPE_LX_INFRA
			    && irqd_is_wakeup_set(data))
				continue; /* already done above */

			if (doattach)
				attach_to_irq(s->irq, s->irqcap, t);
			else
				detach_from_irq(s->irq, s->irqcap);
		}
	}
}

static void l4x_pfc_suspend(void)
{
	l4_cap_idx_t pfc_cap;
	long err;

	l4x_re_resolve_name("pfc", &pfc_cap);
	if (l4_is_valid_cap(pfc_cap)
	    && (err = L4XV_FN_e(l4_platform_ctl_system_suspend(pfc_cap, 0))))
		L4XV_FN_v(LOG_printf("L4x: l4_platform_ctl_system_suspend "
		                     "failed: %ld\n", err));
}

static int l4x_pm_plat_enter(suspend_state_t state)
{
	if (state != PM_SUSPEND_MEM)
		return -EINVAL;

	loop_over_irqs(1);

	flush_cache_all();

	l4x_pfc_suspend();

	l4x_global_wait_save();

	loop_over_irqs(0);

	return 0;
}

static void l4x_pm_plat_finish(void)
{
	l4x_global_saved_event_inject();
}

static void l4x_pm_plat_wake(void)
{
}

static int l4x_pm_plat_valid(suspend_state_t state)
{
#ifdef CONFIG_SUSPEND
#ifdef CONFIG_L4_VCPU
	if (suspend_possible || suspend_irq_type)
		return suspend_valid_only_mem(state);
#endif
#endif
	return 0;
}

static struct platform_suspend_ops l4x_pm_plat_ops = {
	.prepare = l4x_pm_plat_prepare,
	.enter   = l4x_pm_plat_enter,
	.finish  = l4x_pm_plat_finish,
	.wake    = l4x_pm_plat_wake,
	.valid   = l4x_pm_plat_valid,
};

struct l4x_virtual_mem_struct {
	struct list_head list;
	unsigned long address;
	pte_t pte;
};

static LIST_HEAD(virtual_pages);
static DEFINE_SPINLOCK(virtual_pages_lock);

enum l4x_virtual_mem_type {
	L4X_VIRTUAL_MEM_TYPE_MAP,
	L4X_VIRTUAL_MEM_TYPE_UNMAP,
};

void l4x_virtual_mem_register(unsigned long address, pte_t pte)
{
	struct l4x_virtual_mem_struct *e;
	unsigned long flags;

	if (!(e = kmalloc(sizeof(*e), GFP_KERNEL)))
		BUG();
	e->address = address;
	e->pte     = pte;
	spin_lock_irqsave(&virtual_pages_lock, flags);
	list_add_tail(&e->list, &virtual_pages);
	spin_unlock_irqrestore(&virtual_pages_lock, flags);
}

void l4x_virtual_mem_unregister(unsigned long address)
{
	struct l4x_virtual_mem_struct *e, *tmp;

	/* move all regions that are to be removed to a local list under
	 * lock protection. Then free the memory without lock protection.
	 *
	 * Q: may there ever by more than one matching element in the list?
	 *    I assume not.
	 */
	LIST_HEAD(to_free);
	unsigned long flags;
	spin_lock_irqsave(&virtual_pages_lock, flags);
	list_for_each_entry_safe(e, tmp, &virtual_pages, list) {
		if (e->address == address) {
			list_del(&e->list);
			list_add(&e->list, &to_free);
		}
	}
	spin_unlock_irqrestore(&virtual_pages_lock, flags);

	list_for_each_entry_safe(e, tmp, &to_free, list) {
		list_del(&e->list);
		kfree(e);
	}
}

static void l4x_virtual_mem_handle_pages(enum l4x_virtual_mem_type t)
{
	struct l4x_virtual_mem_struct *e;
	list_for_each_entry(e, &virtual_pages, list) {
		if (t == L4X_VIRTUAL_MEM_TYPE_MAP) {
			if (0)
				l4x_printf("map virtual %lx -> %" PTE_VAL_FMTTYPE "x\n",
				           e->address, pte_val(e->pte));
			l4lx_memory_map_virtual_page(e->address, e->pte);
		} else {
			if (0)
				l4x_printf("unmap virtual %lx\n", e->address);
			l4lx_memory_unmap_virtual_page(e->address);
		}
	}
}


static int l4x_pm_suspend(void)
{
#ifndef CONFIG_L4_VCPU
	struct task_struct *p;
	for_each_process(p) {
		if (l4_is_invalid_cap(p->thread.l4x.user_thread_id))
			continue;

		// FIXME: destroy better, threads and task
		if (l4lx_task_delete_task(p->thread.l4x.user_thread_id))
			l4x_printf("Error deleting %s(%d)\n", p->comm, p->pid);
		if (l4lx_task_number_free(p->thread.l4x.user_thread_id))
			l4x_printf("Error freeing %s(%d)\n", p->comm, p->pid);
		p->thread.l4x.user_thread_id = L4_INVALID_CAP;
		l4x_printf("kicked %s(%d)\n", p->comm, p->pid);
	}
#endif

	// Actually only needed for s2d
	l4x_virtual_mem_handle_pages(L4X_VIRTUAL_MEM_TYPE_UNMAP);
	return 0;
}

static void l4x_pm_resume(void)
{
#ifndef CONFIG_L4_VCPU
	struct task_struct *p;
#endif

	// Actually only needed for s2d
	l4x_virtual_mem_handle_pages(L4X_VIRTUAL_MEM_TYPE_MAP);

	// needs fixing for vCPU mode if someone wants that
#ifndef CONFIG_L4_VCPU
	for_each_process(p) {
		l4_msgtag_t tag;
		l4_umword_t src_id;

		if (l4_is_invalid_cap(p->thread.l4x.user_thread_id))
			continue;

		if (l4lx_task_get_new_task(L4_INVALID_CAP,
		                           &p->thread.l4x.user_thread_id))
			l4x_printf("l4lx_task_get_new_task failed\n");
		if (l4lx_task_create(p->thread.l4x.user_thread_id))
			l4x_printf("l4lx_task_create for %s(%d) failed\n",
			           p->comm, p->pid);

		do {
			tag = l4_ipc_wait(l4_utcb(), &src_id, L4_IPC_SEND_TIMEOUT_0);
			if (l4_ipc_error(tag, l4_utcb()))
				l4x_printf("ipc error %lx\n", l4_ipc_error(tag, l4_utcb()));
		} while ( 1 ); //FIXME //!l4_thread_equal(src_id, p->thread.l4x.user_thread_id));

		l4x_printf("contacted %s(%d)\n", p->comm, p->pid);
	}
#endif
}

static struct syscore_ops l4x_pm_syscore_ops = {
	.suspend = l4x_pm_suspend,
	.resume  = l4x_pm_resume,
};

static ssize_t wakeup_irq_add_store(struct kobject *kobj,
                                    struct kobj_attribute *attr,
                                    const char *buf, size_t sz)
{
	unsigned irq;
	int r;

	r = kstrtouint(buf, 0, &irq);
	if (r)
		return r;

	if (irq >= NR_IRQS)
		return -EINVAL;

	if (list_empty(&wakeup_srcs))
		suspend_possible = 1;
	r = l4x_wakeup_source_register(irq);
	if (r)
		return r;
	return sz;
}

static ssize_t wakeup_irq_del_store(struct kobject *kobj,
                                    struct kobj_attribute *attr,
                                    const char *buf, size_t sz)
{
	unsigned irq;
	int r;

	r = kstrtouint(buf, 0, &irq);
	if (r)
		return r;

	if (irq >= NR_IRQS)
		return -EINVAL;

	r = l4x_wakeup_source_unregister(irq);
	if (!r)
		r = sz;
	if (list_empty(&wakeup_srcs))
		suspend_possible = 0;
	return r;
}

static ssize_t wakeup_srcs_show(struct kobject *kobj,
                                struct kobj_attribute *attr, char *buf)
{
	struct wakeup_src *s, *tmp;
	ssize_t c = 0;

	list_for_each_entry_safe(s, tmp, &wakeup_srcs, list)
		c += sprintf(buf + c, "%d\n", s->irq);

	return c;
}

static ssize_t wakeup_src_type_store(struct kobject *kobj,
                                     struct kobj_attribute *attr,
                                     const char *buf, size_t sz)
{
	int i = 0;
	for (; i < IRQ_TYPE_TYPES; ++i) {
		int l = strlen(wakeup_irq_types[i]);
		if (sz < l)
			l = sz;
		if (!strncmp(buf, wakeup_irq_types[i], l)) {
			suspend_irq_type = i;
			break;
		}
	}
	return sz;
}

static ssize_t wakeup_src_type_show(struct kobject *kobj,
                                    struct kobj_attribute *attr, char *buf)
{
	int i = 0;
	ssize_t s = 0;
	for (; i < IRQ_TYPE_TYPES; ++i) {
		const char *fmt = i == suspend_irq_type ? "%s[%s]" : "%s%s";
		s += sprintf(buf + s, fmt, i ? " " : "",
		             wakeup_irq_types[i]);

	}
	s += sprintf(buf + s, "\n");
	return s;
}

static struct kobj_attribute wakeup_src_add_attr =
	__ATTR(wakeup_irq_add,    0600, wakeup_srcs_show, wakeup_irq_add_store);
static struct kobj_attribute wakeup_src_del_attr =
	__ATTR(wakeup_irq_remove, 0600, wakeup_srcs_show, wakeup_irq_del_store);
static struct kobj_attribute wakeup_src_type_attr =
	__ATTR(wakeup_irq_type, 0600, wakeup_src_type_show, wakeup_src_type_store);

static __init int l4x_pm_init(void)
{
	int r;

	r = sysfs_create_file(power_kobj, &wakeup_src_add_attr.attr);
	if (r)
		pr_err("failed to create sysfs file: %d\n", r);

	r = sysfs_create_file(power_kobj, &wakeup_src_del_attr.attr);
	if (r)
		pr_err("failed to create sysfs file: %d\n", r);

	r = sysfs_create_file(power_kobj, &wakeup_src_type_attr.attr);
	if (r)
		pr_err("failed to create sysfs file: %d\n", r);

	register_syscore_ops(&l4x_pm_syscore_ops);
	suspend_set_ops(&l4x_pm_plat_ops);

	return 0;
}
arch_initcall(l4x_pm_init);
