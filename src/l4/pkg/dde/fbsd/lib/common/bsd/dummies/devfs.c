/**
 * As access to device special files is not (yet) needed these
 * function bodies are empty. Later device creation/destruction
 * can be handled here.
 *
 * \author Thomas Friebel <tf13@os.inf.tu-dresden.de>
 */
#include <sys/types.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

MALLOC_DEFINE(M_DEVFS, "devfs", "device structs");

void devfs_create(struct cdev *dev) {
}

void devfs_destroy(struct cdev *dev) {
}

struct cdev *make_dev(struct cdevsw *devsw, int minornr, uid_t uid, gid_t gid, int perms, const char *fmt, ...) {
	return (struct cdev*) malloc(sizeof(struct cdev), M_DEVFS, M_WAITOK);
}

void destroy_dev(struct cdev *dev) {
}

const char *devtoname(struct cdev *dev) {
	return "";
}
