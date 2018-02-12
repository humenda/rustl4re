
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/param.h>
#include <linux/smp.h>
#include <linux/timex.h>
#include <linux/clockchips.h>
#include <linux/irq.h>
#include <linux/cpu.h>
#include <linux/sched_clock.h>

#include <asm/l4lxapi/thread.h>

#include <asm/generic/cap_alloc.h>
#include <asm/generic/setup.h>
#include <asm/generic/smp.h>
#include <asm/generic/vcpu.h>
#include <asm/generic/timer.h>

#include <l4/sys/factory.h>
#include <l4/sys/irq.h>
#include <l4/sys/task.h>
#include <l4/log/log.h>
#include <l4/re/env.h>

enum { PRIO_TIMER = CONFIG_L4_PRIO_IRQ_BASE + 1 };

enum Protos
{
	L4X_PROTO_TIMER = 19L,
};

enum L4x_timer_ops
{
	L4X_TIMER_OP_START = 0UL,
	L4X_TIMER_OP_STOP  = 1UL,
};

typedef unsigned long long l4timer_time_t;
static int timer_irq;

static struct clock_event_device __percpu *timer_evt;
static l4_cap_idx_t __percpu *timer_irq_caps;
static DEFINE_PER_CPU(l4_cap_idx_t, timer_srv);
static DEFINE_PER_CPU(l4lx_thread_t, timer_threads);

static inline l4_timeout_t next_timeout(l4_cpu_time_t next_to, l4_utcb_t *u)
{
	return l4_timeout(L4_IPC_TIMEOUT_0, l4_timeout_abs_u(next_to, 1, u));
}

static void L4_CV timer_thread(void *data)
{
	l4_timeout_t to;
	l4_utcb_t *u = l4_utcb();
	l4_cap_idx_t irq_cap = *(l4_cap_idx_t *)data;
	l4_msgtag_t t;
	l4_umword_t l;
	l4_msg_regs_t *v = l4_utcb_mr_u(u);
	l4timer_time_t increment = 0;
	l4_cpu_time_t next_to = 0;
	enum {
		idx_base_mr   = 2,
		mr_i          = sizeof(v->mr64[0]) / sizeof(v->mr[0]),
	};
	const unsigned idx_at = l4_utcb_mr64_idx(idx_base_mr);
	const unsigned idx_increment = l4_utcb_mr64_idx(idx_base_mr + mr_i);

	to = L4_IPC_NEVER;
	t = l4_ipc_wait(u, &l, to);
	while (1) {
		int reply = 1;
		int r = 0;

		if (l4_ipc_error(t, u) == L4_IPC_RETIMEOUT) {
			if (l4_ipc_error(l4_irq_trigger_u(irq_cap, u), u))
				LOG_printf("IRQ timer trigger failed\n");

			if (increment) {
				next_to += increment;
				to = next_timeout(next_to, u);
			} else
				to = L4_IPC_NEVER;
			reply = 0;
		} else if (l4_error(t) == L4X_PROTO_TIMER) {
			switch (v->mr[0]) {
				case L4X_TIMER_OP_START:
					next_to = v->mr64[idx_at];
					to = next_timeout(next_to, u);
					increment = v->mr64[idx_increment];
					r = 0;
					break;
				case L4X_TIMER_OP_STOP:
					to = L4_IPC_NEVER;
					increment = 0;
					r = 0;
					break;
				default:
					LOG_printf("l4timer: invalid opcode\n");
					r = -ENOSYS;
					break;
			};
		} else
			LOG_printf("l4timer: msg r=%ld\n", l4_error(t));

		t = l4_msgtag(r, 0, 0, 0);
		if (reply)
			t = l4_ipc_reply_and_wait(u, t, &l, to);
		else
			t = l4_ipc_wait(u, &l, to);
	}
}

