#ifndef __CONTXT_EVH_H__
#define __CONTXT_EVH_H__

extern int evh_init(void);

extern l4semaphore_t keysem;
extern int __shift;
extern int __ctrl;
extern int __keyin;
extern l4_threadid_t evh_l4id;

#endif

