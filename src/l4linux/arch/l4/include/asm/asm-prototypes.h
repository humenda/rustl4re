/* Global makefiles have hard-coded paths for this file */
#ifdef CONFIG_X86
#include "../../x86/include/asm/asm-prototypes.h"
#endif
#if defined(CONFIG_ARM) || defined(CONFIG_ARM64)
#include "../../../include/asm-generic/asm-prototypes.h"
#endif
