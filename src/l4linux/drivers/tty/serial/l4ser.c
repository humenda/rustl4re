/*
 *  drivers/tty/serial/l4ser.c
 *
 *  L4 pseudo serial driver.
 *
 *  Based on sa1100.c and other drivers from drivers/tty/serial/.
 */
#if defined(CONFIG_L4_SERIAL_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
#define SUPPORT_SYSRQ
#endif

#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/console.h>
#include <linux/sysrq.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial_core.h>
#include <linux/serial.h>
#include <linux/of_platform.h>

#include <l4/re/env.h>
#include <l4/sys/vcon.h>
#include <l4/sys/factory.h>
#include <l4/sys/icu.h>
#include <l4/sys/task.h>
#include <asm/generic/setup.h>
#include <asm/generic/cap_alloc.h>
#include <asm/generic/util.h>
#include <asm/generic/vcpu.h>

/* This is the same major as the sa1100 one */
#define SERIAL_L4SER_MAJOR	204
#define MINOR_START		5
#define UART_NR			8

struct l4ser_uart_port {
	struct uart_port port;
	l4_cap_idx_t vcon_cap;
	l4_cap_idx_t vcon_irq_cap;
	struct platform_device *pdev;
	const char *capname;
	char inited;
};

static char capnames[UART_NR][20];
static int nr_capnames;
static struct l4ser_uart_port l4ser_port[UART_NR];

static void l4ser_stop_tx(struct uart_port *port)
{
}

static void l4ser_stop_rx(struct uart_port *port)
{
}

static void l4ser_enable_ms(struct uart_port *port)
{
}

static void
l4ser_process_buf(struct uart_port *port, char *buf, int len)
{
	for (; len--; ++buf) {
		port->icount.rx++;

		if (uart_handle_sysrq_char(port, *buf))
			continue;

		tty_insert_flip_char(&port->state->port, *buf, TTY_NORMAL);
	}
}

static void
l4ser_rx_chars(struct uart_port *port)
{
	struct l4ser_uart_port *l4port = (struct l4ser_uart_port *)port;
	char chbuf[16];
	int r;

	BUG_ON(l4x_is_vcpu() && !irqs_disabled());
	if (l4_is_invalid_cap(l4port->vcon_irq_cap))
		return;

	do {
		r = l4_vcon_read_with_flags(l4port->vcon_cap,
		                            chbuf, sizeof(chbuf));
		if (r < 0)
			break;

#if defined(SUPPORT_SYSRQ)
		if (r & L4_VCON_READ_STAT_BREAK) {
			port->icount.brk++;
			WARN_ON(!uart_handle_break(port));
		}
#endif

		r &= L4_VCON_READ_SIZE_MASK;
		l4ser_process_buf(port, chbuf, min((int)sizeof(chbuf), r));
	} while (r > sizeof(chbuf));

	tty_flip_buffer_push(&port->state->port);
	return;
}

static void l4ser_start_tx(struct uart_port *port)
{
	struct l4ser_uart_port *l4port = (struct l4ser_uart_port *)port;
	struct circ_buf *xmit = &port->state->xmit;
	int c;

	if (port->x_char) {
		L4XV_FN_v(l4_vcon_write(l4port->vcon_cap, &port->x_char, 1));
		port->icount.tx++;
		port->x_char = 0;
		return;
	}

	while (!uart_circ_empty(xmit)) {
		c = CIRC_CNT_TO_END(xmit->head, xmit->tail, UART_XMIT_SIZE);
		if (c > L4_VCON_WRITE_SIZE)
			c = L4_VCON_WRITE_SIZE;
		L4XV_FN_v(l4_vcon_write(l4port->vcon_cap, &xmit->buf[xmit->tail], c));
		xmit->tail = (xmit->tail + c) & (UART_XMIT_SIZE - 1);
		port->icount.tx += c;
	}
}

static irqreturn_t l4ser_int(int irq, void *dev_id)
{
	struct uart_port *sport = dev_id;

	l4ser_rx_chars(sport);

	return IRQ_HANDLED;
}

static unsigned int l4ser_tx_empty(struct uart_port *port)
{
	return TIOCSER_TEMT;
}

static unsigned int l4ser_get_mctrl(struct uart_port *port)
{
	return 0;
}

static void l4ser_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
}

static void l4ser_break_ctl(struct uart_port *port, int break_state)
{
}

static int l4ser_startup(struct uart_port *port)
{
	int retval;

	if (port->irq) {
		retval = request_irq(port->irq, l4ser_int,
		                     IRQF_TRIGGER_RISING, "L4-uart", port);
		if (retval)
			return retval;

		L4XV_FN_v(l4ser_rx_chars(port));
	}

	return 0;
}

