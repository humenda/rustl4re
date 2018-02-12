/*
 * \brief   DDE for Linux 2.6 test program
 * \author  Bjoern Doebel <doebel@os.inf.tu-dresden.de>
 * \author  Christian Helmuth <ch12@os.inf.tu-dresden.de>
 * \date    2007-01-22
 */

#include <asm/current.h>

#include <linux/kernel.h>
#include <linux/completion.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
//#include <linux/kthread.h>

#include <l4/dde/dde.h>
#include <l4/dde/ddekit/initcall.h>
#include <l4/dde/linux26/dde26.h>

#include <l4/util/parse_cmd.h>
#include <l4/util/util.h>
#include <l4/log/log.h>

/* We define 4 initcalls and see if these are executed
 * in the beginning.
 */
static __init void foo(void) { printk("foo  module_init\n"); }
static __init void bar(void) { printk("bar  device_initcall\n"); }
static __init void bla(void) { printk("bla  arch_initcall\n"); }
static __init void blub(void) { printk("blub subsys_initcall\n"); }
module_init(foo);
device_initcall(bar);
arch_initcall(bla);
subsys_initcall(blub);

/***********************************************************************
 ** Test 1: Check whether the current() macro works.                  **
 ***********************************************************************/
static void current_test(void)
{
	struct task_struct *t = NULL;

	printk("Current() test.\n");

	t = current;
	printk("\tt = %p\n", t);
}


/***********************************************************************
 ** Test 2: Getting complicated. Test startup of some kernel threads  **
 **         and wait for them to finish using completions.            **
 ***********************************************************************/
#define NUM_KTHREADS	5
static struct completion _kthread_completions_[NUM_KTHREADS];

static int kernel_thread_func(void *arg)
{
	printk("\t\tKernel thread %d\n", (int)arg);
	printk("\t\tcurrent = %p\n", current);

	/* do some work */
	msleep(200);

	complete_and_exit( &_kthread_completions_[(int)arg], 0 );
	return 0;
}


static void kernel_thread_test(void)
{
	int i;
	printk("Testing kernel_thread()\n");
	for (i=0; i < NUM_KTHREADS; i++) {
		int j;
		printk("\tInitializing completion for kernel thread.%x\n", i+1);
		init_completion(&_kthread_completions_[i]);
		printk("\tStarting kthread.%x\n", i+1);
		j = kernel_thread(kernel_thread_func, (void *)i, 0);
		printk("\treturn: %d\n", j);
	}

	for (i=0; i < NUM_KTHREADS; i++) {
		printk("\tWaiting for kthread.%x to complete.\n", i+1);
		wait_for_completion(&_kthread_completions_[i]);
		printk("\tkthread.%x has exited.\n", i+1);
	}
}


/******************************************************************************
 ** Test 3: Test kernel wait queues: start a thread incrementing wait_value, **
 **         and  sleep until wait_value is larger than 6 for the first time. **
 ******************************************************************************/
static DECLARE_WAIT_QUEUE_HEAD(_wq_head);
static int wait_value = 0;
static struct completion wq_completion;

static int inc_func(void *arg)
{
	int i = 0;

	printk("\033[33mI am counting up wait_value.\033[0m\n");
	for (i=0; i<10; i++)
	{
		printk("\033[33mwait_value: %d\033[0m\n", ++wait_value);
		wake_up(&_wq_head);
		msleep(500);
	}
	complete_and_exit(&wq_completion, 0);
}


static void wq_test(void)
{
	int pid;
	printk("\033[32mWait_queue test. I'm waiting vor wait_value to become >6.\033[0m\n");

	init_completion(&wq_completion);
	pid = kernel_thread(inc_func, 0, 0);

	wait_event(_wq_head, wait_value > 6);
	printk("\033[32;1mwait_value > 6 occured!\033[0m\n");

	wait_for_completion(&wq_completion);
	printk("\033[32mtest done.\033[0m\n");
}


/****************************************************************************
 ** Test 4: Tasklets                                                       **
 ****************************************************************************/
static void tasklet_func(unsigned long i)
{
	printk("TASKLET: %d\n", i);
}


static DECLARE_TASKLET(low0, tasklet_func, 0);
static DECLARE_TASKLET(low1, tasklet_func, 1);
static DECLARE_TASKLET(low2, tasklet_func, 2);
static DECLARE_TASKLET_DISABLED(low3, tasklet_func, 3);

static DECLARE_TASKLET(hi0, tasklet_func, 10);
static DECLARE_TASKLET(hi1, tasklet_func, 11);
static DECLARE_TASKLET_DISABLED(hi2, tasklet_func, 12);


