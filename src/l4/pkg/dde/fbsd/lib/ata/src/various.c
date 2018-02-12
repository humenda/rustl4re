#include <sys/types.h>
#include <sys/param.h>
#include <sys/conf.h>
#include <machine/stdarg.h>

#include <l4/dde/ddekit/printf.h>

int set_dumper(struct dumperinfo *di) {
	ddekit_printf("%s should never be called\n", __FUNCTION__);
	return 0;
}
