#ifndef __DDE_26_H
#define __DDE_26_H

#include <l4/sys/compiler.h>

__BEGIN_DECLS

#include <l4/dde/ddekit/thread.h>


#define WARN_UNIMPL         printk("unimplemented: %s\n", __FUNCTION__)

/** \defgroup dde26
 *
 * DDELinux2.6 subsystems
 */

/** Initialize process subsystem.
 * \ingroup dde26
 */
//int  l4dde26_process_init(void);

/** Initialize DDELinux locks.
 * \ingroup dde26
 */
void l4dde26_init_locks(void);

/** Initialize SoftIRQ subsystem.
 * \ingroup dde26
 */
void l4dde26_softirq_init(void);

/** Initialize the first DDE process.
 * \ingroup dde26
 */
void l4dde26_process_init(void);

/** Initialize timer subsystem.
 * \ingroup dde26
 */
void l4dde26_init_timers(void);

/** Initialize PCI subsystem.
 * \ingroup dde26
 */
void l4dde26_init_pci(void);

/** Perform initcalls.
 *
 * This will run all initcalls from DDELinux. This includes
 * initcalles specified in the DDELinux libraries as well as
 * initcalls from the device driver.
 *
 * \ingroup dde26
 */
//void l4dde26_do_initcalls(void);

/** Initialize memory subsystem.
 * \ingroup dde26
 */
void l4dde26_kmalloc_init(void);

/** Initialize calling thread to become a DDEKit worker. 
 *
 *  This needs to be called for every thread that will issue
 *  calls to Linux functions.
 *
 * \ingroup dde26
 */
int l4dde26_process_add_worker(void);

/** Enable an existing DDEKit thread to run as a DDELinux process.
 *
 * We need to call this for existing DDEKit threads (e.g., timers)
 * that will end up running Linux code (e.g. timer functions).
 *
 * \ingroup dde26
 */
int l4dde26_process_from_ddekit(ddekit_thread_t *t);

/** Perform vital initcalls to initialize DDELinux 2.6
 *
 * This includes initialization of the DDEKit.
 *
 * \ingroup dde26
 */
void l4dde26_init(void);

__END_DECLS

#endif
