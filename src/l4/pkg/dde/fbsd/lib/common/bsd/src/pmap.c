#include <sys/types.h>
#include <sys/param.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include <l4/dde/ddekit/pgtab.h>

vm_paddr_t pmap_kextract(vm_offset_t va) {
	ddekit_addr_t pte;
	unsigned offs;

	offs = va & PAGE_MASK;

	pte = ddekit_pgtab_get_physaddr((void*)va);

	return (vm_paddr_t) (pte + offs);
}
