#pragma once

#include <l4/re/env.h>

#ifdef __cplusplus
#define EXTERN extern "C"
#else
#define EXTERN
#endif
////////////////////////////////////////////////////////////////////////////////
// env.h
//
EXTERN l4_cap_idx_t l4re_env_get_cap_w(char const *name);
