/*****************************************************************************/
/**
 * \file   input/lib/src/emul_irq.c
 * \brief  L4INPUT: Linux IRQ handling emulation
 *
 * \date   11/20/2003
 * \author Christian Helmuth <ch12@os.inf.tu-dresden.de>
 * \author Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *
 */
/*
 * (c) 2003-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <stdio.h>
#include <string.h>
#include <l4/sys/ipc.h>
#include <l4/sys/types.h>
//#include <l4/util/irq.h>
#include <l4/irq/irq.h>
#if defined(ARCH_x86) || defined(ARCH_amd64)
#include <l4/util/port_io.h>
#endif

#include <pthread.h>

/* Linux */
#include <linux/interrupt.h>

#include "internal.h"

#define DEBUG_IRQ         0
#define DEBUG_IRQ_VERBOSE 0

/* return within 50ms even if there was no interrupt */
#define IRQ_TIMEOUT	  l4_ipc_timeout(0,0,781,6)

/* INTERRUPT HANDLING EMULATION */

static struct irq_desc {
	int active;
	int num;
	void *cookie;
	pthread_t t;
        pthread_mutex_t startup;
	irqreturn_t (*handler)(int, void *, struct pt_regs *);
} handlers[NR_IRQS];

static int irq_prio   = -1;

/* OMEGA0 STUFF */

#define OM_MASK    0x00000001
#define OM_UNMASK  0x00000002
#define OM_CONSUME 0x00000004
#define OM_AGAIN   0x00000008

/** Attach IRQ line.
 *
 * \param  irq           IRQ number
 * \retval handle        IRQ handle
 *
 * \return 0 on success (attached interrupt), -1 if attach failed.
 */
static inline int __omega0_attach(unsigned int irq, l4irq_t **handle)
{
  return (*handle = l4irq_attach(irq)) ? 0 : -1;
}

/** Wait for IRQ notification.
 *
 * \param  irq           IRQ number
 * \param  handle        IRQ line handle
 * \param  flags         Flags:
 *                       - \c OM_MASK    request IRQ mask
 *                       - \c OM_UNMASK  request IRQ unmask
 *                       - \c OM_CONSUME IRQ consumed
 * \return               0 for success
 *                       L4_IPC_RETIMEOUT if no IRQ was received (timeout)
 */
static inline int __omega0_wait(unsigned int irq, l4irq_t *handle,
                                unsigned int flags)
{
  return l4irq_wait(handle);
}

/** IRQ HANDLER THREAD.
 *
 * \param irq_desc  IRQ handling descriptor (IRQ number and handler routine)
 */
static void *__irq_handler(void *_data)
{
        struct irq_desc *irq_desc = (struct irq_desc *)_data;
	unsigned int irq = irq_desc->num; /* save irq number */
	void *cookie = irq_desc->cookie;  /* save cookie/dev_id pointer */
	long ret;
	l4irq_t *irq_handle;
	unsigned int om_flags;

	/* get permission for attaching to IRQ */
	ret = __omega0_attach(irq, &irq_handle);
	if (ret < 0)
	  {
	    fprintf(stderr, "failed to attach IRQ %d!\n", irq);
            return NULL;
	  }

	/* we are up */
        ret = pthread_mutex_unlock(&handlers[irq].startup);

#if DEBUG_IRQ
	printf("emul_irq.c: __irq_handler %p for IRQ %d running\n",
               irq_desc->handler, irq);
#endif

	if (ret)
          {
	    fprintf(stderr, "IRQ thread startup failed!\n");
            return NULL;
          }

	om_flags = OM_UNMASK;
	for (;;) {
		ret = __omega0_wait(irq, irq_handle, om_flags);

		switch (ret) {
		case 0:
#if DEBUG_IRQ_VERBOSE
			printf("emul_irq.c: got IRQ %d\n", irq);
#endif
			if (irq_desc->active)
				irq_desc->handler(irq, cookie, NULL);
			om_flags = 0;
			break;

#if 0
		case L4_IPC_RETIMEOUT:
			if (irq_desc->active)
				irq_desc->handler(irq, cookie, NULL);
			om_flags = OM_AGAIN;
			break;
#endif

		default:
			fprintf(stderr, "error receiving irq\n");
                        return NULL;
		}
	}
}

/** Request Interrupt.
 * \ingroup grp_irq
 *
 * \param irq      interrupt number
 * \param handler  interrupt handler -> top half
 * \param ...
 * \param cookie   cookie pointer passed back
 *
 * \return 0 on success; error code otherwise
 */
int request_irq(unsigned int irq,
                irqreturn_t (*handler)(int, void *, struct pt_regs *),
                unsigned long flags, const char *devname, void *cookie)
{
	pthread_t irq_tid;
	char buf[7];

	if (irq >= NR_IRQS)
		return -L4_EINVAL;
	if (!handler)
		return -L4_EINVAL;

	handlers[irq].num = irq;
	handlers[irq].cookie = cookie;
	handlers[irq].handler = handler;

	handlers[irq].active = 1;

	if(handlers[irq].t) {
#if DEBUG_IRQ
		printf("emul_irq.c: reattached to irq %d\n", irq);
#endif
		return 0;
	}

	sprintf(buf, ".irq%.2X", irq);
	/* create IRQ handler thread */
        pthread_mutex_init(&handlers[irq].startup, NULL);
        pthread_mutex_lock(&handlers[irq].startup);

        pthread_attr_t a;
        pthread_attr_init(&a);
        if (irq_prio != -1) {
            struct sched_param sp;
            sp.sched_priority = irq_prio;
            pthread_attr_setschedpolicy(&a, SCHED_L4);
            pthread_attr_setschedparam(&a, &sp);
            pthread_attr_setinheritsched(&a, PTHREAD_EXPLICIT_SCHED);
        }
        if (pthread_create(&irq_tid, &a, __irq_handler, (void *)
                           &handlers[irq]))
          {
            printf("emul_irq.c: Creating irq-thread failed\n");
            pthread_mutex_unlock(&handlers[irq].startup);
            return 0;
          }

        pthread_mutex_lock(&handlers[irq].startup);
        pthread_mutex_unlock(&handlers[irq].startup);

#if DEBUG_IRQ
	printf("emul_irq.c: attached to irq %d\n", irq);
#endif

	handlers[irq].t = irq_tid;

	return 0;
}

/** Release Interrupt.
 *
 * \param irq     interrupt number
 * \param cookie  cookie pointer passed back
 *
 * \todo Implementation.
 */
void free_irq(unsigned int irq, void *cookie)
{
	handlers[irq].active = 0;

#if DEBUG_IRQ
	printf("emul_irq.c: attempt to free irq %d\n", irq);
#endif
}

/* INTERRUPT EMULATION INITIALIZATION */
void l4input_internal_irq_init(int prio)
{
	irq_prio   = prio;

	memset(&handlers, 0, sizeof(handlers));
}
