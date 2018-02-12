#include <vm/uma.h>
#include <machine/stdarg.h>

#include <l4/dde/dde.h>
#include <l4/dde/fbsd/dde.h>
#include <l4/dde/fbsd/bsd_dde.h>
#include <l4/log/l4log.h>
#include <l4/sys/l4int.h>

l4_ssize_t l4libc_heapsize = 1*1024*1024;

static int  ctor2(void *mem, int size, void *arg, int flags) {
	LOG_Enter("(mem=%p, size=%d)", mem, size);
	return 0;
}
static void dtor2(void *mem, int size, void *arg) {
	LOG_Enter("(mem=%p, size=%d)", mem, size);
}
static int  init2(void *mem, int size, int flags) {
	LOG_Enter("(mem=%p, size=%d)", mem, size);
	return 0;
}
static void fini2(void *mem, int size) {
	LOG_Enter("(mem=%p, size=%d)", mem, size);
}
static int  ctor3(void *mem, int size, void *arg, int flags) {
	LOG_Enter("(mem=%p, size=%d)", mem, size);
	return 0;
}
static void dtor3(void *mem, int size, void *arg) {
	LOG_Enter("(mem=%p, size=%d)", mem, size);
}
static int  init3(void *mem, int size, int flags) {
	LOG_Enter("(mem=%p, size=%d)", mem, size);
	return 0;
}
static void fini3(void *mem, int size) {
	LOG_Enter("(mem=%p, size=%d)", mem, size);
}

static void ztst1(uma_zone_t zone, int**arr) {
	int i;

	for (i=0; i<=19; i++) {
		arr[i] = (int*) uma_zalloc(zone, 0);
		*(arr[i]) = i;
	}
	for (i=5; i<=9; i++) {
		if (*(arr[i]) != i)
			LOG_Error("bad magic");
		*(arr[i]) = 0xdead;
		uma_zfree(zone, arr[i]);
	}
	for (i=14; i>=10; i--) {
		if (*(arr[i]) != i)
			LOG_Error("bad magic");
		*(arr[i]) = 0xbeef;
		uma_zfree(zone, arr[i]);
	}
	for (i=14; i>=5; i--) {
		arr[i] = (int*) uma_zalloc(zone, 0);
		*(arr[i]) = i;
	}
	for (i=0; i<=19; i++) {
		if (*(arr[i]) != i)
			LOG_Error("bad magic");
		uma_zfree(zone, arr[i]);
	}
}

int main(int argc, char **argv) {
	int* arr1[20];
	int* arr2[20];
	int* arr3[20];

	LOG_Enter("");

	ddekit_init();
	bsd_dde_init();

	uma_zone_t zone1 = uma_zcreate(
		"test zone 1",  400,
		NULL, NULL, NULL, NULL,
		0, 0
	);
	uma_zone_t zone2 = uma_zcreate(
		"test zone 2",   2,
		ctor2, dtor2, init2, fini2,
		UMA_ALIGN_CACHE, UMA_ZONE_NOFREE
	);
	uma_zone_t zone3 = uma_zcreate(
			"test zone 3", 753,
			ctor3, dtor3, init3, fini3,
			UMA_ALIGN_PTR,  UMA_ZONE_ZINIT
	);

	ztst1(zone1, arr1);
	ztst1(zone3, arr3);
	
	arr2[0] = (int*) uma_zalloc(zone2, 0);
	LOGl("the following free should print error msg");
	uma_zfree(zone2, arr2[0]);
	arr2[1] = (int*) uma_zalloc(zone2, 0);
	LOGl("%p and %p should be %d-aligned", arr2[0], arr2[1], UMA_ALIGN_CACHE+1);

	LOGl("creating too big zone");
	uma_zcreate(
		"test zone 4",  5000,
		NULL, NULL, NULL, NULL,
		0, 0
	);

	uma_zdestroy(zone1);
	LOGl("how to destroy a zone with unfreeable items?");
	uma_zdestroy(zone2);
	uma_zdestroy(zone3);

	return 0;
}
