/*
 * Functions implementing the API defined in asm/l4lxapi/thread.h
 */

#include <l4/sys/err.h>
#include <l4/sys/thread.h>
#include <l4/sys/scheduler.h>
#include <l4/sys/factory.h>
#include <l4/sys/task.h>
#include <l4/re/env.h>
#include <l4/re/c/rm.h>
#include <l4/log/log.h>
#include <l4/sys/debugger.h>

#include <asm/l4lxapi/thread.h>
#include <asm/l4lxapi/misc.h>
#include <asm/generic/kthreads.h>
#include <asm/generic/cap_alloc.h>
#include <asm/generic/smp.h>
#include <asm/generic/stack_id.h>
#include <asm/generic/vcpu.h>
#include <asm/generic/l4lib.h>
#include <asm/generic/jdb.h>
#include <asm/api/api.h>
#include <asm/api/macros.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>

struct free_block_t {
	l4_umword_t pad[L4_UTCB_GENERIC_DATA_SIZE - 1];
	struct free_block_t *next_free;
};

struct free_list_t {
	struct free_block_t *head;
	unsigned blk_sz;
};
static struct free_list_t ku_free_small = {
	.blk_sz = L4_UTCB_OFFSET,
};
#ifdef CONFIG_KVM
static struct free_list_t ku_free_big = {
	.blk_sz = L4_PAGESIZE,
};
#endif

static void enqueue_to_ku_mem_list(void *k, struct free_list_t *fl)
{
	struct free_block_t *f = k;
	f->next_free = fl->head;
	fl->head = f;
}

static void add_mem_to_free_list(l4_addr_t start, l4_addr_t end,
                                 unsigned sz, struct free_list_t *fl)
{
	l4_addr_t n = start;
	while (n + sz <= end) {
		enqueue_to_ku_mem_list((void *)n, fl);
		n += sz;
	}
}

void l4lx_thread_utcb_alloc_init(void)
{
	l4_fpage_t utcb_area = l4re_env()->utcb_area;
	l4_addr_t free_utcb = l4re_env()->first_free_utcb;
	l4_addr_t end       = ((l4_addr_t)l4_fpage_page(utcb_area) << 12UL)
	                      + (1UL << (l4_addr_t)l4_fpage_size(utcb_area));
	add_mem_to_free_list(free_utcb, end, L4_UTCB_OFFSET,
	                     &ku_free_small);

	/* Used up all initial slots... */
	l4re_env()->first_free_utcb = ~0UL;
}

static int alloc_kumem(l4_addr_t *v, unsigned pages_order)
{
	// this is nearly l4re_util_kumem_alloc but does not start to look
	// for free space at 0
	int r;
	unsigned sh = pages_order + L4_PAGESHIFT;

	r = l4re_rm_reserve_area(v, (1 << pages_order) * L4_PAGESIZE,
                                 L4RE_RM_RESERVED | L4RE_RM_SEARCH_ADDR,
	                         sh);
	if (r)
		return r;

	if ((r = l4_error(l4_task_add_ku_mem(L4_BASE_TASK_CAP,
	                                     l4_fpage(*v, sh, L4_FPAGE_RW))))) {
		l4re_rm_free_area(*v);
		return r;
	}

	return 0;
}

static int get_more_kumem(unsigned order_pages, unsigned blk_sz,
                          struct free_list_t *fl)
{
	l4_addr_t kumem = (l4_addr_t)l4_utcb();
	if (alloc_kumem(&kumem, order_pages))
		return 1;

	add_mem_to_free_list(kumem, kumem + (1 << order_pages) * L4_PAGESIZE,
	                     blk_sz, fl);
	return 0;
}

static void *l4lx_thread_ku_alloc_alloc(struct free_list_t *fl)
{
	struct free_block_t *n = fl->head;
	if (!n) {
		if (get_more_kumem(0, fl->blk_sz, fl))
			return 0;

		n = fl->head;
	}

	fl->head = n->next_free;
	memset(n, 0, fl->blk_sz);
	return n;
}

static void *l4lx_thread_ku_alloc_alloc_u(void)
{
	return l4lx_thread_ku_alloc_alloc(&ku_free_small);
}

#if defined(CONFIG_L4_VCPU) || defined(CONFIG_KVM)
static void *l4lx_thread_ku_alloc_alloc_v(void)
{
#ifdef CONFIG_KVM
	return l4lx_thread_ku_alloc_alloc(&ku_free_big);
#else
	return l4lx_thread_ku_alloc_alloc(&ku_free_small);
#endif
}
#endif


