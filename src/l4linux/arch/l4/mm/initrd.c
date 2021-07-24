#include <linux/init.h>

#include <asm/generic/setup.h>

#ifdef CONFIG_BLK_DEV_INITRD
void __init free_initrd_mem(unsigned long start, unsigned long end)
{
       l4x_free_initrd_mem();
}
#endif
