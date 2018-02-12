/* Minimal main program -- everything is loaded from the library */

#include <l4/re/elf_aux.h>
L4RE_ELF_AUX_ELEM_T(l4re_elf_aux_mword_t, _stack_size,
                     L4RE_ELF_AUX_T_STACK_SIZE, 8 * 1024 * 1024);
#include "Python.h"

#ifdef __FreeBSD__
#include <floatingpoint.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
void test_dlopen() {
	void *handle;
	handle = dlopen("rom/_functools.so", 2);
	if (!handle) {
		printf("%s: %s\n", __func__, dlerror());
		exit(-1);
	}
	dlclose(handle);
	exit(0);
}
void test_input() {
  char *buf = (char *)malloc(sizeof(char) * 100);
  if (fgets(buf, 100, stdin) == NULL) {
    printf("fgets error\n");
  } else {
    printf("buf=%s\n", buf);
  }
/*while(1) {
    c = fgetc(stdin);
	printf("%i", c);
  }*/
}

void test_fstat() {
  struct stat statb;
  FILE *fp = NULL;
  fp = fopen("_collections.so", "r");
  if (fp == NULL) {
    printf("fopen failed\n");
	exit(-1);
  }

  if (fstat(fileno(fp), &statb) != 0) {
	  printf("fstat failed\n");
	  exit(-1);
  }

  printf("fstat dev=%lld ino=%ld\n", statb.st_dev, statb.st_ino);
  exit(0);

}

int
main(int argc, char **argv)
{
//	test_input();
//    test_dlopen();
//    test_fstat();
	/* 754 requires that FP exceptions run in "no stop" mode by default,
	 * and until C vendors implement C99's ways to control FP exceptions,
	 * Python requires non-stop mode.  Alas, some platforms enable FP
	 * exceptions by default.  Here we disable them.
	 */
#ifdef __FreeBSD__
	fp_except_t m;

	m = fpgetmask();
	fpsetmask(m & ~FP_X_OFL);
#endif
	return Py_Main(argc, argv);
}
