#ifndef _LINUX_INIT_H
#define _LINUX_INIT_H

#define __init			/* ignore */
#define __exit			/* ignore */
#define __obsolete_setup(str)	/* ignore */

#define subsys_initcall(x) module_init(x)
#define module_init(x) \
	int l4input_internal_##x(void) { return x(); }
#define module_exit(x) \
	void l4input_internal_##x(void) { x(); }

/* XXX Could be useful for tweaking */
#define __setup(str, fn) \
	static int(*__setup_##fn)(char*) __attribute__ ((unused)) = fn


#if __GNUC__ == 2 && __GNUC_MINOR__ < 96
#define __builtin_expect(x, expected_value) (x)
#endif

#define likely(x)		__builtin_expect(!!(x), 1)
#define unlikely(x)		__builtin_expect(!!(x), 0)

#endif
