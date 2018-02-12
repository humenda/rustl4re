/*
 * \brief   DDE for Linux 2.6 Unit tests
 * \author  Bjoern Doebel <doebel@os.inf.tu-dresden.de>
 * \author  Christian Helmuth <ch12@os.inf.tu-dresden.de>
 * \date    2007-05-12
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

#include <l4/cunit/CUnit.h>
#include <l4/cunit/Basic.h>
#include <l4/util/parse_cmd.h>
#include <l4/util/util.h>
#include <l4/log/l4log.h>

/* We define 4 initcalls and see if these are executed
 * in the beginning and in the right order. Expecations:
 *
 * - After running the init calls, _init_count == 4
 * - {_foo, _bar, _bla, _blub}_count carry numbers from 0 through 3
 *   giving their execution order. These need to be:
 *   - _foo_count == 3
 *   - _bar_count == 2
 *   - _bla_count == 0
 *   - _blub_count == 1
 */
static unsigned _init_count = 0;
static unsigned _foo_count  = 0;
static unsigned _bar_count  = 0;
static unsigned _bla_count  = 0;
static unsigned _blub_count = 0;

/** Test checking whether the initcalls have been executed correctly. */
static int test_initcalls(void)
{
	CU_ASSERT(_init_count == 4);
	CU_ASSERT(_foo_count  == 3);
	CU_ASSERT(_bar_count  == 2);
	CU_ASSERT(_blub_count == 1);
	CU_ASSERT(_bla_count  == 0);
	return 0;
}


static void foo(void) 
{
	_foo_count = _init_count++;
}
late_initcall(foo);

static void bar(void) 
{
	_bar_count = _init_count++;
}
device_initcall(bar);

static void bla(void) 
{
	_bla_count = _init_count++;
}
arch_initcall(bla);

static void blub(void) 
{
	_blub_count = _init_count++;
}
subsys_initcall(blub);

/***********************************************************************
 ** Test 1: Check whether the current() macro works.                  **
 ***********************************************************************/
static int current_test(void)
{
	struct task_struct *t = current;
	CU_ASSERT(t != NULL);
	return 0;
}


/***********************************************************************
 ** Test 2: Getting complicated. Test startup of some kernel threads  **
 **         and wait for them to finish using completions.            **
 ***********************************************************************/
#define NUM_KTHREADS	5
static struct completion _kthread_completions_[NUM_KTHREADS];

static int kernel_thread_func(void *arg)
{
	CU_ASSERT((int)arg >= 0);
	CU_ASSERT((int)arg < NUM_KTHREADS);
	CU_ASSERT(current != NULL);

	/* do some work */
	msleep(200);

	complete_and_exit( &_kthread_completions_[(int)arg], 0 );
	return 0;
}


static int kernel_thread_test(void)
{
	int i;
	
	for (i=0; i < NUM_KTHREADS; i++) {
		int j;
		// initialize completion event
		init_completion(&_kthread_completions_[i]);
		// start kernel thread
		j = kernel_thread(kernel_thread_func, (void *)i, 0);
		CU_ASSERT(j > 0);
	}

	for (i=0; i < NUM_KTHREADS; i++) {
		// await completion
		wait_for_completion(&_kthread_completions_[i]);
	}
	CU_ASSERT(i==NUM_KTHREADS);

	return 0;
}


/******************************************************************************
 ** Test 3: Test kernel wait queues: start a thread incrementing wait_value, **
 **         and  sleep until wait_value is larger than 6 for the first time. **
 ******************************************************************************/
static DECLARE_WAIT_QUEUE_HEAD(_wq_head);
static unsigned wait_value = 0;
static struct completion wq_completion;

static int inc_func(void *arg)
{
	int i = 0;

	for (i=0; i<10; i++)
	{
		++wait_value;
		wake_up(&_wq_head);
		msleep(500);
	}
	CU_ASSERT(wait_value == 10);
	CU_ASSERT(i == 10);
	complete_and_exit(&wq_completion, 0);
}


static int wq_test(void)
{
	int pid;

	init_completion(&wq_completion);
	// start a thread incrementing wait_value
	pid = kernel_thread(inc_func, 0, 0);
	CU_ASSERT(pid > 0);

	// wait until wait_value > 6
	wait_event(_wq_head, wait_value > 6);
	CU_ASSERT(wait_value > 6);

	// wait for inc_thread to exit
	wait_for_completion(&wq_completion);
	CU_ASSERT(wait_value == 10);

	return 0;
}


/****************************************************************************
 ** Test 4: Tasklets                                                       **
 ****************************************************************************/