static l4_msgtag_t l4timer_start_u(l4_cap_idx_t timer, unsigned flags,
                                   l4timer_time_t at, l4timer_time_t increment,
                                   l4_utcb_t *utcb)
{
	int idx = 2;
	l4_msg_regs_t *v = l4_utcb_mr_u(utcb);
	v->mr[0] = L4X_TIMER_OP_START;
	v->mr[1] = flags;
	v->mr64[l4_utcb_mr64_idx(idx)] = at;
	idx += sizeof(v->mr64[0]) / sizeof(v->mr[0]);
	v->mr64[l4_utcb_mr64_idx(idx)] = increment;
	idx += sizeof(v->mr64[0]) / sizeof(v->mr[0]);
	return l4_ipc_call(timer, utcb,
	                   l4_msgtag(L4X_PROTO_TIMER, idx, 0, 0),
	                   L4_IPC_NEVER);
}

static l4_msgtag_t l4timer_stop_u(l4_cap_idx_t timer, l4_utcb_t *utcb)
{
	l4_msg_regs_t *v = l4_utcb_mr_u(utcb);
	v->mr[0] = L4X_TIMER_OP_STOP;
	return l4_ipc_call(timer, utcb, l4_msgtag(L4X_PROTO_TIMER, 1, 0, 0),
                           L4_IPC_NEVER);
}

static l4_msgtag_t l4timer_start(l4_cap_idx_t timer, unsigned flags,
                                 l4timer_time_t at, l4timer_time_t increment)
{
	return l4timer_start_u(timer, flags, at, increment, l4_utcb());
}

static l4_msgtag_t l4timer_stop(l4_cap_idx_t timer)
{
	return l4timer_stop_u(timer, l4_utcb());
}


static int timer_set_oneshot(struct clock_event_device *clk)
{
	int r;
	r = L4XV_FN_e(l4timer_stop(this_cpu_read(timer_srv)));
	if (r) {
		pr_warn("l4timer: stop failed (%d)\n", r);
		return r;
	}

	while (L4XV_FN_i(l4_ipc_error(l4_ipc_receive(*this_cpu_ptr(timer_irq_caps),
	                                             l4_utcb(), L4_IPC_BOTH_TIMEOUT_0),
	                              l4_utcb())) != L4_IPC_RETIMEOUT)
		;

	return 0;
}

static int timer_set_periodic(struct clock_event_device *clk)
{
	const l4timer_time_t increment = 1000000 / HZ;
	int r;
	r = L4XV_FN_e(l4timer_start(this_cpu_read(timer_srv), 0,
	                            l4_kip_clock(l4lx_kinfo),
	                            increment));
	if (r)
		pr_warn("l4timer: start failed (%d)\n", r);
	return r;
}

static int timer_set_next_event(unsigned long evt,
                                struct clock_event_device *unused)
{
	int r;
	r = L4XV_FN_e(l4timer_start(this_cpu_read(timer_srv), 0,
	                            l4_kip_clock(l4lx_kinfo) + evt,
	                            0));
	if (r)
		pr_warn("l4timer: start failed (%d)\n", r);
	return 0;
}

static int timer_clock_event_init(struct clock_event_device *clk)
{
	int r;
	l4_cap_idx_t irq_cap;
	unsigned cpu = smp_processor_id();
	l4lx_thread_t thread;
	char s[12];

	irq_cap = l4x_cap_alloc();
	if (l4_is_invalid_cap(irq_cap))
		return -ENOMEM;

	r = L4XV_FN_e(l4_factory_create_irq(l4re_env()->factory, irq_cap));
	if (r)
		goto out1;

	*this_cpu_ptr(timer_irq_caps) = irq_cap;

	snprintf(s, sizeof(s), "timer%d", cpu);
	s[sizeof(s) - 1] = 0;

	if (L4XV_FN_i(l4lx_thread_create(&thread, timer_thread,
                                         cpu, NULL, &irq_cap, sizeof(irq_cap),
                                         l4x_cap_alloc(), PRIO_TIMER,
                                         0, 0, s, NULL))) {
		r = -ENOMEM;
		goto out2;
	}

	this_cpu_write(timer_threads, thread);
	this_cpu_write(timer_srv, l4lx_thread_get_cap(thread));

	clk->features           = CLOCK_EVT_FEAT_ONESHOT
	                          | CLOCK_EVT_FEAT_PERIODIC;
	clk->name               = "l4-timer";
	clk->rating             = 300;
	clk->cpumask            = cpumask_of(cpu);
	clk->irq                = timer_irq;
	clk->set_next_event     = timer_set_next_event;
	clk->set_state_shutdown = timer_set_oneshot,
	clk->set_state_oneshot  = timer_set_oneshot,
	clk->set_state_periodic = timer_set_periodic,
	clk->tick_resume        = timer_set_periodic,

	enable_percpu_irq(timer_irq, IRQ_TYPE_NONE);

	clk->set_state_shutdown(clk);
	clockevents_config_and_register(clk, 1000000, 1, ~0UL);

	return 0;

out2:
	L4XV_FN_v(l4_task_release_cap(L4RE_THIS_TASK_CAP, irq_cap));
out1:
	l4x_cap_free(irq_cap);

	return r;
}

