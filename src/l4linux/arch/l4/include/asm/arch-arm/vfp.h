/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __INCLUDUE__ASM__ARCH_ARM__VFP_H__
#define __INCLUDUE__ASM__ARCH_ARM__VFP_H__

#include <asm-arm/vfp.h>

#undef FPSID
#undef FPSCR
#undef MVFR1
#undef MVFR0
#undef FPEXC
#undef FPINST
#undef FPINST2

#define FPSID   0
#define FPSCR   1
#define MVFR1   6
#define MVFR0   7
#define FPEXC   8
#define FPINST  9
#define FPINST2 10

#endif /* __INCLUDUE__ASM__ARCH_ARM__VFP_H__ */
