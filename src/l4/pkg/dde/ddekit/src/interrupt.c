/*
 * This file is part of DDEKit.
 *
 * (c) 2006-2012 Bjoern Doebel <doebel@os.inf.tu-dresden.de>
 *               Christian Helmuth <ch12@os.inf.tu-dresden.de>
 *               Thomas Friebel <tf13@os.inf.tu-dresden.de>
 *     economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

/*
 * \brief   Hardware-interrupt subsystem
 *
 * FIXME could intloop_param freed after startup?
 * FIXME use consume flag to indicate IRQ was handled
 */

#include <l4/dde/ddekit/interrupt.h>
#include <l4/dde/ddekit/semaphore.h>
#include <l4/dde/ddekit/thread.h>
#include <l4/dde/ddekit/memory.h>
#include <l4/dde/ddekit/panic.h>
#include <l4/dde/ddekit/printf.h>

#include <l4/re/c/util/cap_alloc.h>
#include <l4/sys/irq.h>
#include <l4/sys/icu.h>
#include <l4/util/util.h>
#include <l4/io/io.h>
#include <l4/irq/irq.h>

#include <stdio.h>

#include <l4/sys/debugger.h>
#include <l4/sys/kdebug.h>
#include <pthread-l4.h>

#define MAX_INTERRUPTS   128

#define DEBUG_INTERRUPTS  0
/*
 * Internal type for interrupt loop parameters
 */
struct intloop_params
{
	unsigned          irq;       /* irq number */
	int               shared;    /* irq sharing supported? */
	void(*thread_init)(void *);  /* thread initialization */
	void(*handler)(void *);      /* IRQ handler function */
	void             *priv;      /* private token */ 
	ddekit_sem_t     *started1;
	ddekit_sem_t     *started;

	int               start_err;
};

static struct
{
	int               handle_irq; /* nested irq disable count */
	ddekit_sem_t     *irqsem;     /* synch semaphore */
	ddekit_sem_t	 *stopsem;    /* stop semaphore */
	ddekit_thread_t  *irq_thread; /* thread ID for detaching from IRQ later on */
	l4_cap_idx_t	  irq_cap;   /* capability of IRQ object */
	unsigned	 trigger;	/* trigger mode control */
	struct intloop_params   *params;
} ddekit_irq_ctrl[MAX_INTERRUPTS];


#if 0
static void ddekit_irq_exit_fn(l4_cap_idx_t thread __attribute__((unused)),
                               void *data)
{
	int idx = (int)data;

	// * detach from IRQ
	l4_irq_detach(ddekit_irq_ctrl[idx].irq_cap);
}

L4THREAD_EXIT_FN_STATIC(exit_fn, ddekit_irq_exit_fn);
#endif

static int do_irq_attach(int irq)
{
  ddekit_irq_ctrl[irq].irq_cap = l4re_util_cap_alloc();

  if (l4_is_invalid_cap(ddekit_irq_ctrl[irq].irq_cap))
    return -1;

  if (l4io_request_irq(irq, ddekit_irq_ctrl[irq].irq_cap))
    return -2;

  pthread_t pthread = (pthread_t)ddekit_thread_get_id(ddekit_irq_ctrl[irq].irq_thread);
  l4_cap_idx_t thread_cap = pthread_l4_cap(pthread);
  if (l4_msgtag_has_error(l4_rcv_ep_bind_thread(ddekit_irq_ctrl[irq].irq_cap,
                                                thread_cap,
                                                irq /*ddekit_irq_ctrl[irq].trigger, */)))
    return -3;

  return 0;
}

static int do_irq_detach(int irq)
{
  if (l4_msgtag_has_error(l4_irq_detach(ddekit_irq_ctrl[irq].irq_cap)))
    return -3;

  if (l4io_release_irq(irq, ddekit_irq_ctrl[irq].irq_cap))
    return -2;

  l4re_util_cap_free(ddekit_irq_ctrl[irq].irq_cap);

  ddekit_irq_ctrl[irq].irq_cap = L4_INVALID_CAP;

  return 0;
}

static l4_umword_t do_irq_wait(int irq)
{
  l4_msgtag_t res;
  l4_umword_t label;
  
  do
    {
      res = l4_irq_wait(ddekit_irq_ctrl[irq].irq_cap, &label, L4_IPC_NEVER);
#if DEBUG_INTERRUPTS
      if (l4_ipc_error(res, l4_utcb()))
	ddekit_printf("do_irq_wait returned %d\n", l4_ipc_error(res, l4_utcb()));
#endif
    } while (l4_ipc_error(res, l4_utcb()));

  return label;
}

