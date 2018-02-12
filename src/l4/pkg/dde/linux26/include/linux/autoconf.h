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
#undef CONFIG_IDE_PROC_FS

/* Also, no sysFS */
#undef CONFIG_SYSFS

/* No NFS support */
#undef CONFIG_ROOT_NFS

/* We don't support hotplug */
#undef CONFIG_HOTPLUG
#undef CONFIG_HOTPLUG_CPU

/* No Sysctl */
#undef CONFIG_SYSCTL

/* No power management */
#undef CONFIG_PM
#undef CONFIG_PM_SLEEP

/* irqs assigned statically */
#undef CONFIG_GENERIC_IRQ_PROBE

/* No message-signalled interrupts for PCI. */
#undef CONFIG_PCI_MSI

/* We don't need event counters, because we don't have /proc/vmstat */
#undef CONFIG_VM_EVENT_COUNTERS

/* No pnp for now */
#undef CONFIG_PNP

/* No IDE ACPI */
#undef CONFIG_BLK_DEV_IDEACPI
#define CONFIG_BLK_DEV_IDEDMA 1

#undef CONFIG_TIMER_STATS
#undef CONFIG_KALLSYMS

/* We still use the SLAB impl, it's mapped to ddekit_slabs anyway. */
#undef CONFIG_SLUB
#define CONFIG_SLAB 1

#define WANT_PAGE_VIRTUAL

#undef CONFIG_SECURITY
#undef CONFIG_FREEZER
#undef CONFIG_AUDIT
#undef CONFIG_AUDITSYSCALL
#undef CONFIG_NETPOLL

/* No wireless extensions for now */
#undef CONFIG_WIRELESS_EXT

/* We don't have IPtables in here */
#undef CONFIG_NF_CONNTRACK
#undef CONFIG_NF_CONNTRACK_IPV4
#undef CONFIG_NF_CONNTRACK_IPV6
#undef CONFIG_NF_CONNTRACK_PROC_COMPAT
#undef CONFIG_NF_CONNTRACK_IRC
#undef CONFIG_NF_CONNTRACK_FTP
#undef CONFIG_NF_CONNTRACK_SIP
#undef CONFIG_NF_CONNTRACK_SECMARK

//#define DEBUG
#undef CONFIG_DEBUG_FS
#undef CONFIG_SCHED_MC
#undef CONFIG_SCHED_SMT
#undef CONFIG_BLK_DEV_IO_TRACE

#undef CONFIG_FW_LOADER
#undef CONFIG_DMI

#define CONFIG_BLK_DEV_IDEDMA_PCI 1
#define CONFIG_BLK_DEV_IDEDMA_SFF 1
