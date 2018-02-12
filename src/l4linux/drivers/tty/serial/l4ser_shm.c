/*
 *  drivers/char/l4shm.c
 *
 *  L4 pseudo serial driver backed with shared memory.
 *
 *  Based on sa1100.c and other drivers from drivers/serial/.
 */

#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/console.h>
#include <linux/device.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial_core.h>
#include <linux/serial.h>

#include <asm/generic/setup.h>
#include <asm/generic/cap_alloc.h>
#include <asm/generic/vcpu.h>

#include <l4/re/c/namespace.h>
#include <l4/shmc/shmc.h>

/* This is the same major as the sa1100 one */
#define SERIAL_L4SER_SHM_MAJOR	204
#define MINOR_START		15

enum {
	WAIT_TIMEOUT = 20000,
	NR_OF_PORTS = 3,
};

static int shmsize = 32 << 10;

struct l4ser_shm_uart_port {
	struct uart_port port;
	l4shmc_area_t   shmcarea;
	l4shmc_signal_t tx_sig;
	l4shmc_signal_t rx_sig;
	l4shmc_chunk_t  tx_chunk;
	l4shmc_chunk_t  rx_chunk;
	char           *tx_ring_start;
	char           *rx_ring_start;
	unsigned long   tx_ring_size;
	unsigned long   rx_ring_size;

	char name[20];
	int inited, create;
};

struct chunk_head {
	unsigned long next_offs_to_write; // end of ring content
	unsigned long next_offs_to_read;  // start of ring content
	unsigned long writer_blocked;     // ring buffer full
};

struct ring_chunk_head {
	unsigned long size; // 0 == not used,  >= 0 valid chunk
};


static int ports_to_add_pos;
static struct l4ser_shm_uart_port l4ser_shm_port[NR_OF_PORTS];

static inline int chunk_size(l4shmc_area_t *s)
{
	return (l4shmc_area_size(s) / 2) - 100;
}


static void l4ser_shm_stop_tx(struct uart_port *port)
{
}

static void l4ser_shm_stop_rx(struct uart_port *port)
{
}

static void l4ser_shm_enable_ms(struct uart_port *port)
{
}

static void
l4ser_shm_rx_chars(struct uart_port *port)
{
	struct l4ser_shm_uart_port *l4port = (struct l4ser_shm_uart_port *)port;
	struct tty_port *ttyport = &port->state->port;

	struct chunk_head *chhead;
	struct ring_chunk_head *rph;
	unsigned long offs;

	chhead = (struct chunk_head *)l4shmc_chunk_ptr(&l4port->rx_chunk);
	offs = chhead->next_offs_to_read;

	while (1) {
		unsigned long l;

		rph = (struct ring_chunk_head *)(l4port->rx_ring_start + offs);
		if (!rph->size)
			break;
		offs += sizeof(struct ring_chunk_head);
		offs %= l4port->rx_ring_size;

		if (offs + rph->size > l4port->rx_ring_size)
			l = l4port->rx_ring_size - offs;
		else
			l = rph->size;


		port->icount.rx += rph->size;

		tty_insert_flip_string(ttyport,
		                       (const unsigned char *)l4port->rx_ring_start + offs,
		                       l);
		if (l != rph->size)
			tty_insert_flip_string(ttyport,
			                       (const unsigned char *)l4port->rx_ring_start,
			                       rph->size - l);


		offs = (offs + rph->size + sizeof(struct ring_chunk_head) - 1)
		       & ~(sizeof(struct ring_chunk_head) - 1);
		offs %= l4port->rx_ring_size;
		chhead->next_offs_to_read = offs;
		rph->size = 0;

	}
	tty_flip_buffer_push(ttyport);

	if (chhead->writer_blocked)
		L4XV_FN_v(l4shmc_trigger(&l4port->tx_sig));

	chhead = (struct chunk_head *)l4shmc_chunk_ptr(&l4port->tx_chunk);
	chhead->writer_blocked = 0;

	return;
}

static DEFINE_SPINLOCK(txlock);
static DEFINE_SPINLOCK(rxlock);

