/**
 * copin/out() implementation. Written from scratch.
 *
 * \author Thomas Friebel <tf13@os.inf.tu-dresden.de>
 */

#include <sys/types.h>
#include <sys/systm.h>

int	copyinstr(const void *udaddr, void *kaddr, size_t len, size_t *lencopied) {
	int x;
	char* src;
	char* dst;

	src = (char*) udaddr;
	dst = (char*) kaddr;

	for (x=0; x<len; x++) {
		dst[x] = src[x];
		if (!src[x]) {
			//XXXY: assuming the 0 character doesn't count
			// maybe a typical +-1 error :/
			x--;
			break;
		}
	}

	if (lencopied)
		*lencopied = x;
	return 0;
}

int	copyin(const void *udaddr, void *kaddr, size_t len) {
	memcpy(kaddr, udaddr, len);
	return 0;
}

int	copyout(const void *kaddr, void *udaddr, size_t len) {
	memcpy(udaddr, kaddr, len);
	return 0;
}