static unsigned _low_count = 0;
static unsigned _high_count = 0;

static void tasklet_low_func(unsigned long i)
{
	++_low_count;
}

static void tasklet_high_func(unsigned long i)
{
	++_high_count;
}

static DECLARE_TASKLET(low0, tasklet_low_func, 0);
static DECLARE_TASKLET(low1, tasklet_low_func, 1);
static DECLARE_TASKLET(low2, tasklet_low_func, 2);
static DECLARE_TASKLET_DISABLED(low3, tasklet_low_func, 3);

static DECLARE_TASKLET(hi0, tasklet_high_func, 10);
static DECLARE_TASKLET(hi1, tasklet_high_func, 11);
static DECLARE_TASKLET_DISABLED(hi2, tasklet_high_func, 12);


static int tasklet_test(void)
{
	l4dde26_softirq_init();

	// schedule tasklets 0-3, 3 disabled
	tasklet_schedule(&low0);
	tasklet_schedule(&low1);
	tasklet_schedule(&low2);
	tasklet_schedule(&low3);
	msleep(500);

	// 0-2 should have executed by now
	CU_ASSERT(_low_count == 3);
	tasklet_enable(&low3);
	msleep(500);
	// 3 should be ready, too
	CU_ASSERT(_low_count == 4);
	
	// schedule hi and low tasklets, hi2 is disabled
	tasklet_hi_schedule(&hi0);
	tasklet_hi_schedule(&hi1);
	tasklet_hi_schedule(&hi2);
	tasklet_schedule(&low0);
	tasklet_schedule(&low1);
	tasklet_schedule(&low2);
	msleep(500);
	CU_ASSERT(_high_count == 2);
	CU_ASSERT(_low_count  == 7);

	// enable hi2
	tasklet_enable(&hi2);
	// schedule low3 2 times, should only run once
	tasklet_schedule(&low3);
	tasklet_schedule(&low3);
	msleep(500);
	CU_ASSERT(_high_count == 3);
	CU_ASSERT(_low_count == 8);

	return 0;
}


#if 0
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
	printk("jiffies(%p): %ld, HZ(%p): %ld\n", &jiffies, jiffies, &HZ, HZ);

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
	printk("END TIMER TEST\n");
}
#endif


/******************************
 ** Test 6: Memory subsystem **
 ******************************/

static void memory_kmem_cache_test(void)
{
	struct kmem_cache *cache0 = NULL;
	struct obj0
	{
		unsigned foo;
		unsigned bar;
	};
	static struct obj0 *p0[1024];

	struct kmem_cache *cache1 = NULL;
	struct obj1
	{
		char      foo[50];
		unsigned *bar;
	};
	static struct obj1 *p1[256];

	CU_ASSERT(cache0 == NULL);
	CU_ASSERT(cache1 == NULL);

	cache0 = kmem_cache_create("obj0", sizeof(*p0[0]), 0, 0, 0);
	cache1 = kmem_cache_create("obj1", sizeof(*p1[0]), 0, 0, 0);

	CU_ASSERT(cache0 != NULL);
	CU_ASSERT(cache1 != NULL);

	unsigned i;
	for (i = 0; i < 1024; ++i) {
		p0[i] = kmem_cache_alloc(cache0, i);
		CU_ASSERT(p0[i] != NULL);
	}

	for (i = 0; i < 256; ++i) {
		p1[i] = kmem_cache_alloc(cache1, i);
		CU_ASSERT(p1[i] != NULL);
	}

	for (i = 256; i > 0; --i)
		kmem_cache_free(cache1, p1[i-1]);

	for (i = 1024; i > 0; --i)
		kmem_cache_free(cache0, p0[i-1]);

	kmem_cache_destroy(cache1);
	kmem_cache_destroy(cache0);
}


static void memory_page_alloc_test(void)
{
	unsigned long p[4];
	p[0] = __get_free_page(GFP_KERNEL);
	p[1] = __get_free_pages(GFP_KERNEL, 1);
	p[2] = __get_free_pages(GFP_KERNEL, 2);
	p[3] = __get_free_pages(GFP_KERNEL, 3);

	CU_ASSERT(p[0] != 0);
	CU_ASSERT(p[1] != 0);
	CU_ASSERT(p[2] != 0);
	CU_ASSERT(p[3] != 0);

	free_pages(p[0], 0);
	free_pages(p[1], 1);
	free_pages(p[2], 2);
	free_pages(p[3], 3);
}


