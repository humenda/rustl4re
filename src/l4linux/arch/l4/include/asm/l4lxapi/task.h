/*
 * This header files defines the L4Linux internal interface
 * for task management. All APIs must to implement it.
 */
#ifndef __ASM_L4__L4LXAPI__TASK_H__
#define __ASM_L4__L4LXAPI__TASK_H__

#include <l4/sys/types.h>

/**
 * \defgroup task (User-)Task management functions.
 * \ingroup l4lxapi
 */

/**
 * \brief Initialize task management.
 * \ingroup task.
 *
 *
 * General information about tasks:
 *   - The entity called task is meant for user space tasks in L4Linux,
 *     i.e. threads running in another address space then the L4Linux
 *     server
 *   - The term "task" has no connection with L4 tasks.
 *   - The task in L4Linux is represented by an (unsigned) integer
 *     which is non-ambiguous in the L4Linux server (the same number can
 *     exist in several L4Linux servers running in parallel though)
 */
void l4lx_task_init(void);

/**
 * \brief Allocate a task from the task management system for later use.
 * \ingroup task
 *
 * \return A valid task, or L4_NIL_ID if no task could be allocated.
 */
l4_cap_idx_t l4lx_task_number_allocate(void);

/**
 * \brief Free task number after the task has been deleted.
 * \ingroup task
 *
 * \param task		The task to delete.
 *
 * \return 0 on succes, -1 if task number invalid or already free
 */
int l4lx_task_number_free(l4_cap_idx_t task);

/**
 * \brief Allocate a new task number and return threadid for user task.
 * \ingroup task
 *
 * \param	parent_id	If not NIL_ID, a new thread within
 *                              parent_id's address space will be
 *                              allocated, for CLONE_VM tasks.
 *
 * \retval	id		Thread ID of the user thread.
 *
 * \return 0 on success, != 0 on error
 */
int l4lx_task_get_new_task(l4_cap_idx_t parent_id,
                           l4_cap_idx_t *id);

/**
 * \brief Create a (user) task. The pager is the Linux server.
 * \ingroup task
 *
 * \param	task_no	Task number of the task to be created
 *                        (task number is from l4lx_task_allocate()).
 * \return 0 on success, error code otherwise
 *
 * This function additionally sets the priority of the thread 0 to
 * CONFIG_L4_PRIO_USER_PROCESS.
 *
 */
int l4lx_task_create(l4_cap_idx_t task_no);

int l4lx_task_create_thread_in_task(l4_cap_idx_t thread, l4_cap_idx_t task,
                                    l4_cap_idx_t pager, unsigned cpu);


/**
 * \brief Create a (user) task.
 * \ingroup task
 *
 * \param	task_no See l4lx_task_create
 * \param	pager	The pager for this task.
 *
 * \return See l4lx_task_create
 */
int l4lx_task_create_pager(l4_cap_idx_t task_no, l4_cap_idx_t pager);

/**
 * \brief Terminate a task.
 * \ingroup task
 *
 * \param	task	Cap of the task to delete.
 *
 * \return	0 on success, <0 on error
 */
int l4lx_task_delete_task(l4_cap_idx_t task);

/**
 * \brief Terminate a thread.
 * \ingroup task
 *
 * \param	task	Cap of the thread to delete.
 *
 * \return	0 on success, <0 on error
 */
int l4lx_task_delete_thread(l4_cap_idx_t thread);

#endif /* ! __ASM_L4__L4LXAPI__TASK_H__ */
