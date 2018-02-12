/* Include original DDE26 autoconf file */
#include_next <linux/autoconf.h>

/* Because we do ! need INET support */
#define CONFIG_INET 1
