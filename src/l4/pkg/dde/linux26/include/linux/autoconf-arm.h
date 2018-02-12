/* Include Linux' contributed autoconf file */
#include_next <linux/autoconf.h>

#undef CONFIG_MODULES

/* Because we don't need INET support */
#undef CONFIG_INET
#undef CONFIG_XFRM
#undef CONFIG_IP_NF_IPTABLES
#undef CONFIG_IP_FIB_HASH
#undef CONFIG_NETFILTER_XT_MATCH_STATE
#undef CONFIG_INET_XFRM_MODE_TRANSPORT
#undef CONFIG_INET_XFRM_MODE_TUNNEL
#undef CONFIG_IP_NF_CONNTRACK
#undef CONFIG_TCP_CONG_BIC
#undef CONFIG_IP_NF_FILTER
#undef CONFIG_IP_NF_FTP
#undef CONFIG_IP_NF_TARGET_LOG

/* No highmem for our drivers */
#undef CONFIG_HIGHMEM
#undef CONFIG_HIGHMEM4G
#define CONFIG_NOHIGHMEM 1

/* No PROC fs for us */
#undef CONFIG_PROC_FS

/* Also, no sysFS */
#undef CONFIG_SYSFS

/* No NFS support */
#undef CONFIG_ROOT_NFS

/* We don't support hotplug */
#undef CONFIG_HOTPLUG

/* No Sysctl */
#undef CONFIG_SYSCTL

/* No power management */
#undef CONFIG_PM

/* irqs assigned statically */
#undef CONFIG_GENERIC_IRQ_PROBE

/* No message-signalled interrupts for PCI. */
#undef CONFIG_PCI_MSI

/* We don't need event counters, because we don't have /proc/vmstat */
#undef CONFIG_VM_EVENT_COUNTERS

#define WANT_PAGE_VIRTUAL

//#define CONFIG_RWSEM_GENERIC_SPINLOCK
#define CONFIG_CPU_SA1100
#define CONFIG_CPU_ARM710
//#define CONFIG_CPU_V6
//#define CONFIG_CPU_TLB_V6
#undef CONFIG_SECCOMP
//#undef CONFIG_SMP
#undef CONFIG_KPROBES
#undef CONFIG_HUGETLB_PAGE
#undef CONFIG_HUGETLBFS
//#define CONFIG_VECTORS_BASE 0
#undef CONFIG_KALLSYMS
