/**
 * FreeBSD implementaion in
 * kern/tty.c
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/conf.h>

int ttyioctl(struct cdev *dev, u_long cmd, caddr_t data, int flag, struct thread *td) {
	return ENOTTY;
}

int ttykqfilter(struct cdev *dev, struct knote *kn) {
	return ENOTTY;
}

int ttypoll(struct cdev *dev, int events, struct thread *td) {
	return ENOTTY;
}

int ttyread(struct cdev *dev, struct uio *uio, int flag) {
	return ENOTTY;
}

int ttywrite(struct cdev *dev, struct uio *uio, int flag) {
	return ENOTTY;
}