static void l4ser_shutdown(struct uart_port *port)
{
	if (port->irq)
		free_irq(port->irq, port);
}

static void l4ser_set_termios(struct uart_port *port, struct ktermios *termios,
                              struct ktermios *old)
{
}

static const char *l4ser_type(struct uart_port *port)
{
	return port->type == PORT_SA1100 ? "L4-vcon" : NULL;
}


static int l4ser_request_port(struct uart_port *port)
{
	return 0;
}

static void l4ser_release_port(struct uart_port *port)
{
}

static void l4ser_config_port(struct uart_port *port, int flags)
{
	if (flags & UART_CONFIG_TYPE)
		port->type = PORT_SA1100;
}

static int
l4ser_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	return 0;
}

#ifdef CONFIG_CONSOLE_POLL
static int l4ser_poll_get_char(struct uart_port *port)
{
	struct l4ser_uart_port *l4port = (struct l4ser_uart_port *)port;
	char c;
	int r = L4XV_FN_i(l4_vcon_read(l4port->vcon_cap, &c, 1));
	if (r < 1)
		return NO_POLL_CHAR;

	return c;
}

static void l4ser_poll_put_char(struct uart_port *port, unsigned char ch)
{
	struct l4ser_uart_port *l4port = (struct l4ser_uart_port *)port;

	if (l4_is_invalid_cap(l4port->vcon_cap))
		return;

	L4XV_FN_v(l4_vcon_write(l4port->vcon_cap, &ch, 1));
}
#endif

static const struct uart_ops l4ser_pops = {
	.tx_empty	= l4ser_tx_empty,
	.set_mctrl	= l4ser_set_mctrl,
	.get_mctrl	= l4ser_get_mctrl,
	.stop_tx	= l4ser_stop_tx,
	.start_tx	= l4ser_start_tx,
	.stop_rx	= l4ser_stop_rx,
	.enable_ms	= l4ser_enable_ms,
	.break_ctl	= l4ser_break_ctl,
	.startup	= l4ser_startup,
	.shutdown	= l4ser_shutdown,
	.set_termios	= l4ser_set_termios,
	.type		= l4ser_type,
	.release_port	= l4ser_release_port,
	.request_port	= l4ser_request_port,
	.config_port	= l4ser_config_port,
	.verify_port	= l4ser_verify_port,
#ifdef CONFIG_CONSOLE_POLL
	.poll_get_char  = l4ser_poll_get_char,
	.poll_put_char  = l4ser_poll_put_char,
#endif
};

static int l4ser_init_port(struct l4ser_uart_port *lup, unsigned id)
{
	int irq, r;
	l4_msgtag_t t;
	l4_vcon_attr_t vcon_attr;

	if (lup->inited)
		return 0;
	lup->inited = 1;

	if (id == 0)
		lup->vcon_cap = l4re_env()->log;
	else if ((r = l4x_re_resolve_name(l4ser_port[id].capname,
	                                  &lup->vcon_cap)))
		return r;

	lup->vcon_irq_cap = l4x_cap_alloc();
	if (l4_is_invalid_cap(lup->vcon_irq_cap))
		return -ENOMEM;

	t = L4XV_FN(l4_msgtag_t,
	            l4_factory_create_irq(l4re_env()->factory,
	                                  lup->vcon_irq_cap));
	if (l4_error(t)) {
		l4x_cap_free(lup->vcon_irq_cap);
		return -ENOMEM;
	}

	t = L4XV_FN(l4_msgtag_t,
	            l4_icu_bind(lup->vcon_cap, 0,
	                        lup->vcon_irq_cap));
	if ((l4_error(t))) {
		L4XV_FN_v(l4_task_delete_obj(L4RE_THIS_TASK_CAP,
		                             lup->vcon_irq_cap));
		l4x_cap_free(lup->vcon_irq_cap);

		// No interrupt, just output
		lup->vcon_irq_cap = L4_INVALID_CAP;
		irq = 0;
	} else if ((irq = l4x_register_irq(lup->vcon_irq_cap)) < 0) {
		L4XV_FN_v(l4_task_delete_obj(L4RE_THIS_TASK_CAP,
		                             lup->vcon_irq_cap));
		l4x_cap_free(lup->vcon_irq_cap);
		return -EIO;
	}

	vcon_attr.i_flags = 0;
	vcon_attr.o_flags = 0;
	vcon_attr.l_flags = 0;
	L4XV_FN_v(l4_vcon_set_attr(lup->vcon_cap, &vcon_attr));

	lup->port.uartclk   = 3686400;
	lup->port.ops       = &l4ser_pops;
	lup->port.fifosize  = 8;
	lup->port.line      = id;
	lup->port.iotype    = UPIO_MEM;
	lup->port.membase   = (void *)1;
	lup->port.mapbase   = 1;
	lup->port.flags     = UPF_BOOT_AUTOCONF;
	lup->port.irq       = irq;

	return 0;
}

