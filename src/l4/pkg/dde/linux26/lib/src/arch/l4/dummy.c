#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/syscalls.h>
#include <linux/mutex.h>
#include <linux/seqlock.h>
#include <linux/srcu.h>
#include <linux/time.h>
#include <linux/blkdev.h>
#include <linux/idr.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <net/sch_generic.h>

#include <l4/dde/ddekit/panic.h>
#include <l4/dde/linux26/dde26.h>

seqlock_t xtime_lock;
int rcu_scheduler_active = 0;
struct timespec wall_to_monotonic;
struct timespec xtime;
u64 jiffies_64 = 0;
int swapper_space;
unsigned long num_physpages;
atomic_long_t vm_committed_space;
const kernel_cap_t __cap_empty_set = CAP_EMPTY_SET;


extern int get_option(char **str, int *pint)
{
	WARN_UNIMPL;
	return -1;
}

int init_srcu_struct(struct srcu_struct *sp)
{
	WARN_UNIMPL;
	return -1;
}

void srcu_read_unlock(struct srcu_struct *sp, int idx)
{
	WARN_UNIMPL;
}

int srcu_read_lock(struct srcu_struct *sp)
{
	WARN_UNIMPL;
	return -1;
}


void synchronize_srcu(struct srcu_struct *sp)
{
	WARN_UNIMPL;
}


int idle_cpu(int cpu)
{
	WARN_UNIMPL;
	return -1;
}

void __register_nosave_region(unsigned long b, unsigned long e, int km)
{
	WARN_UNIMPL;
}

int do_settimeofday(struct timespec *tv)
{
	WARN_UNIMPL;
	return -1;
}

void update_xtime_cache(u64 nsec)
{
	WARN_UNIMPL;
}

void vmalloc_sync_all(void)
{
	WARN_UNIMPL;
}

#if 0
void clock_was_set(void)
{
	WARN_UNIMPL;
}
#endif

void unlock_page(struct page *page)
{
	WARN_UNIMPL;
}


int do_adjtimex(struct timex *a)
{
	WARN_UNIMPL;
	return -1;
}


s64 div_s64_rem(s64 dividend, s32 divisor, s32 *remainder)
{
	return 0;
}


int try_to_release_page(struct page *page, gfp_t gfp_mask)
{
	WARN_UNIMPL;
	return -1;
}

int try_to_free_swap(struct page *p)
{
	WARN_UNIMPL;
	return -1;
}

unsigned find_get_pages(struct address_space *mapping, pgoff_t start,
                        unsigned int nr_pages, struct page **pages)
{
	WARN_UNIMPL;
	return 0;
}


unsigned find_get_pages_tag(struct address_space *mapping, pgoff_t *index,
                        int tag, unsigned int nr_pages, struct page **pages)
{
	WARN_UNIMPL;
	return 0;
}


int __attribute__((weak)) bdi_init(struct backing_dev_info *bdi)
{
	WARN_UNIMPL;
	return -1;
}


long strnlen_user(const char __user *str, long n)
{
	WARN_UNIMPL;
	return -1;
}

void async_synchronize_full(void)
{
	WARN_UNIMPL;
}

int cap_settime(struct timespec *ts, struct timezone *tz)
{
	WARN_UNIMPL;
	return -1;
}


char *get_options(const char *str, int nints, int *ints)
{
	WARN_UNIMPL;
	return NULL;
}

void qdisc_calculate_pkt_len(struct sk_buff *skb,
                             struct qdisc_size_table *stab)
{
	WARN_UNIMPL;
}


void release_sock(struct sock *sk)
{
	WARN_UNIMPL;
}


void sock_wfree(struct sk_buff *skb)
{
	WARN_UNIMPL;
}


int pcibios_set_pcie_reset_state(struct pci_dev *dev,
                                 enum pcie_reset_state state)
{
	WARN_UNIMPL;
	return -1;
}
