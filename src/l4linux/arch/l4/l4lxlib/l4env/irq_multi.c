/*
 * Multi IRQ implementation. We only use one single IRQ thread.
 */

#include <linux/bitops.h>
#include <linux/init.h>
#include <linux/interrupt.h>

#include <l4/sys/irq.h>
#include <l4/sys/icu.h>

#include <asm/api/config.h>
#include <asm/api/macros.h>

#include <asm/l4lxapi/irq.h>
#include <asm/l4lxapi/thread.h>

#include <asm/generic/setup.h>
#include <asm/generic/do_irq.h>
#include <asm/generic/cap_alloc.h>
#include <asm/generic/io.h>
#include <asm/generic/smp.h>

#include <l4/log/log.h>


#define d_printk(format, args...)  do { printk(format , ## args); } while (0)
#define dd_printk(format, args...) do { if (0) printk(format , ## args); } while (0)

enum irq_cmds {
	CMD_IRQ_ENABLE  = 1,
	CMD_IRQ_DISABLE = 2,
	CMD_IRQ_UPDATE  = 3,
};

static l4_umword_t irq_state[NR_IRQS];
static l4lx_thread_t irq_ths[NR_CPUS];
static l4_cap_idx_t irq_caps[NR_IRQS];
static l4_umword_t irq_disable_cmd_state[NR_IRQS]; /* Make this a bitmask?! */

#ifdef CONFIG_SMP
static unsigned     irq_cpus[NR_IRQS];

static inline void set_irq_cpu(unsigned irq, unsigned cpu)
{
        irq_cpus[irq] = cpu;
}
static inline unsigned get_irq_cpu(unsigned irq)
{
        return irq_cpus[irq];
}
int l4lx_irq_dev_set_affinity(struct irq_data *data,
                              const struct cpumask *dest, bool force)
{
	return 0;
}
#else
static inline void set_irq_cpu(unsigned irq, unsigned cpu) { (void)irq; (void)cpu; }
static inline unsigned get_irq_cpu(unsigned irq) { (void)irq; return 0; }
#endif



/*
 * Return the priority of an interrupt thread.
 */
int l4lx_irq_prio_get(unsigned int irq)
{
	return CONFIG_L4_PRIO_IRQ_BASE;
}


static void attach_to_IRQ(unsigned int irq)
{
	int ret;

	/* Associate INTR */
	if ((ret = l4_error(l4_rcv_ep_bind_thread(irq_caps[irq],
	                                          l4lx_thread_get_cap(irq_ths[0 * smp_processor_id()]),
	                                          irq << 2))))
		dd_printk("%s: can't register to irq %u: return=%d\n",
		          __func__, irq, ret);
	if ((ret = l4_error(l4_irq_unmask(irq_caps[irq]))) != L4_PROTO_IRQ)
		dd_printk("unmask of irq %d failed err=%x\n", irq, ret);

	irq_state[irq] = 1;
}

static void detach_from_interrupt(unsigned irq)
{
	if (l4_error(l4_irq_detach(irq_caps[irq])))
		dd_printk("%02d: Unable to detach from IRQ\n", irq);
	irq_disable_cmd_state[irq] = irq_state[irq] = 0;
}


/* Send a command to the interrupt thread. */
static void send_msg_to_irq_thread(unsigned int irq, unsigned int cpu,
                                   enum irq_cmds cmd)
{
	l4_msgtag_t tag;

	if (irq >= NR_IRQS) {
		printk("%s: IRQ %d is too big/not available.\n", __func__, irq);
		return;
	}

	do {
		l4_utcb_mr()->mr[0] = irq;
		tag = l4_ipc_send(l4lx_thread_get_cap(irq_ths[0 * smp_processor_id()]),
		                  l4_utcb(),
		                  l4_msgtag(cmd, 1, 0, 0), L4_IPC_NEVER);

		if (l4_ipc_error(tag, l4_utcb()))
			printk("Failure while trying to send to irq thread,"
			       " retrying\n");

	} while (l4_ipc_error(tag, l4_utcb()));
}

/*
 * Wait for an interrupt to arrive
 */
