/*
 * Created on: 12.11.2013
 *     Author: Matthias Lange <matthias.lange@kernkonzept.com>
 *             Alexander Warg <alexander.warg@kernkonzept.com>
 */

#include <l4/l4virtio/virtio.h>
#include <l4/l4virtio/l4virtio>

L4_CV int
l4virtio_set_status(l4_cap_idx_t cap, unsigned status) L4_NOTHROW
{
  return L4::Cap<L4virtio::Device>(cap)->set_status(status);
}

L4_CV int
l4virtio_config_queue(l4_cap_idx_t cap, unsigned queue) L4_NOTHROW
{
  return L4::Cap<L4virtio::Device>(cap)->config_queue(queue);
}

L4_CV int
l4virtio_register_ds(l4_cap_idx_t cap, l4_cap_idx_t ds_cap,
                     l4_uint64_t base, l4_umword_t offset,
                     l4_umword_t sz) L4_NOTHROW
{
  L4::Ipc::Cap<L4Re::Dataspace> ds;
  ds = L4::Ipc::Cap<L4Re::Dataspace>::from_ci(ds_cap);
  return L4::Cap<L4virtio::Device>(cap)->register_ds(ds, base, offset, sz);
}

L4_CV int
l4virtio_register_iface(l4_cap_idx_t cap, l4_cap_idx_t guest_irq,
                        l4_cap_idx_t host_irq, l4_cap_idx_t config_ds) L4_NOTHROW
{
  L4::Ipc::Cap<L4::Irq> girq;
  girq = L4::Ipc::make_cap(L4::Cap<L4::Irq>(guest_irq), L4_CAP_FPAGE_RW);
  return L4::Cap<L4virtio::Device>(cap)
    ->register_iface(girq, L4::Cap<L4::Irq>(host_irq),
                     L4::Cap<L4Re::Dataspace>(config_ds));
}