static void l4lx_thread_ku_alloc_free_u(void *k)
{
	return enqueue_to_ku_mem_list(k, &ku_free_small);
}

static void l4lx_thread_ku_alloc_free_v(void *k)
{
#ifdef CONFIG_KVM
	return enqueue_to_ku_mem_list(k, &ku_free_big);
#else
	return enqueue_to_ku_mem_list(k, &ku_free_small);
#endif
}

int l4lx_thread_start(struct l4lx_thread_start_info_t *s)
{
	l4_sched_param_t schedp = l4_sched_param(s->prio, 0);
	l4_msgtag_t res;

	schedp.affinity = l4_sched_cpu_set(s->pcpu, 0, 1);

	res = l4_scheduler_run_thread(l4re_env()->scheduler,
	                              s->l4cap, &schedp);
	if (l4_error(res)) {
		LOG_printf("l4lx: Failed to set pcpu%d of thread (%lx): %ld\n",
		           s->pcpu, s->l4cap >> L4_CAP_SHIFT, l4_error(res));
		return l4_error(res);
	}

	return l4_error(l4_thread_ex_regs(s->l4cap, s->ip, s->sp, 0));
}
EXPORT_SYMBOL(l4lx_thread_start);


#ifdef CONFIG_ARM
void __thread_launch(void);
asm(
"__thread_launch:\n"
"	ldmia sp!, {r0}\n" // arg1
"	ldmia sp!, {r1}\n" // func
"	ldmia sp!, {lr}\n" // ret
"	bic sp, sp, #7\n"
"	mov pc, r1\n"
);
#endif
#ifdef CONFIG_X86_64
void __thread_launch(void);
asm(
"__thread_launch:\n"
"	popq %rdi\n" // arg1
"	ret\n"
);
#endif

int l4lx_thread_create(l4lx_thread_t *thread,
                       L4_CV void (*thread_func)(void *data),
                       unsigned vcpu,
                       void *stack_pointer,
                       void *stack_data, unsigned stack_data_size,
                       l4_cap_idx_t l4cap, int prio,
                       l4_utcb_t *utcbp,
                       l4_vcpu_state_t **vcpu_state,
                       const char *name,
                       struct l4lx_thread_start_info_t *deferstart)
{
	int err;
	struct l4lx_thread_start_info_t si_buf, *si;
	l4_utcb_t *utcb;
	l4_umword_t *sp, *sp_data;
#if defined(CONFIG_L4_VCPU) || defined(CONFIG_KVM)
	int vcpu_state_allocated = 0;
#endif

	if (l4_is_invalid_cap(l4cap))
		return -EINVAL;

	err = l4_error(l4_factory_create_thread(l4re_env()->factory, l4cap));
	if (err)
		goto out_free_cap;

	if (!stack_pointer) {
		stack_pointer = l4lx_thread_stack_alloc(l4cap);
		if (!stack_pointer) {
			LOG_printf("l4x: Out of thread stacks\n");
			err = -ENOMEM;
			goto out_rel_cap;
		}
	}


	// the sub of 8 is x86 specific! fix this
	stack_pointer = (char *)stack_pointer - sizeof(struct pt_regs) - 8;

	sp_data = (l4_umword_t *)((char *)stack_pointer - stack_data_size);
	memcpy(sp_data, stack_data, stack_data_size);

	sp = sp_data;
#ifdef CONFIG_X86_64
	sp = (l4_umword_t *)((l4_umword_t)sp & ~0xf);
	*(--sp) = 0;
	*(--sp) = (l4_umword_t)thread_func;
	*(--sp) = (l4_umword_t)sp_data;
#elif defined(CONFIG_ARM)
	*(--sp) = 0;
	*(--sp) = (l4_umword_t)thread_func;
	*(--sp) = (l4_umword_t)sp_data;
#else
	*(--sp) = (l4_umword_t)sp_data;
	*(--sp) = 0;
#endif

	l4x_dbg_set_object_name(l4cap, "%s", name);
	err = -ENOMEM;
	if (!utcbp) {
		utcb = l4lx_thread_ku_alloc_alloc_u();
		if (!utcb)
			goto out_rel_cap;
	} else
		utcb = utcbp;

#if defined(CONFIG_L4_VCPU) || defined(CONFIG_KVM)
	if (vcpu_state && !*vcpu_state) {
		*vcpu_state = (l4_vcpu_state_t *)l4lx_thread_ku_alloc_alloc_v();
		vcpu_state_allocated = 1;
		if (!*vcpu_state)
			goto out_free_utcb;
	}
#endif

	*thread = utcb;
	mb();

