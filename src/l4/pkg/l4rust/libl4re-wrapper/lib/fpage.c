#include <l4/l4rust/fpage.h>
#include <l4/sys/types.h>

EXTERN l4_fpage_t l4_fpage_w (l4_addr_t address, unsigned int size, unsigned char rights) L4_NOTHROW {
	return l4_fpage(address, size, rights);
}
EXTERN l4_fpage_t l4_fpage_all_w (void) L4_NOTHROW {
	return l4_fpage_all();
}
EXTERN l4_fpage_t l4_fpage_invalid_w (void) L4_NOTHROW {
	return l4_fpage_invalid();
}
EXTERN l4_fpage_t l4_iofpage_w (unsigned long port, unsigned int size) L4_NOTHROW {
	return l4_iofpage(port, size);
}
EXTERN l4_fpage_t l4_obj_fpage_w (l4_cap_idx_t obj, unsigned int order, unsigned char rights) L4_NOTHROW {
	return l4_obj_fpage(obj, order, rights);
}
EXTERN int l4_is_fpage_writable_w (l4_fpage_t fp) L4_NOTHROW {
	return l4_is_fpage_writable(fp);
}
EXTERN unsigned l4_fpage_rights_w (l4_fpage_t f) L4_NOTHROW {
	return l4_fpage_rights(f);
}
EXTERN unsigned l4_fpage_type_w (l4_fpage_t f) L4_NOTHROW {
	return l4_fpage_type(f);
}
EXTERN unsigned l4_fpage_size_w (l4_fpage_t f) L4_NOTHROW {
	return l4_fpage_size(f);
}
EXTERN unsigned long l4_fpage_page_w (l4_fpage_t f) L4_NOTHROW {
	return l4_fpage_page(f);
}
EXTERN l4_addr_t l4_fpage_memaddr_w (l4_fpage_t f) L4_NOTHROW {
	return l4_fpage_memaddr(f);
}
EXTERN l4_cap_idx_t l4_fpage_obj_w (l4_fpage_t f) L4_NOTHROW {
	return l4_fpage_obj(f);
}
EXTERN unsigned long l4_fpage_ioport_w (l4_fpage_t f) L4_NOTHROW {
	return l4_fpage_ioport(f);
}
EXTERN l4_fpage_t l4_fpage_set_rights_w (l4_fpage_t src, unsigned char new_rights) L4_NOTHROW {
	return l4_fpage_set_rights(src, new_rights);
}
EXTERN int l4_fpage_contains_w (l4_fpage_t fpage, l4_addr_t addr, unsigned size) L4_NOTHROW {
	return l4_fpage_contains(fpage, addr, size);
}
EXTERN unsigned char l4_fpage_max_order_w (unsigned char order, l4_addr_t addr, l4_addr_t min_addr, l4_addr_t max_addr, l4_addr_t hotspot) {
	return l4_fpage_max_order(order, addr, min_addr, max_addr, hotspot);
}