static void tasklet_test(void)
{
	printk("BEGIN TASKLET TEST\n");

	l4dde26_softirq_init();

	printk("sleep 1000 msec\n");
	msleep(1000);

	printk("Scheduling tasklets 0-2 immediately. 3 is disabled for 2 seconds.\n");
	tasklet_schedule(&low0);

	tasklet_schedule(&low1);
	tasklet_schedule(&low2);
	tasklet_schedule(&low3);
	msleep(2000);
	tasklet_enable(&low3);

	msleep(1000);
	
#if 0
	printk("Scheduling hi_tasklets 10-12, and tasklets 0-2\n");
	tasklet_hi_schedule(&hi0);
	tasklet_hi_schedule(&hi1);
	tasklet_hi_schedule(&hi2);
	tasklet_schedule(&low0);
	tasklet_schedule(&low1);
	tasklet_schedule(&low2);
	tasklet_enable(&hi2);
#endif

	unsigned idx;
	for (idx=0; idx < 3; ++idx) {
	    printk("Scheduling (disabled) tasklet 3 twice - should only run once after enabling.\n");
	    tasklet_disable(&low3);
	    tasklet_schedule(&low3);
	    tasklet_schedule(&low3);
	    tasklet_enable(&low3);
	    msleep(2000);
	}


	printk("END TASKLET TEST\n");
}


/******************************************************************************
 ** Test 5: Timers                                                           **
 **                                                                          **
 ** Schedule a periodic timer printing "tick" every second. Additionally,    **
 ** schedule timers for 5, 10, 15, 20, and 25 seconds. Timer at 15s will     **
 ** deactivate the 20s timer.                                                **
 ******************************************************************************/

static struct timer_list _timer;
static struct timer_list _timer5;
static struct timer_list _timer10;
static struct timer_list _timer15;
static struct timer_list _timer20;
static struct timer_list _timer25;

static void tick_func(unsigned long d)
{
	printk("tick (%ld)\n", jiffies);
	_timer.expires = jiffies + HZ;
	add_timer(&_timer);
}


static void timer_func(unsigned long d)
{
	printk("timer_func: %lu\n", d);

	if (d == 15) {
		printk("De-scheduling 20s timer.\n");
		del_timer(&_timer20);
	}

	if (timer_pending(&_timer20))
		printk("timer for 20s still pending.\n");
	else
		printk("timer for 20s has been disabled.\n");
}


static void timer_test(void)
{
	l4dde26_init_timers();

	printk("BEGIN TIMER TEST\n");
#ifdef ARCH_arm
	printk("jiffies: %ld, HZ: %ld\n", jiffies, HZ);
#else
	printk("jiffies(%p): %ld, HZ(%p): %ld\n", &jiffies, jiffies, &HZ, HZ);
#endif

	setup_timer(&_timer, tick_func, 0);
	_timer.expires = jiffies + HZ;
	add_timer(&_timer);

	setup_timer(&_timer5, timer_func, 5);
	_timer5.expires = jiffies + 5*HZ;
	setup_timer(&_timer10, timer_func, 10); 
	_timer10.expires = jiffies + 10*HZ;
	setup_timer(&_timer15, timer_func, 15); 
	_timer15.expires = jiffies + 15*HZ;
	setup_timer(&_timer20, timer_func, 20); 
	_timer20.expires = jiffies + 20*HZ;
	setup_timer(&_timer25, timer_func, 25); 
	_timer25.expires = jiffies + 25*HZ;

	add_timer(&_timer5);
	add_timer(&_timer10);
	add_timer(&_timer15);
	add_timer(&_timer20);
	add_timer(&_timer25);

	msleep(30000);

	del_timer(&_timer);
	printk("END TIMER TEST\n");
}


/******************************
 ** Test 6: Memory subsystem **
 ******************************/

static void memory_kmem_cache_test(void)
{
	struct kmem_cache *cache0;
	struct obj0
	{
		unsigned foo;
		unsigned bar;
	};
	static struct obj0 *p0[1024];

	struct kmem_cache *cache1;
	struct obj1
	{
		char      foo[50];
		unsigned *bar;
	};
	static struct obj1 *p1[256];

	cache0 = kmem_cache_create("obj0", sizeof(*p0[0]), 0, 0, 0);
	cache1 = kmem_cache_create("obj1", sizeof(*p1[0]), 0, 0, 0);
	printk("kmem caches: %p %p\n", cache0, cache1);

	unsigned i;
	for (i = 0; i < 1024; ++i)
		p0[i] = kmem_cache_alloc(cache0, i);

	for (i = 0; i < 256; ++i)
		p1[i] = kmem_cache_alloc(cache1, i);

	for (i = 256; i > 0; --i)
		kmem_cache_free(cache1, p1[i-1]);

	for (i = 1024; i > 0; --i)
		kmem_cache_free(cache0, p0[i-1]);

	kmem_cache_destroy(cache1);
	kmem_cache_destroy(cache0);
	printk("Done testing kmem_cache_alloc() & co.\n");
}


static void memory_page_alloc_test(void)
{
	unsigned long p[4];
	p[0] = __get_free_page(GFP_KERNEL);
	p[1] = __get_free_pages(GFP_KERNEL, 1);
	p[2] = __get_free_pages(GFP_KERNEL, 2);
	p[3] = __get_free_pages(GFP_KERNEL, 3);
	printk("pages: %p %p %p %p\n", p[0], p[1], p[2], p[3]);

	free_pages(p[0], 0);
	free_pages(p[1], 1);
	free_pages(p[2], 2);
	free_pages(p[3], 3);
	printk("Freed pages\n");
}