static unsigned long tx_buf(struct uart_port *port, const char *chunk, int length)
{
	struct l4ser_shm_uart_port *l4port = (struct l4ser_shm_uart_port *)port;
	struct chunk_head *chhead;
	struct ring_chunk_head *rph;
	unsigned long l, offs, nextoffs, free_bytes;
	unsigned long flags;
	struct tty_struct *tty = port->state->port.tty;

	spin_lock_irqsave(&txlock, flags);

	chhead = (struct chunk_head *)l4shmc_chunk_ptr(&l4port->tx_chunk);
	offs = chhead->next_offs_to_write;

	rph = (struct ring_chunk_head *)(l4port->tx_ring_start + offs);
	BUILD_BUG_ON(sizeof(struct ring_chunk_head) & (sizeof(struct ring_chunk_head) - 1));

	// get max free space
	if (chhead->next_offs_to_write >= chhead->next_offs_to_read)
		free_bytes = l4port->tx_ring_size - (chhead->next_offs_to_write - chhead->next_offs_to_read);
	else
		free_bytes = chhead->next_offs_to_read - chhead->next_offs_to_write;

	if (free_bytes <= sizeof(struct ring_chunk_head) * 2) {
		chhead->writer_blocked = 1;
		spin_unlock_irqrestore(&txlock, flags);
		tty->hw_stopped = 1;
		tty->stopped = 1;
		l4shmc_trigger(&l4port->tx_sig);
		return 0;
	}

	if (length > free_bytes - sizeof(struct ring_chunk_head) * 2)
		length = free_bytes - sizeof(struct ring_chunk_head) * 2;

	nextoffs = (offs + length + sizeof(struct ring_chunk_head) + sizeof(struct ring_chunk_head) - 1)
	           & ~(sizeof(struct ring_chunk_head) - 1);

	nextoffs %= l4port->tx_ring_size;

	offs += sizeof(struct ring_chunk_head);
	offs %= l4port->tx_ring_size;

	if (offs + length > l4port->tx_ring_size)
		l = l4port->tx_ring_size - offs;
	else
		l = length;

	memcpy(l4port->tx_ring_start + offs, chunk, l);
	if (l != length)
		memcpy(l4port->tx_ring_start, chunk + l, length - l);

	// now write header
	((struct ring_chunk_head *)(l4port->tx_ring_start + nextoffs))->size = 0;
	smp_wmb();
	rph->size = length;
	chhead->next_offs_to_write = nextoffs;

	spin_unlock_irqrestore(&txlock, flags);
	return length;
}

static void l4ser_shm_tx_chars(struct uart_port *port)
{
	struct l4ser_shm_uart_port *l4port = (struct l4ser_shm_uart_port *)port;
	struct circ_buf *xmit = &port->state->xmit;
	int c, do_trigger = 0;

	struct tty_struct *tty = port->state->port.tty;
	tty->hw_stopped = 0;
	tty->stopped = 0;

	if (port->x_char) {
		if (tx_buf(port, &port->x_char, 1)) {
			port->icount.tx++;
			port->x_char = 0;
			L4XV_FN_v(l4shmc_trigger(&l4port->tx_sig));
		}
		return;
	}

	if (uart_circ_empty(xmit) || uart_tx_stopped(port)) {
		return;
	}

	while (!uart_circ_empty(xmit)) {
		unsigned long r;
		c = CIRC_CNT_TO_END(xmit->head, xmit->tail, UART_XMIT_SIZE);
		if (!(r = tx_buf(port, &xmit->buf[xmit->tail], c)))
			break;
		xmit->tail = (xmit->tail + r) & (UART_XMIT_SIZE - 1);
		port->icount.tx += r;
		do_trigger = 1;
	}

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(port);

	if (do_trigger)
		L4XV_FN_v(l4shmc_trigger(&l4port->tx_sig));
}

static void l4ser_shm_start_tx(struct uart_port *port)
{
	l4ser_shm_tx_chars(port);
}

static irqreturn_t l4ser_shm_int(int irq, void *dev_id)
{
	struct uart_port *sport = dev_id;
	unsigned long flags;

	spin_lock_irqsave(&rxlock, flags);
	l4ser_shm_rx_chars(sport);
	spin_unlock_irqrestore(&rxlock, flags);

	l4ser_shm_tx_chars(sport);

	return IRQ_HANDLED;
}

static unsigned int l4ser_shm_tx_empty(struct uart_port *port)
{
	return TIOCSER_TEMT;
}

static unsigned int l4ser_shm_get_mctrl(struct uart_port *port)
{
	return 0;
}

static void l4ser_shm_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
}

static void l4ser_shm_break_ctl(struct uart_port *port, int break_state)
{
}