#ifdef CONFIG_L4_SERIAL_CONSOLE

static int __init
l4ser_console_setup(struct console *co, char *options)
{
	struct uart_port *up;

	if (co->index >= UART_NR)
		co->index = 0;
	up = &l4ser_port[co->index].port;
	if (!up)
		return -ENODEV;

	return uart_set_options(up, co, 115200, 'n', 8, 'n');
}

/*
 * Interrupts are disabled on entering
 */
static void
l4ser_console_write(struct console *co, const char *s, unsigned int count)
{
	l4_cap_idx_t cap = l4ser_port[co->index].vcon_cap;
	do {
		unsigned i;
		unsigned c = count;
		if (c > L4_VCON_WRITE_SIZE)
			c = L4_VCON_WRITE_SIZE;

		for (i = 0; i < c;) {
			if (s[i] == '\n')  {
				L4XV_FN_v(l4_vcon_write(cap, s, i));
				L4XV_FN_v(l4_vcon_write(cap, "\r\n", 2));
				s += i + 1;
				count -= i + 1;
				c -= i + 1;
				i = 0;
			} else {
				++i;
			}
		}
		L4XV_FN_v(l4_vcon_write(cap, s, c));
		s += c;
		count -= c;
	} while (count);
}


static struct uart_driver l4ser_driver;
static struct console l4ser_console = {
	.name		= "ttyLv",
	.write		= l4ser_console_write,
	.device		= uart_console_device,
	.setup		= l4ser_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
	.data		= &l4ser_driver,
};

static int __init l4ser_rs_console_init(void)
{
	if (l4ser_init_port(&l4ser_port[0], 0))
		return -ENODEV;
	register_console(&l4ser_console);
	return 0;
}
console_initcall(l4ser_rs_console_init);

#define L4SER_CONSOLE	&l4ser_console
#else
#define L4SER_CONSOLE	NULL
#endif

static struct uart_driver l4ser_driver = {
	.owner			= THIS_MODULE,
	.driver_name		= "ttyLv",
	.dev_name		= "ttyLv",
	.major			= SERIAL_L4SER_MAJOR,
	.minor			= MINOR_START,
	.nr			= UART_NR,
	.cons			= L4SER_CONSOLE,
};

#ifdef CONFIG_PM
static int l4ser_suspend(struct device *dev)
{
	struct l4ser_uart_port *lup = dev_get_drvdata(dev);
	if (device_may_wakeup(dev)) {
		dev_info(dev, "enable IRQ wake\n");
		enable_irq_wake(lup->port.irq);
		disable_irq(lup->port.irq);
	}
	return 0;
}

static int l4ser_resume(struct device *dev)
{
	struct l4ser_uart_port *lup = dev_get_drvdata(dev);
	if (device_may_wakeup(dev)) {
		dev_info(dev, "disable IRQ wake\n");
		enable_irq(lup->port.irq);
		disable_irq_wake(lup->port.irq);
	}
	return 0;
}
#endif

static int get_next_free(void)
{
	int id;

	for (id = 0; id < UART_NR; ++id)
		if (!l4ser_port[id].inited)
			break;

	if (id >= UART_NR) {
		pr_err("l4ser: Out of internal storage\n");
		return -ENOMEM;
	}

	return id;
}