static void timer_clock_stop(struct clock_event_device *clk)
{
	l4lx_thread_t t = this_cpu_read(timer_threads);

	disable_percpu_irq(clk->irq);
	clk->set_state_shutdown(clk);

	L4XV_FN_v(l4lx_thread_shutdown(t, 1, NULL, 1));
	this_cpu_write(timer_threads, NULL);

	L4XV_FN_v(l4_task_release_cap(L4RE_THIS_TASK_CAP,
	                              *this_cpu_ptr(timer_irq_caps)));
	l4x_cap_free(*this_cpu_ptr(timer_irq_caps));
	*this_cpu_ptr(timer_irq_caps) = L4_INVALID_CAP;
}

static irqreturn_t timer_interrupt_handler(int irq, void *dev_id)
{
	struct clock_event_device *evt = dev_id;
	evt->event_handler(evt);
	return IRQ_HANDLED;
}

static int l4x_timer_starting_cpu(unsigned cpu)
{
	BUG_ON(cpu != smp_processor_id());
	timer_clock_event_init(this_cpu_ptr(timer_evt));
	return 0;
}

static int l4x_timer_dying_cpu(unsigned cpu)
{
	BUG_ON(cpu != smp_processor_id());
	timer_clock_stop(this_cpu_ptr(timer_evt));
	return 0;
}

static int __init l4x_timer_init_ret(void)
{
	int r;

	if ((timer_irq = l4x_alloc_percpu_irq(&timer_irq_caps)) < 0)
		return -ENOMEM;

	pr_info("l4timer: Using IRQ%d\n", timer_irq);

	timer_evt = alloc_percpu(struct clock_event_device);
	if (!timer_evt) {
		r = -ENOMEM;
		goto out1;
	}

	r = request_percpu_irq(timer_irq, timer_interrupt_handler,
	                       "L4-timer", timer_evt);
	if (r < 0)
		goto out2;

	r = cpuhp_setup_state(CPUHP_AP_ARM_ARCH_TIMER_STARTING,
	                      "l4x/timer",
	                      l4x_timer_starting_cpu, l4x_timer_dying_cpu);

	if (r)
		goto out3;

	return 0;

out3:
	free_percpu_irq(timer_irq, timer_evt);
out2:
	free_percpu(timer_evt);
out1:
	l4x_free_percpu_irq(timer_irq);
	return r;
}

#ifdef CONFIG_GENERIC_SCHED_CLOCK
static u64 kip_clock_read(void)
{
	return l4_kip_clock(l4re_kip());
}
#endif

static u64 l4x_clk_read(struct clocksource *cs)
{
	return l4_kip_clock(l4re_kip());
}

static struct clocksource kipclk_cs = {
	.name           = "l4kipclk",
	.rating         = 200,
	.read           = l4x_clk_read,
	.mask           = CLOCKSOURCE_MASK(64),
	.mult           = 1000,
	.flags          = CLOCK_SOURCE_IS_CONTINUOUS,
};

void __init l4x_timer_init(void)
{
	if (clocksource_register_hz(&kipclk_cs, 1000000))
		pr_err("l4timer: Failed to register clocksource\n");

	if (l4x_timer_init_ret())
		pr_err("l4timer: Failed to initialize!\n");

#ifdef CONFIG_GENERIC_SCHED_CLOCK
	sched_clock_register(kip_clock_read, 64, 1000000);
#endif
}
