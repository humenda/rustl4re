#include <linux/module.h>
#include <asm/generic/log.h>

static int __init l4x_module_init(void)
{
	printk("Hi from the sample module\n");
	l4x_printf("sample module: Also a warm welcome to the console\n");
	return 0;
}

static void __exit l4x_module_exit(void)
{
	l4x_printf("Bye from sample module\n");
}

module_init(l4x_module_init);
module_exit(l4x_module_exit);

MODULE_AUTHOR("Adam Lackorzynski <adam@os.inf.tu-dresden.de>");
MODULE_DESCRIPTION("L4Linux sample module");
MODULE_LICENSE("GPL");
