#ifndef __ASM_L4__GENERIC__ASSERT_H__
#define __ASM_L4__GENERIC__ASSERT_H__

#define ASSERT(x)							\
  if (! (x)) panic("assertion (%s)\n  failed in "			\
		   "function %s in " __BASE_FILE__ " at line %d\n",	\
		   #x , __func__, __LINE__);


#endif /* ! __ASM_L4__GENERIC__ASSERT_H__ */
