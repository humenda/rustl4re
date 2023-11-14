#include <l4/l4rust/vbus.h>

EXTERN int l4vbus_subinterface_supported_w (l4_uint32_t dev_type, l4vbus_iface_type_t iface_type) {
    return l4vbus_subinterface_supported(dev_type, iface_type);
}