	l4_utcb_tcr_u(utcb)->user[L4X_UTCB_TCR_ID]   = l4cap;
	l4_utcb_tcr_u(utcb)->user[L4X_UTCB_TCR_PRIO] = prio;

	l4_thread_control_start();
	l4_thread_control_pager(l4re_env()->rm);
	l4_thread_control_exc_handler(l4re_env()->rm);
	l4_thread_control_bind(utcb, L4_BASE_TASK_CAP);
	err = l4_error(l4_thread_control_commit(l4cap));
	if (err)
		goto out_free_vcpu;

#if defined(CONFIG_L4_VCPU) || defined(CONFIG_KVM)
	if (vcpu_state) {
		int vcpu_ext_not_avail = 0;
		l4_vcpu_state_t *v = *vcpu_state;

		v->state = 0;
		err = 1;

#ifdef CONFIG_KVM
		err = l4_error(l4_thread_vcpu_control_ext(l4cap, (l4_addr_t)v));
		vcpu_ext_not_avail = err == -L4_ENOSYS;
#endif
		if (err)
			err = l4_error(l4_thread_vcpu_control(l4cap,
			                                      (l4_addr_t)v));
		if (err)
			goto out_free_vcpu;

		if (vcpu_ext_not_avail)
			LOG_printf("Failed to init virtualization in "
			           "vCPU%d (%s), KVM won't work.\n",
			           vcpu, name);
	}
#endif

	LOG_printf("%s: Created thread " PRINTF_L4TASK_FORM " (%s) "
	           "(u:%08lx, "
#if defined(CONFIG_L4_VCPU) || defined(CONFIG_KVM)
	           "v:%08lx, "
#endif
	           "sp:%08lx)\n",
	           __func__, PRINTF_L4TASK_ARG(l4cap), name,
	           (l4_addr_t)utcb,
#if defined(CONFIG_L4_VCPU) || defined(CONFIG_KVM)
	           vcpu_state ? (l4_addr_t)(*vcpu_state) : 0,
#endif
	           (l4_umword_t)sp);

	si = deferstart ? deferstart : &si_buf;;

	si->l4cap = l4cap;
	si->sp    = (l4_umword_t)sp;
	si->prio  = prio;
	si->pcpu  = l4x_cpu_physmap_get_id(vcpu);
#if defined(CONFIG_ARM) || defined(CONFIG_X86_64)
	si->ip    = (l4_umword_t)__thread_launch;
#else
	si->ip    = (l4_umword_t)thread_func;
#endif

	if (!deferstart)
		if ((err = l4lx_thread_start(si)))
			goto out_free_vcpu;

	return 0;

out_free_vcpu:
#if defined(CONFIG_L4_VCPU) || defined(CONFIG_KVM)
	if (vcpu_state_allocated)
		l4lx_thread_ku_alloc_free_v(*vcpu_state);

out_free_utcb:
#endif
	if (!utcbp)
		l4lx_thread_ku_alloc_free_u(utcb);
out_rel_cap:
	l4_task_delete_obj(L4RE_THIS_TASK_CAP, l4cap);
out_free_cap:
	l4x_cap_free(l4cap);

	return err;
}
EXPORT_SYMBOL(l4lx_thread_create);

/*
 * l4lx_thread_pager_change
 */
void l4lx_thread_pager_change(l4_cap_idx_t thread, l4_cap_idx_t pager)
{
	l4_utcb_t *u = l4_utcb();

	l4_thread_control_start_u(u);
	l4_thread_control_pager_u(pager, u);
	l4_thread_control_exc_handler_u(pager, u);
	l4_thread_control_commit_u(thread, u);
}

/*
 * l4lx_thread_set_kernel_pager
 */
void l4lx_thread_set_kernel_pager(l4_cap_idx_t thread)
{
	l4lx_thread_pager_change(thread, l4x_start_thread_id);
}


/*
 * l4lx_thread_shutdown
 */
void l4lx_thread_shutdown(l4lx_thread_t u, unsigned free_utcb,
                          void *v, int do_cap_free)
{
	l4_cap_idx_t threadcap = l4lx_thread_get_cap(u);

	/* free "stack memory" used for data if there's some */
	l4lx_thread_stack_return(threadcap);

	if (free_utcb)
		l4lx_thread_ku_alloc_free_u(u);
	if (v)
		l4lx_thread_ku_alloc_free_v((l4_vcpu_state_t *)v);

	l4_task_delete_obj(L4RE_THIS_TASK_CAP, threadcap);
	if (do_cap_free)
		l4x_cap_free(threadcap);
}
EXPORT_SYMBOL(l4lx_thread_shutdown);
