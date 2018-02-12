#ifndef l4_dde_fbsd_dde_h
#define l4_dde_fbsd_dde_h

/**
 * Initialize the DDE. Must be called before any other DDE function.
 * Calls dde_fbsd_setup_bsd_thread() among other things.
 */
void dde_fbsd_init(void);

/**
 * Setup a BSD struct thread and associate it with the current l4 thread.
 * Must be done before calling BSD functions in a newly created thread.
 */
void dde_fbsd_setup_bsd_thread(void);

/**
 * Setup number of buffered pages for slab caches. If too low some are given
 * back to the memory server (dm_phys) and possibly need to be allocated
 * again later which hits performance (but saves memory).
 */
void dde_fbsd_setup_page_buffer(unsigned pages);

#endif