static int l4ser_probe(struct platform_device *pdev)
{
	struct l4ser_uart_port *lup = NULL;
	struct device_node *np = pdev->dev.of_node;
	const char *of_capname = NULL;
	int id = -1;

	if (np) {
		int len;
		const char *n;

		id = of_alias_get_id(np, "serial");
		if (id < 0) {
			const __be32 *idv;
			idv = of_get_property(np, "id", NULL);
			if (!idv) {
				dev_err(&pdev->dev,
				        "Missing 'id' attribute.\n");
				return -EINVAL;
			}
			id = be32_to_cpu(*idv);
		}

		if (id == 0) {
			dev_err(&pdev->dev,
			        "id=0 must not be defined in DT.\n");
			return -EINVAL;
		}

		if (id > 0 && id < UART_NR && l4ser_port[id].inited)
			dev_warn(&pdev->dev, "Duplicate id=%d.\n", id);

		n = of_get_property(np, "cap", &len);
		if (!n) {
			dev_err(&pdev->dev, "Missing 'cap' attribute.\n");
			return -EINVAL;
		}
		of_capname = n;
	}


	if (id < 0)
		id = pdev->id;

	if (id < 0) {
		id = get_next_free();
		if (id < 0)
			return id;
	}

	if (id < 0 || id >= UART_NR)
		return -ENXIO;

	lup = &l4ser_port[id];

	if (of_capname)
		lup->capname = of_capname;

	if (l4ser_init_port(lup, id))
		return -ENODEV;

	lup->port.dev = &pdev->dev;
	platform_set_drvdata(pdev, lup);
	uart_add_one_port(&l4ser_driver, &lup->port);

	if (lup->port.irq)
		device_set_wakeup_capable(lup->port.dev, true);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id l4ser_id_table[] = {
	{ .compatible = "l4linux,l4ser" },
};
MODULE_DEVICE_TABLE(of, l4ser_id_table);
#endif

static int l4ser_remove(struct platform_device *pdev)
{
	struct l4ser_uart_port *lup = platform_get_drvdata(pdev);
	if (lup->port.irq) {
		disable_irq(lup->port.irq);
		device_set_wakeup_capable(&pdev->dev, false);
	}
	uart_remove_one_port(&l4ser_driver, &lup->port);
	return 0;
}

static SIMPLE_DEV_PM_OPS(l4ser_pm_ops, l4ser_suspend, l4ser_resume);
static struct platform_driver l4ser_platform_driver = {
	.probe		= l4ser_probe,
	.remove		= l4ser_remove,
	.driver = {
		.name           = "serial-ttyLv",
		.owner          = THIS_MODULE,
		.pm             = &l4ser_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = l4ser_id_table,
#endif
	},
};

static int __init l4ser_add_one_dev(int id)
{
	int ret;

	l4ser_port[id].pdev = platform_device_alloc("serial-ttyLv", id);
	if (!l4ser_port[id].pdev)
		return -ENOMEM;

	ret = platform_device_add(l4ser_port[id].pdev);
	if (ret) {
		platform_device_put(l4ser_port[id].pdev);
		l4ser_port[id].pdev = NULL;
		return ret;
	}

	return 0;
}

static int __init l4ser_serial_init(void)
{
	int ret, i;
	l4_cap_idx_t tmpcap;

	pr_info("L4 serial driver\n");
	ret = uart_register_driver(&l4ser_driver);
	if (ret) {
		pr_err("l4ser: could not register uart driver: %d\n", ret);
		return ret;
	}

	ret = platform_driver_register(&l4ser_platform_driver);
	if (ret) {
		uart_unregister_driver(&l4ser_driver);
		pr_err("l4ser: could not register platform driver: %d\n", ret);
		return ret;
	}

	/* A warning/hint, remove some time later */
	if (!l4x_re_resolve_name("log", &tmpcap))
		pr_err("l4ser: INFO: Do not use "
		       "\"caps = { log = ... },\" anymore.\n"
		       "l4ser: INFO: The 'log' capability "
		       "is not evaluated anymore.\n"
		       "l4ser: INFO: Pull the 'log' statement out "
		       "of the 'caps' table.\n");

	ret = l4ser_add_one_dev(0);
	if (ret) {
		platform_driver_unregister(&l4ser_platform_driver);
		uart_unregister_driver(&l4ser_driver);
		return ret;
	}

	for (i = 0; i < nr_capnames; ++i) {
		int id;

		if (capnames[i][0] == 0)
			break;

		id = get_next_free();
		if (id < 0)
			return id;

		l4ser_port[id].capname = capnames[i];
		ret = l4ser_add_one_dev(id);
		if (ret)
			return ret;
	}

	return 0;
}

static void __exit l4ser_serial_exit(void)
{
	int id;

#ifdef CONFIG_L4_SERIAL_CONSOLE
	unregister_console(&l4ser_console);
#endif
	for (id = 0; id < UART_NR; ++id)
		if (l4ser_port[id].pdev)
			platform_device_unregister(l4ser_port[id].pdev);

	platform_driver_unregister(&l4ser_platform_driver);
	uart_unregister_driver(&l4ser_driver);
}

module_init(l4ser_serial_init);
module_exit(l4ser_serial_exit);

/* This function is called much earlier than module_init, esp. there's
 * no kmalloc available */
static int l4ser_setup(const char *val, const struct kernel_param *kp)
{
	if (nr_capnames > UART_NR) {
		pr_err("l4ser: Too many additional ports specified\n");
		return 1;
	}
	strlcpy(capnames[nr_capnames], val, sizeof(capnames[0]));
	nr_capnames++;
	return 0;
}

module_param_call(add, l4ser_setup, NULL, NULL, 0200);
MODULE_PARM_DESC(add, "Use l4ser.add=name to add another port, name queried in cap environment");

MODULE_AUTHOR("Adam Lackorzynski <adam@l4re.org>");
MODULE_DESCRIPTION("L4 serial driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS_CHARDEV_MAJOR(SERIAL_L4SER_MAJOR);