static void memory_kmalloc_test(void)
{
	l4dde26_kmalloc_init();

	const unsigned count = 33;
	char *p[count];

	unsigned i;
	for (i = 0; i < count; ++i) {
		p[i] = kmalloc(32 + i*15, GFP_KERNEL);
		CU_ASSERT(p[i] != NULL);
		*p[i] = i;
		CU_ASSERT(*p[i] == i);
	}

	for (i = count; i > 0; --i)
		if (p[i-1]) kfree(p[i-1]);

	for (i = 0; i < count; ++i) {
		p[i] = kmalloc(3000 + i*20, GFP_KERNEL);
		CU_ASSERT(p[i] != NULL);
		*p[i] = i;
		CU_ASSERT(*p[i] == i);
	}

	for (i = count; i > 0; --i)
		if (p[i-1]) kfree(p[i-1]);

}


static int memory_test(void)
{
	if (1) memory_kmem_cache_test();
	if (1) memory_page_alloc_test();
	if (1) memory_kmalloc_test();
	return 0;
}

#if 0
/****************************************************************************
 ** Test 7: KThreads                                                       **
 ****************************************************************************/
void kthread_test(void)
{
}


/****************************************************************************
 ** Test 8: Work queues                                                    **
 ****************************************************************************/
static void work_queue_func(void *data);
static void work_queue_func2(void *data);
static struct workqueue_struct *_wq;
static DECLARE_WORK(_wobj, work_queue_func, NULL);
static DECLARE_WORK(_wobj2, work_queue_func2, NULL);
static int wq_cnt = 0;

static void work_queue_func(void *data)
{
	printk("Work queue function... Do some work here...\n");
	if (++wq_cnt < 5)
		queue_work(_wq, &_wobj);
}


static void work_queue_func2(void *data)
{
	printk("Work queue function 2... Do some work here...\n");
	if (++wq_cnt < 5)
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
	int test_current = 0;
	int test_kernel_thread = 0;
	int test_wait = 0;
	int test_tasklet = 0;
	int test_timer = 0;
	int test_memory = 0;
	int test_kthread = 0;
	int test_work = 0;
	int test_pci = 0;

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

	LOG("DDEKit test. Carrying out tests:");
	LOGd(test_current, "\t* current()");
	LOGd(test_kernel_thread, "\t* kernel_thread()");
	LOGd(test_wait, "\t* wait queues");
	LOGd(test_tasklet, "\t* tasklets");
	LOGd(test_timer, "\t* timers");
	LOGd(test_memory, "\t* memory management");
	LOGd(test_kthread, "\t* kthreads");
	LOGd(test_work, "\t* work queues");
	LOGd(test_pci, "\t* PCI subsystem");

	l4dde26_init();
	l4dde26_process_init();
	l4dde26_do_initcalls();

	if (test_current) current_test();
	if (test_kernel_thread) kernel_thread_test();
	if (test_wait) wq_test();
	if (test_tasklet) tasklet_test();
	if (test_timer) timer_test();
	if (test_memory) memory_test();
/*	if (1) kthread_test(); */
	if (test_work) work_queue_test();
	if (test_pci) pci_test();

	return 0;
}
#endif

static int dde26_ts_init(void)
{
	l4dde26_init();
	l4dde26_process_init();
	l4dde26_do_initcalls();
	return 0;
}


static int dde26_ts_cleanup(void)
{
	return 0;
}


int main(int argc, char **argv)
{
	CU_pSuite dde_testsuite = NULL;
	
	int err = CU_initialize_registry();
	if (err == CUE_SUCCESS)
		printk("Initialized CUnit registry.\n");
	else {
		printk("Could not initialize CUnit registry.\n");
		return -1;
	}

	dde_testsuite = CU_add_suite("DDE2.6 test suite", dde26_ts_init, dde26_ts_cleanup);
	if (dde_testsuite)
		printk("Added DDE2.6 test suite.\n");
	else {
		printk("Could not add DDE2.6 test suite.\n");
		return -2;
	}

	CU_ADD_TEST(dde_testsuite, test_initcalls);
	CU_ADD_TEST(dde_testsuite, current_test);
	CU_ADD_TEST(dde_testsuite, kernel_thread_test);
	CU_ADD_TEST(dde_testsuite, wq_test);
	CU_ADD_TEST(dde_testsuite, tasklet_test);
	CU_ADD_TEST(dde_testsuite, memory_test);

	CU_basic_set_mode(CU_BRM_VERBOSE);
	CU_basic_run_tests();

	CU_cleanup_registry();

	l4_sleep_forever();

	return 0;
}