static int l4ser_shm_startup(struct uart_port *port)
{
	int retval;
	unsigned long flags;

	if (port->irq) {
		retval = request_irq(port->irq, l4ser_shm_int,
		                     IRQF_TRIGGER_RISING, "L4-shm-uart", port);
		if (retval)
			return retval;

		spin_lock_irqsave(&rxlock, flags);
		l4ser_shm_rx_chars(port);
		spin_unlock_irqrestore(&rxlock, flags);
	}

	return 0;
}

static void l4ser_shm_shutdown(struct uart_port *port)
{
	if (port->irq)
		free_irq(port->irq, port);
}

static void l4ser_shm_set_termios(struct uart_port *port,
                                  struct ktermios *termios,
                                  struct ktermios *old)
{
}

static const char *l4ser_shm_type(struct uart_port *port)
{
	return port->type == PORT_SA1100 ? "L4" : NULL;
}


static int l4ser_shm_request_port(struct uart_port *port)
{
	return 0;
}

static void l4ser_shm_release_port(struct uart_port *port)
{
}

static void l4ser_shm_config_port(struct uart_port *port, int flags)
{
	if (flags & UART_CONFIG_TYPE)
		port->type = PORT_SA1100;
}

static int
l4ser_shm_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	return 0;
}

static const struct uart_ops l4ser_shm_pops = {
	.tx_empty	= l4ser_shm_tx_empty,
	.set_mctrl	= l4ser_shm_set_mctrl,
	.get_mctrl	= l4ser_shm_get_mctrl,
	.stop_tx	= l4ser_shm_stop_tx,
	.start_tx	= l4ser_shm_start_tx,
	.stop_rx	= l4ser_shm_stop_rx,
	.enable_ms	= l4ser_shm_enable_ms,
	.break_ctl	= l4ser_shm_break_ctl,
	.startup	= l4ser_shm_startup,
	.shutdown	= l4ser_shm_shutdown,
	.set_termios	= l4ser_shm_set_termios,
	.type		= l4ser_shm_type,
	.release_port	= l4ser_shm_release_port,
	.request_port	= l4ser_shm_request_port,
	.config_port	= l4ser_shm_config_port,
	.verify_port	= l4ser_shm_verify_port,
};

static int __init l4ser_shm_init_port(int num, const char *name)
{
	int irq;
	struct chunk_head *ch;
	struct l4ser_shm_uart_port *p = &l4ser_shm_port[num];
	L4XV_V(f);

	if (p->inited)
		return 0;
	p->inited = 1;

	if (shmsize < PAGE_SIZE)
		shmsize = PAGE_SIZE;

	pr_info("l4ser_shm: Requesting, role %s, Shmsize %d Kbytes\n",
	        p->create ? "Creator" : "User", shmsize >> 10);

	L4XV_L(f);
	if (p->create) {
		if (l4shmc_create(name, shmsize)) {
			L4XV_U(f);
			pr_err("l4ser_shm/%s: Failed to create shm\n",
			       p->name);
			return -ENOMEM;
		}
	}

	if (l4shmc_attach_to(name, WAIT_TIMEOUT,
	                     &p->shmcarea)) {
		L4XV_U(f);
		pr_err("l4ser_shm/%s: Failed to attach to shm\n", p->name);
		return -ENOMEM;
	}

	if (l4shmc_add_chunk(&p->shmcarea, p->create ? "joe" : "bob",
	                     chunk_size(&p->shmcarea),
	                     &p->tx_chunk))
		goto unlock;

	if (l4shmc_add_signal(&p->shmcarea, p->create ? "joe" : "bob",
	                      &p->tx_sig))
		goto unlock;

	if (l4shmc_connect_chunk_signal(&p->tx_chunk,
	                                &p->tx_sig))
		goto unlock;

	/* Now get the receiving side */
	if (l4shmc_get_chunk_to(&p->shmcarea, p->create ? "bob" : "joe",
	                        WAIT_TIMEOUT, &p->rx_chunk))
		goto unlock;

	if (l4shmc_get_signal_to(&p->shmcarea, p->create ? "bob" : "joe",
	                         WAIT_TIMEOUT, &p->rx_sig))
		goto unlock;

	if (l4shmc_connect_chunk_signal(&p->rx_chunk,
	                                &p->rx_sig))
		goto unlock;
	L4XV_U(f);

	if ((irq = l4x_register_irq(l4shmc_signal_cap(&p->rx_sig))) < 0)
		return -ENOMEM;

	ch = (struct chunk_head *)l4shmc_chunk_ptr(&p->tx_chunk);
	ch->next_offs_to_write = 0;
	ch->next_offs_to_read  = 0;
	ch->writer_blocked     = 0;

	p->tx_ring_size = l4shmc_chunk_capacity(&p->tx_chunk)
	                       - sizeof(struct chunk_head);
        p->rx_ring_size = l4shmc_chunk_capacity(&p->rx_chunk)
                               - sizeof(struct chunk_head);

        p->tx_ring_start = (char *)l4shmc_chunk_ptr(&p->tx_chunk)
                               + sizeof(struct chunk_head);
        p->rx_ring_start = (char *)l4shmc_chunk_ptr(&p->rx_chunk)
                               + sizeof(struct chunk_head);


	p->port.uartclk   = 3686400;
	p->port.ops       = &l4ser_shm_pops;
	p->port.fifosize  = 8;
	p->port.line      = num;
	p->port.iotype    = UPIO_MEM;
	p->port.membase   = (void *)1;
	p->port.mapbase   = 1;
	p->port.flags     = UPF_BOOT_AUTOCONF;
	p->port.irq       = irq;

	return 0;

unlock:
	L4XV_U(f);
	return -ENOMEM;
}