/**
 * Interrupt service loop
 *
 */
static void intloop(void *arg)
{
	struct intloop_params *params = arg;

	printf("Thread 0x%lx for IRQ %d\n", l4_debugger_global_id(pthread_l4_cap(pthread_self())), params->irq);

	/* wait for main thread thread to fill irqctrl structure */
	ddekit_sem_down(params->started1);
	
	int my_index = params->irq;

	/* allocate irq */
	int ret = do_irq_attach(my_index);
	if (ret < 0) {
		/* inform thread creator of error */
		/* XXX does error code have any meaning to DDEKit users? */
		params->start_err = ret;
		ddekit_sem_up(params->started);
		return;
	}

	// XXX IMPLEMENT EXIT FN?
#if 0
	/* 
	 * Setup an exit fn. This will make sure that we clean up everything,
	 * before shutting down an IRQ thread.
	 */
	if (l4thread_on_exit(&exit_fn, (void *)my_index) < 0)
		ddekit_panic("Could not set exit handler for IRQ fn.");
#endif

	/* after successful initialization call thread_init() before doing anything
	 * else here */
	if (params->thread_init) params->thread_init(params->priv);

	/* save handle + inform thread creator of success */
	params->start_err = 0;
	ddekit_sem_up(params->started);

#if 0
	/* prepare request structure */
	req.s.param   = params->irq + 1;
	req.s.wait    = 1;
	req.s.consume = 0;
	req.s.unmask  = 1;
#endif

	while (1) {
		l4_umword_t label;

		/* wait for int */
		label = do_irq_wait(my_index);

		/* if label == 0, than the interrupt should be disabled */
		if (!label) {
		    break;
		  }
#if DEBUG_INTERRUPTS
		ddekit_printf("received irq 0x%X\n", params->irq);
#endif

		/* unmask only the first time */
		//req.s.unmask = 0;

		/* only call registered handler function, if IRQ is not disabled */
		ddekit_sem_down(ddekit_irq_ctrl[my_index].irqsem);
		if (ddekit_irq_ctrl[my_index].handle_irq > 0) {
#if DEBUG_INTERRUPTS
		    ddekit_printf("handling IRQ 0x%X\n", params->irq);
#endif
		    params->handler(params->priv);
		}
		else {
#if DEBUG_INTERRUPTS
			ddekit_printf("not handling IRQ %x, because it is disabled (%d).\n",
			              my_index, ddekit_irq_ctrl[my_index].handle_irq);
#endif
		  }
		ddekit_sem_up(ddekit_irq_ctrl[my_index].irqsem);
	}
	
	int res = do_irq_detach(my_index);
	if (res)
	  {
	    printf("do_irq_detach failed with:%d\n", res);
	    ddekit_panic("detach from irq failed\n");
	  }
	
	/* signal successful detach from irq */
	ddekit_sem_up(ddekit_irq_ctrl[my_index].stopsem);

	l4_sleep_forever();
}


/**
 * Attach to hardware interrupt
 *
 * \param irq          IRQ number to attach to
 * \param shared       set to 1 if interrupt sharing is supported; set to 0
 *                     otherwise
 * \param thread_init  called just after DDEKit internal init and before any
 *                     other function
 * \param handler      IRQ handler for interrupt irq
 * \param priv         private token (argument for thread_init and handler)
 *
 * \return pointer to interrupt thread created
 */
ddekit_thread_t *ddekit_interrupt_attach(int irq, int shared,
                                         void(*thread_init)(void *),
                                         void(*handler)(void *), void *priv)
{
	struct intloop_params *params;
	ddekit_thread_t *thread;
	char thread_name[10];

	if (irq >= MAX_INTERRUPTS)
	  {
#if DEBUG_INTERRUPTS
	    ddekit_printf("IRQ: Interrupt number out of range\n");
#endif
	    return NULL;
	  }

	/* initialize info structure for interrupt loop */
	params = ddekit_simple_malloc(sizeof(*params));
	if (!params) return NULL;

	params->irq         = irq;
	params->thread_init = thread_init;
	params->handler     = handler;
	params->priv        = priv;
	params->started1    = ddekit_sem_init(0);
	params->started     = ddekit_sem_init(0);
	params->start_err   = 0;
	params->shared      = shared;

	/* construct name */
	snprintf(thread_name, 10, "irq%02X", irq);

	/* create interrupt loop thread */
	thread = ddekit_thread_create(intloop, params, thread_name, DDEKIT_IRQ_PRIO);
	if (!thread) {
		ddekit_simple_free(params);
		return NULL;
	}
	
	ddekit_irq_ctrl[irq].handle_irq = 1; /* IRQ nesting level is initially 1 */
	ddekit_irq_ctrl[irq].irq_thread = thread;
	ddekit_irq_ctrl[irq].irqsem     = ddekit_sem_init(1);
	ddekit_irq_ctrl[irq].stopsem	= ddekit_sem_init(0);
	ddekit_irq_ctrl[irq].params	= params;

	/* tigger intloop initialization */
	ddekit_sem_up(params->started1);

	/* wait for intloop initialization result */
	ddekit_sem_down(params->started);
	ddekit_sem_deinit(params->started);
	ddekit_sem_deinit(params->started1);
	if (params->start_err) {
		ddekit_simple_free(params);
		return NULL;
	}

	return thread;
}