static void memory_kmalloc_test(void)
{
	// XXX initialized by dde26_init()!
//	l4dde26_kmalloc_init();

	const unsigned count = 33;
	char *p[count];

	int i;
	for (i = 0; i < count; ++i) {
		p[i] = kmalloc(32 + i*15, GFP_KERNEL);
		*p[i] = i;
		printk("p[%d] = %p\n", i, p[i]);
	}

	for (i = count; i > 0; --i)
		if (p[i-1]) kfree(p[i-1]);

	for (i = 0; i < count; ++i) {
		p[i] = kmalloc(3000 + i*20, GFP_KERNEL);
		*p[i] = i;
		printk("p[%d] = %p\n", i, p[i]);
	}

	for (i = count; i > 0; --i)
		if (p[i-1]) kfree(p[i-1]);

}


static void memory_test(void)
{
	printk("memory test\n");
	if (1) memory_kmem_cache_test();
	if (1) memory_page_alloc_test();
	if (1) memory_kmalloc_test();
	printk("End of memory test\n");
}


/****************************************************************************
 ** Test 7: KThreads                                                       **
 ****************************************************************************/
void kthread_test(void)
{
}


/****************************************************************************
 ** Test 8: Work queues                                                    **
 ****************************************************************************/
static void work_queue_func(struct work_struct *data);
static void work_queue_func2(struct work_struct *data);
static struct workqueue_struct *_wq;
static DECLARE_WORK(_wobj, work_queue_func);
static DECLARE_WORK(_wobj2, work_queue_func2);
static int wq_cnt = 0;

static void work_queue_func(struct work_struct *data)
{
	printk("(1) Work queue function... Do some work here...\n");
	if (++wq_cnt < 5)
		queue_work(_wq, &_wobj);
}


static void work_queue_func2(struct work_struct *data)
{
	printk("(2) Work queue function 2... Do some work here...\n");
	if (++wq_cnt < 10)
		schedule_work(&_wobj2);
}


static void work_queue_test(void)
{
	int i;
	printk("BEGIN WQ TEST\n");
	_wq = create_workqueue("HelloWQ");
	BUG_ON(_wq == NULL);
	queue_work(_wq, &_wobj);
	schedule_work(&_wobj2);
	printk("END WQ TEST\n");
}


/****************************************************************************
 ** Test 9: PCI                                                            **
 ****************************************************************************/

void pci_test(void)
{
	l4dde26_init_pci();
}


/*************************************************
 ** Main routine (switch on desired tests here) **
 *************************************************/

int main(int argc, const char **argv)
{
	int test_current = 1;
	int test_kernel_thread = 0;
	int test_wait = 0;
	int test_tasklet = 1;
	int test_timer = 0;
	int test_memory = 0;
	int test_kthread = 0;
	int test_work = 0;
	int test_pci = 0;

	msleep(1000);

	if (parse_cmdline(&argc, &argv,
                'c', "current", "test current() function",
                PARSE_CMD_SWITCH, 1, &test_current,
                'k', "kernel-thread", "test startup of kernel threads",
                PARSE_CMD_SWITCH, 1, &test_kernel_thread,
                'w', "waitqueue", "test wait queues",
                PARSE_CMD_SWITCH, 1, &test_wait,
                't', "tasklet", "test tasklets",
                PARSE_CMD_SWITCH, 1, &test_tasklet,
                'T', "timer", "test timers",
                PARSE_CMD_SWITCH, 1, &test_timer,
                'm', "memory", "test memory management",
                PARSE_CMD_SWITCH, 1, &test_memory,
                'K', "kthread", "test kthreads",
                PARSE_CMD_SWITCH, 1, &test_kthread,
                'W', "workqueue", "test work queues",
                PARSE_CMD_SWITCH, 1, &test_work,
                'p', "pci", "test PCI stuff",
                PARSE_CMD_SWITCH, 1, &test_pci,
                0, 0))
		return 1;

	printk("DDEKit test. Carrying out tests:\n");
	printk("\t* current()\n");
	printk("\t* kernel_thread()\n");
	printk("\t* wait queues\n");
	printk("\t* tasklets\n");
	printk("\t* timers\n");
	printk("\t* memory management\n");
	printk("\t* kthreads\n");
	printk("\t* work queues\n");
	printk("\t* PCI subsystem\n");

#if 0
	printk("l4dde26_init()\n");
	l4dde26_init();
	printk("l4dde26_process_init()\n");
	l4dde26_process_init();
	printk("l4dde26_do_initcalls()\n");
	l4dde26_do_initcalls();
#endif

	printk("Init done. Running tests.\n");
	if (test_current) current_test();
	if (test_kernel_thread) kernel_thread_test();
	if (test_wait) wq_test();
	if (test_tasklet) tasklet_test();
	if (test_timer) timer_test();
	if (test_memory) memory_test();
	if (test_kthread) kthread_test();
	if (test_work) work_queue_test();
//	if (test_pci) pci_test();
	printk("Test done.\n");

	l4_sleep_forever();

	return 0;
}
