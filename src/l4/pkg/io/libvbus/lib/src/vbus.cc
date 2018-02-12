/*
 * (c) 2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>,
 *          Torsten Frenzel <frenzel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/vbus/vbus.h>
#include <l4/vbus/vbus>
#include <l4/cxx/ipc_stream>
#include <l4/util/splitlog2.h>

#include <l4/vbus/vdevice-ops.h>

int
l4vbus_get_device_by_hid(l4_cap_idx_t vbus, l4vbus_device_handle_t parent,
                         l4vbus_device_handle_t *child, char const *hid,
                         int depth, l4vbus_device_t *devinfo)
{
  L4::Ipc::Iostream s(l4_utcb());
  s << parent << l4_uint32_t(L4vbus_vdevice_get_by_hid) << *child << depth
    << hid;
  int err = l4_error(s.call(vbus, L4vbus::Vbus::Protocol));
  if (err < 0)
    return err;

  s >> *child;

  if (devinfo)
    s.get(*devinfo);
  else
    s.skip<l4vbus_device_t>(1);
  return err;
}

int
l4vbus_get_next_device(l4_cap_idx_t vbus, l4vbus_device_handle_t parent,
                       l4vbus_device_handle_t *child, int depth,
                       l4vbus_device_t *devinfo)
{
  L4::Ipc::Iostream s(l4_utcb());
  s << parent << l4_uint32_t(L4vbus_vdevice_get_next) << *child << depth;
  int err = l4_error(s.call(vbus, L4vbus::Vbus::Protocol));
  if (err < 0)
    return err;

  s >> *child;

  if (devinfo)
    s.get(*devinfo);

  return err;
}

int
l4vbus_get_device(l4_cap_idx_t vbus, l4vbus_device_handle_t dev,
                  l4vbus_device_t *devinfo)
{
  L4::Ipc::Iostream s(l4_utcb());
  s << dev << l4_uint32_t(L4vbus_vdevice_get);
  int err = l4_error(s.call(vbus, L4vbus::Vbus::Protocol));
  if (err < 0)
    return err;

  s >> dev;

  if (devinfo)
    s.get(*devinfo);

  return err;
}

int
l4vbus_get_resource(l4_cap_idx_t vbus, l4vbus_device_handle_t dev,
                    int res_idx, l4vbus_resource_t *res)
{
  L4::Ipc::Iostream s(l4_utcb());
  s << dev << l4_uint32_t(L4vbus_vdevice_get_resource) << res_idx;
  int err = l4_error(s.call(vbus, L4vbus::Vbus::Protocol));
  if (err < 0)
    return err;

  s.get(*res);
  return err;
}

static
int
__vbus_request_port(l4_cap_idx_t vbus, l4vbus_resource_t const &res)
{
  l4vbus_resource_t r;
  r.type = res.type;
  r.start = res.start;
  while (r.start <= res.end)
    {
      l4_uint16_t log2_size = l4util_splitlog2_size(r.start, res.end);
      r.end = r.start + (1UL << log2_size) -1;

      L4::Ipc::Iostream s(l4_utcb());
      s << l4vbus_device_handle_t(0)
        << L4::Opcode(L4vbus_vbus_request_resource);
      s.put(r);
      s << L4::Ipc::Rcv_fpage::io(r.start, log2_size, 0);

      long err= l4_error(s.call(vbus, L4vbus::Vbus::Protocol));
      if (err < 0)
	return err;
      r.start += (1 << log2_size);
    }

  return 0;
}

int
l4vbus_request_resource(l4_cap_idx_t vbus, l4vbus_resource_t const *res,
                        int /*flags*/)
{
  if (res->type == L4VBUS_RESOURCE_PORT)
    return __vbus_request_port(vbus, *res);
  return -L4_EINVAL;
}

int
l4vbus_assign_dma_domain(l4_cap_idx_t vbus, unsigned domain_id,
                         unsigned flags, l4_cap_idx_t dma_space)
{
  L4::Ipc::Iostream s(l4_utcb());
  s << l4vbus_device_handle_t(0)
    << L4::Opcode(L4vbus_vbus_assign_dma_domain)
    << domain_id << flags;

  if (l4_is_valid_cap(dma_space))
    s << L4::Ipc::make_cap(L4::Cap<void>(dma_space), L4_CAP_FPAGE_RWS).fpage();

  return l4_error(s.call(vbus, L4vbus::Vbus::Protocol));
}

int
l4vbus_release_resource(l4_cap_idx_t vbus, l4vbus_resource_t const *res)
{
  L4::Ipc::Iostream s(l4_utcb());
  s << l4vbus_device_handle_t(0)
    << l4_uint32_t(L4vbus_vbus_release_resource) << res->type << res->start
    << res->end;
  int err = l4_error(s.call(vbus, L4vbus::Vbus::Protocol));

  return err;
}

int
l4vbus_is_compatible(l4_cap_idx_t vbus, l4vbus_device_handle_t dev,
                     char const *cid)
{
  L4::Ipc::Iostream s(l4_utcb());
  s << dev << l4_uint32_t(L4vbus_vdevice_is_compatible) << cid;
  return l4_error(s.call(vbus, L4vbus::Vbus::Protocol));
}

int
l4vbus_get_hid(l4_cap_idx_t vbus, l4vbus_device_handle_t dev, char *hid,
               unsigned long max_len)
{
  unsigned long len = max_len;
  L4::Ipc::Iostream s(l4_utcb());
  s << dev << l4_uint32_t(L4vbus_vdevice_get_hid);
  int err = l4_error(s.call(vbus, L4vbus::Vbus::Protocol));
  if (err < 0)
    return err;

  s >> L4::Ipc::buf_cp_in(hid, len);
  return len;
}

int
l4vbus_vicu_get_cap(l4_cap_idx_t vbus, l4vbus_device_handle_t icu,
                    l4_cap_idx_t res)
{
  L4::Ipc::Iostream s(l4_utcb());
  s << icu << l4_uint32_t(L4vbus_vicu_get_cap);
  s << L4::Ipc::Small_buf(res);
  int err = l4_error(s.call(vbus, L4vbus::Vbus::Protocol));

  return err;

}
