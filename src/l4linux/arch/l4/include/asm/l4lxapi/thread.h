/*
 * This header file defines the L4Linux internal interface
 * for thread management. All API must implement it.
 */
#ifndef __ASM_L4__L4LXAPI__THREAD_H__
#define __ASM_L4__L4LXAPI__THREAD_H__

#include <asm/generic/kthreads.h>

/* Convenience include */
#include <asm/l4lxapi/generic/thread_gen.h>

/**
 * \defgroup thread Thread management functions.
 * \ingroup l4lxapi
 */

/**
 * \internal
 * \ingroup thread
 */
struct l4lx_thread_start_info_t {
	l4_cap_idx_t l4cap;
	l4_umword_t ip, sp;
	u16 pcpu;
	u16 prio;
};

/**
 * \brief Initialize thread handling.
 * \ingroup thread
 *
 * Call before any thread is created.
 */
void l4lx_thread_init(void);

/**
 * \brief Create a thread.
 * \ingroup thread
 *
 * \param thread        Returned thread handle
 * \param thread_func	Thread function.
 * \param cpu_nr        CPU to start thread on.
 * \param stack_pointer	The stack, if set to NULL a stack will be allocated.
 *			This is the stack pointer from the top of the stack
 *			which means that you can put other data on the top
 *			of the stack yourself. If you supply your stack
 *			yourself you have to make sure your stack is big
 *			enough.
 * \param stack_data	Pointer to some data which will be copied to the
 *			stack. It can be used to transfer data to your new
 *			thread.
 * \param stack_data_size Size of your data pointed to by the stack_data
 *			pointer.
 * \param l4cap		Capability slot to be used for new thread.
 * \param prio		Priority of the thread. If set to -1 the default
 *			priority will be choosen (i.e. no prio will be set).
 * \param utcb          Utcb to use, if NULL, a UTCB will be allocated.
 * \param vcpu          Pointer to vcpu-pointer, if NULL, a vCPU state will
 *                      be allocated.
 * \param name		String describing the thread. Only used for
 *			debugging purposes.
 * \param deferstart    If non-null will store start info into deferstart
 *                      and not start the thread. The thread must be started
 *                      with l4lx_thread_start then. If NULL, the thread
 *                      will be started by l4lx_thread_create.
 *
 * \return 0 for success, error code otherwise
 *
 * The stack layout for non L4Env threads:
 *
 * <pre>
 * Stack                                    Stack
 * bottom                                    top
 * ___________________________________...._____
 * |                            | | |      |  |
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *      <===============       ^ ^ ^   ^     ^
 *         Stack growth        | | |   |     |--- thread ID (not always)
 *                             | | |   |--------- data for new thread
 *                             | | |------------- pointer of data section
 *                             | |                given to the new thread
 *                             | |--------------- fake return address
 *                             |----------------- ESP for new thread
 * </pre>
 */
int l4lx_thread_create(l4lx_thread_t *thread,
                       L4_CV void (*thread_func)(void *data),
                       unsigned cpu_nr,
                       void *stack_pointer,
                       void *stack_data, unsigned stack_data_size,
                       l4_cap_idx_t l4cap, int prio,
                       l4_utcb_t *utcb,
                       l4_vcpu_state_t **vcpu_state,
                       const char *name,
                       struct l4lx_thread_start_info_t *deferstart);

/**
 * \brief Start thread.
 * \ingroup thread
 *
 * \param startinfo  Structure filled out by l4lx_thread_create.
 *
 * \return 0 for success, error code otherwise.
 */
int l4lx_thread_start(struct l4lx_thread_start_info_t *startinfo);

/**
 * \brief Check whether a thread is valid.
 *
 * \param t Thread.
 *
 * \return != 0 if valid, 0 if not
 */
L4_INLINE int l4lx_thread_is_valid(l4lx_thread_t t);

/**
 * \brief Get cap of a thread.
 *
 * \param t Thread.
 *
 * \return Cap of thread t.
 */
L4_INLINE l4_cap_idx_t l4lx_thread_get_cap(l4lx_thread_t t);

/**
 * \brief Change the pager of a (kernel) thread.
 * \ingroup thread
 *
 * \param thread	Thread to modify.
 * \param pager		Pager thread.
 */
void l4lx_thread_pager_change(l4_cap_idx_t thread, l4_cap_idx_t pager);

/**
 * \brief Change pager of given thread to the kernel pager.
 * \ingroup thread
 *
 * \param thread        Thread to modify.
 */
void l4lx_thread_set_kernel_pager(l4_cap_idx_t thread);

/**
 * \brief Shutdown a thread.
 * \ingroup thread
 *
 * \param u           Thread to kill
 * \param free_utcb   If true, UTCB memory will be freed.
 * \param v           Optional vcpu state
 * \param do_cap_free Free the thread capability at the allocator.
 *
 * Note that the thread capability will not be freed if a thread shuts down
 * itself.
 */
void l4lx_thread_shutdown(l4lx_thread_t u, unsigned free_utcb,
                          void *v, int do_cap_free);

/**
 * \brief Check if two thread ids are equal. Do not use with different
 *        tasks (at least in L4ENV)!
 * \ingroup thread
 *
 * \param  t1		Thread 1.
 * \param  t2		Thread 2.
 *
 * \return 1 if threads are equal, 0 if not.
 */
L4_INLINE int l4lx_thread_equal(l4_cap_idx_t t1, l4_cap_idx_t t2);

/*
 * Somewhat private init function.
 */
void l4lx_thread_utcb_alloc_init(void);

/*****************************************************************************
 * Include inlined implementations
 *****************************************************************************/

#include <asm/l4lxapi/impl/thread.h>

#endif /* ! __ASM_L4__L4LXAPI__THREAD_H__ */
