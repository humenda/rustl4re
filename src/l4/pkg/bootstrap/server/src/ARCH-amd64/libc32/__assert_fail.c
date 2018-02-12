#include "assert.h"
#include "stdio.h"
#include "stdlib.h"

extern void _exit(int rc);

void
__assert_fail (const char *__assertion, const char *__file,
	       unsigned int __line)
{
  printf("ASSERTION_FAILED (%s)\n"
	 "  in file %s:%d\n", __assertion, __file, __line);
  _exit (EXIT_FAILURE);
}

extern __typeof(__assert_fail) __assert 
  __attribute__((weak, alias("__assert_fail")));
