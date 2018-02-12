#include <sys/types.h>
#include <sys/systm.h>

void bzero(void *buf, size_t len) {
	char *p = buf;

	while (len--)
		*p++ = 0;
}