static struct uart_driver l4ser_shm_reg = {
	.owner			= THIS_MODULE,
	.driver_name		= "ttyLs",
	.dev_name		= "ttyLs",
	.major			= SERIAL_L4SER_SHM_MAJOR,
	.minor			= MINOR_START,
	.nr			= NR_OF_PORTS,
};

static int __init l4ser_shm_serial_init(void)
{
	int ret;
	int i;

	pr_info("l4ser_shm: L4 shared mem serial driver\n");

	ret = uart_register_driver(&l4ser_shm_reg);
	if (ret)
		return ret;

	ret = -ENODEV;
	for (i = 0; i < ports_to_add_pos; ++i) {
		if (!*l4ser_shm_port[i].name)
			continue;
		if (l4ser_shm_init_port(i, l4ser_shm_port[i].name)) {
			pr_warn("l4ser_shm: Failed to initialize additional port '%s'.\n",
			        l4ser_shm_port[i].name);
			continue;
		}
		pr_info("l4ser_shm: Adding '%s'\n", l4ser_shm_port[i].name);
		uart_add_one_port(&l4ser_shm_reg, &l4ser_shm_port[i].port);
		ret = 0;
	}

	return ret;
}

static void __exit l4ser_shm_serial_exit(void)
{
	int i;
	for (i = 0; i < ports_to_add_pos; ++i)
		uart_remove_one_port(&l4ser_shm_reg, &l4ser_shm_port[i].port);
	uart_unregister_driver(&l4ser_shm_reg);
}

module_init(l4ser_shm_serial_init);
module_exit(l4ser_shm_serial_exit);

/* This function is called much earlier than module_init, esp. there's
 * no kmalloc available */
static int l4ser_shm_setup(const char *val, const struct kernel_param *kp)
{
	int l;
	char *c;
	if (ports_to_add_pos >= NR_OF_PORTS) {
		pr_err("l4ser_shm: Too many ports specified, max %d\n",
		       NR_OF_PORTS);
		return 1;
	}
	l = strlen(val) + 1;
	if (l > sizeof(l4ser_shm_port[ports_to_add_pos].name))
		l = sizeof(l4ser_shm_port[ports_to_add_pos].name);
	c = strchr(val, ',');
	if (c) {
		l = c - val + 1;
		if (!strncmp(c + 1, "create", 6))
			l4ser_shm_port[ports_to_add_pos].create = 1;
	}
	strlcpy(l4ser_shm_port[ports_to_add_pos].name, val, l);
	ports_to_add_pos++;
	return 0;
}

module_param_call(add, l4ser_shm_setup, NULL, NULL, 0200);
MODULE_PARM_DESC(add, "Use l4ser_shm.add=name[,create] to add a device, name queried in namespace");

MODULE_AUTHOR("Adam Lackorzynski <adam@os.inf.tu-dresden.de");
MODULE_DESCRIPTION("L4 serial shm driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS_CHARDEV_MAJOR(SERIAL_L4SER_SHM_MAJOR);
