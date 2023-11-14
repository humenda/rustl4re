#pragma once

#include <l4/vbus/vbus_types.h>
#include <l4/vbus/vbus_interfaces.h>

#ifdef __cplusplus
#define EXTERN extern "C"
#else
#define EXTERN
#endif

EXTERN int l4vbus_subinterface_supported_w (l4_uint32_t dev_type, l4vbus_iface_type_t iface_type);
