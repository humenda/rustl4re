#ifndef _LINUX_FS_H
#define _LINUX_FS_H

struct file;
struct inode;
struct file_operations {
	int (*open) (struct inode *, struct file *);
};

static inline int register_chrdev(unsigned int i, const char *c,
                                  struct file_operations *f) {return 0;};
static inline int unregister_chrdev(unsigned int i, const char *c) {return 0;};

#define fops_get(fops) (fops)
#define fops_put(fops) do {} while (0)

#define iminor(x) (1)

#endif

