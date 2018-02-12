
/*
 * Resources are tree-like, allowing
 * nesting etc..
 */
struct resource {
	const char *name;
	unsigned long start, end;
	unsigned long flags;
	struct resource *parent, *sibling, *child;
};

extern void * request_region(unsigned long start, unsigned long len, const char *name);
extern void release_region(unsigned long start, unsigned long len);