static inline unsigned int
wait_for_irq_message(unsigned cpu, unsigned int irq_to_ack)
{
	int cmd, err;
	l4_umword_t label;
	l4_msgtag_t tag;
	l4_utcb_t *utcb = l4_utcb();

	while (1) {
		if (unlikely(irq_state[irq_to_ack]
		             && irq_disable_cmd_state[irq_to_ack]))
			detach_from_interrupt(irq_to_ack);
		else if (irq_to_ack)
			l4_irq_unmask(irq_caps[irq_to_ack]);

		tag = l4_ipc_wait(utcb, &label, L4_IPC_NEVER);
		err = l4_ipc_error(tag, utcb);

		if (unlikely(err)) {
			LOG_printf("%s: IPC error (0x%x)\n", __func__, err);
		} else if (likely(label)) {
			int irq = label >> 2;
			if (likely(irq_state[irq]))
				return irq;
			d_printk("Invalid message to IRQ thread %d\n", irq);
		} else {
			int irq = l4_utcb_mr_u(utcb)->mr[0];

			if (irq >= NR_IRQS) {
				LOG_printf("IRQ: %d invalid\n", irq);
				enter_kdebug("bIG");
			}
			cmd = l4_msgtag_label(tag);

			/* Non-IRQ message, handle */

			if (cmd == CMD_IRQ_ENABLE) {
			} else if (cmd == CMD_IRQ_DISABLE) {
				if (irq_state[irq]
				    && irq_disable_cmd_state[irq]) {
				}
			} else if (cmd == CMD_IRQ_UPDATE) {
				l4x_prepare_irq_thread(current_stack_pointer,
				                       get_irq_cpu(irq));
			} else
				LOG_printf("Unknown cmd: %u %lu\n", cmd, irq_state[irq]);
		}
	}
} /* wait_for_irq_message */

/*
 * IRQ thread, here we sit in a loop waiting to handle
 * incoming interrupts
 */
static L4_CV void irq_thread(void *data)
{
	unsigned cpu = *(unsigned *)data;
	unsigned int irq = 0;

	l4x_prepare_irq_thread(current_stack_pointer, 0);

	d_printk("%s: Starting IRQ thread on CPU %d\n", __func__, cpu);

	for (;;) {
		irq = wait_for_irq_message(cpu, irq);
		l4x_do_IRQ(irq);
	}
} /* irq_thread */

/* ############################################################# */

/*
 * Common stuff.
 */

void l4lx_irq_init(void)
{
	int cpu = smp_processor_id();
	char thread_name[11];

	/* Start IRQ thread */
	d_printk("%s: creating IRQ thread on cpu %d\n", __func__, cpu);

	snprintf(thread_name, sizeof(thread_name), "IRQ CPU%d", cpu);
	thread_name[sizeof(thread_name) - 1] = 0;
	if (l4lx_thread_create(&irq_ths[cpu],
	                       irq_thread, cpu, NULL,
	                       &cpu, sizeof(cpu),
	                       l4x_cap_alloc(),
	                       l4lx_irq_prio_get(1),
	                       0, 0, thread_name, NULL))
		enter_kdebug("Error creating IRQ-thread!");
}

unsigned int l4lx_irq_icu_startup(struct irq_data *data)
{
	unsigned irq = data->irq;
	struct l4x_irq_desc_private *p = irq_get_chip_data(irq);

	BUG_ON(!l4_is_invalid_cap(irq_caps[irq]));

	irq_caps[irq] = l4x_irq_init(p->icu, irq, p->trigger, "icu");
	if (l4_is_invalid_cap(irq_caps[irq])) {
		pr_err("l4x-irq: Failed to initialize IRQ %d\n", irq);
		return -ENXIO;
	}

	l4lx_irq_dev_enable(data);
	return 0;
}

unsigned int l4lx_irq_plain_startup(struct irq_data *data)
{
	l4lx_irq_dev_enable(data);
	return 0;
}

void l4lx_irq_icu_shutdown(struct irq_data *data)
{
	l4lx_irq_dev_disable(data);
	l4x_irq_release(irq_caps[data->irq]);
	l4x_cap_free(irq_caps[data->irq]);
	irq_caps[data->irq] = L4_INVALID_CAP;
}

void l4lx_irq_plain_shutdown(struct irq_data *data)
{
}

void l4lx_irq_dev_enable(struct irq_data *data)
{
	dd_printk("%s: %u\n", __func__, data->irq);

	if (!irq_state[data->irq]) {
		// actually we would also need to wake-up the IRQ thread...
		// but the timer will do it sometimes...
		irq_caps[data->irq] = l4x_have_irqcap(data->irq, 0);
		BUG_ON(l4_is_invalid_cap(irq_caps[data->irq]));
		attach_to_IRQ(data->irq);
		//send_msg_to_irq_thread(irq, smp_processor_id(), CMD_IRQ_ENABLE);
	}
}

void l4lx_irq_dev_disable(struct irq_data *data)
{
	dd_printk("%s: %u\n", __func__, data->irq);
	if (irq_state[data->irq])
		send_msg_to_irq_thread(data->irq, smp_processor_id(), CMD_IRQ_DISABLE);
}


void l4lx_irq_dev_ack(struct irq_data *data)
{
}

void l4lx_irq_dev_mask(struct irq_data *data)
{
}

void l4lx_irq_dev_mask_ack(struct irq_data *data)
{
}

void l4lx_irq_dev_unmask(struct irq_data *data)
{
}

void l4lx_irq_dev_eoi(struct irq_data *data)
{
	dd_printk("%s: %u\n", __func__, data->irq);
	//l4_irq_unmask(irq_caps[irq]);
}
