#pragma once

#include "../../arm/vfp/vfpinstr.h"

#ifdef CONFIG_L4
#ifndef __L4X_ARM_VFP_H__USE_REAL_FUNCS
#undef fmrx
#undef fmxr

unsigned l4x_fmrx(unsigned long reg);
void l4x_fmxr(unsigned long reg, unsigned long val);

#define fmrx(x)    l4x_fmrx(x)
#define fmxr(x, y) l4x_fmxr(x, y)
#endif
#endif
