#ifndef DDE_MEM_H
#define DDE_MEM_H

#include <linux/mm.h>

void dde_page_cache_add(struct page *p);
void dde_page_cache_remove(struct page *p);
struct page* dde_page_lookup(unsigned long va);

#endif
