#ifndef __ASM_L4__L4X_I386__IOPORT_H__
#define __ASM_L4__L4X_I386__IOPORT_H__

int l4x_x86_handle_user_port_request(struct task_struct *task,
                                     unsigned nr, unsigned len);

#endif /* ! __ASM_L4__L4X_I386__IOPORT_H__ */