/**
 * Detach from interrupt by disabling it and then shutting down the IRQ
 * thread.
 */
void ddekit_interrupt_detach(int irq)
{
	//ddekit_interrupt_disable(irq);
	//ddekit_thread_terminate(ddekit_irq_ctrl[irq].irq_thread);
	pthread_t _thread = (pthread_t)ddekit_thread_get_id(ddekit_irq_ctrl[irq].irq_thread);
	l4_cap_idx_t thread_cap = pthread_l4_cap(_thread);
	l4_msgtag_t res = l4_ipc_send(thread_cap, l4_utcb(),
	    			      l4_msgtag(0, 0, 0, 0), L4_IPC_NEVER);
	if (l4_ipc_error(res, l4_utcb()))
		ddekit_panic("cannot stop irq thread\n");
	
	// wait for thread to detach from irq
	ddekit_sem_down(ddekit_irq_ctrl[irq].stopsem);
	
	if (ddekit_thread_terminate(ddekit_irq_ctrl[irq].irq_thread))
	    ddekit_panic("cannot shutdown irq thread\n");

	ddekit_simple_free(ddekit_irq_ctrl[irq].params);
	
	ddekit_irq_ctrl[irq].handle_irq = 0;
	ddekit_irq_ctrl[irq].irq_thread = 0;
	ddekit_sem_deinit(ddekit_irq_ctrl[irq].irqsem);
	ddekit_sem_deinit(ddekit_irq_ctrl[irq].stopsem);
}


void ddekit_interrupt_disable(int irq)
{
	if (!l4_is_invalid_cap(ddekit_irq_ctrl[irq].irq_cap)) {
		ddekit_sem_down(ddekit_irq_ctrl[irq].irqsem);
		--ddekit_irq_ctrl[irq].handle_irq;
		ddekit_sem_up(ddekit_irq_ctrl[irq].irqsem);
	}
}


void ddekit_interrupt_enable(int irq)
{
	if (!l4_is_invalid_cap(ddekit_irq_ctrl[irq].irq_cap)) {
		ddekit_sem_down(ddekit_irq_ctrl[irq].irqsem);
		++ddekit_irq_ctrl[irq].handle_irq;
		ddekit_sem_up(ddekit_irq_ctrl[irq].irqsem);
	}
}

int ddekit_irq_set_type(int irq, unsigned type)
{
	if (irq < MAX_INTERRUPTS) {
		ddekit_printf("IRQ: set irq type of %u to %x\n", irq, type);
		switch (type & IRQF_TRIGGER_MASK) {
			case IRQF_TRIGGER_RISING:
			  ddekit_irq_ctrl[irq].trigger = L4_IRQ_F_POS_EDGE;
			  break;
			case IRQF_TRIGGER_FALLING:
			  ddekit_irq_ctrl[irq].trigger = L4_IRQ_F_NEG_EDGE;
			  break;
			case IRQF_TRIGGER_HIGH:
			  ddekit_irq_ctrl[irq].trigger = L4_IRQ_F_LEVEL_HIGH;
			  break;
			case IRQF_TRIGGER_LOW:
			  ddekit_irq_ctrl[irq].trigger = L4_IRQ_F_LEVEL_LOW;
			  break;
			default: ddekit_irq_ctrl[irq].trigger = 0; break;
		}

		return 0;
	}

	return -1;
}

void ddekit_init_irqs(void)
{
	int i;
	for (i = 0; i < MAX_INTERRUPTS; i++) {
		ddekit_irq_ctrl[i].irq_cap = L4_INVALID_CAP;
		ddekit_irq_ctrl[i].trigger = 0;
	}
}

