#include "dma_domain.h"
#include "debug.h"
#include <cassert>
#include <cstdio>

unsigned Dma_domain::_next_free_domain;
bool Dma_domain_if::_supports_remapping;

void
Dma_domain::add_to_group(Dma_domain_group *g)
{
  if (!_v_domain && !g->_set)
    {
      Dma_domain_set *n = new Dma_domain_set();
      g->assign_set(n);
      add_to_set(n);
      return;
    }

  if (_v_domain == g->_set)
    return;

  if (_v_domain && g->_set)
    {
      // domain is already in a group and the target group already contains
      // other domains
      g->merge(_v_domain);
      return;
    }

  if (_v_domain)
    {
      g->assign_set(_v_domain);
      return;
    }

  add_to_set(g->_set);
}

int
Dma_domain_if::set_dma_space(bool set, L4::Cap<L4Re::Dma_space> space)
{
  if (0)
    printf("%s:%d: %s : %s\n", __FILE__, __LINE__, __func__,
           set ? "bind" : "unbind");

  // FIXME: check trustworthyness of `space`
  if (!set)
    return 0; // FIXME: space->disassociate(_kern_dma_space);

  if (!_supports_remapping)
    {
      d_printf(DBG_DEBUG2, "DMA: use CPU-phys addresses for DMA\n");
      return space->associate(L4::Ipc::make_cap(_kern_dma_space, 0),
                              L4Re::Dma_space::Space_attrib::Phys_space);
    }

  if (_kern_dma_space && !_managed_kern_dma_space)
    return -L4_EBUSY;

  if (!_kern_dma_space)
    {
      d_printf(DBG_DEBUG2, "DMA: create kern DMA space for managed DMA\n");
      int r = create_managed_kern_dma_space();
      if (r < 0)
        return r;

      _managed_kern_dma_space = true;
    }

  d_printf(DBG_DEBUG2, "DMA: associate managed DMA space (cap=%lx)\n",
           _kern_dma_space.cap());
  return space->associate(L4::Ipc::make_cap_rws(_kern_dma_space),
                          L4Re::Dma_space::Space_attribs::None);
}

int
Dma_domain_set::create_managed_kern_dma_space()
{
  assert (!_kern_dma_space);

  if (_domains.empty())
    return -L4_ENOENT;

  L4::Cap<L4::Task> kds = L4::Cap<L4::Task>::Invalid;
  for (auto d: _domains)
    {
      if (d->kern_dma_space() && !d->managed_kern_dma_space())
        {
          d_printf(DBG_ERR, "error: conflicting DMA remapping assignment "
                            "(unmamanged DMA domain in group)\n");
          return -L4_EBUSY;
        }

      if (d->kern_dma_space() && kds && kds != d->kern_dma_space())
        {
          d_printf(DBG_ERR, "error: conflicting DMA remapping assignment "
                            "(conflicting DMA domain assignment)\n");
          return -L4_EBUSY;
        }

      if (d->kern_dma_space())
        kds = d->kern_dma_space();
    }

  if (kds)
    d_printf(DBG_INFO, "reuse managed DMA domain for DMA domain group\n");
  else
    {
      int r = _domains[0]->create_managed_kern_dma_space();
      if (r < 0)
        return r;
      kds = _domains[0]->kern_dma_space();
    }

  for (auto d: _domains)
    if (!d->kern_dma_space())
      d->set_managed_kern_dma_space(kds);

  set_managed_kern_dma_space(kds);

  return 0;
}


int
Dma_domain_set::set_dma_task(bool set, L4::Cap<L4::Task> dma_task)
{
  if (managed_kern_dma_space())
    return -L4_EBUSY;

  if (set && kern_dma_space())
    return -L4_EBUSY;

  if (!set && !kern_dma_space())
    return -L4_EINVAL;

  static L4::Cap<L4::Task> const &me = L4Re::This_task;
  if (!set && !me->cap_equal(kern_dma_space(), dma_task).label())
    return -L4_EINVAL;

  L4::Cap<L4::Task> kds = L4::Cap<L4::Task>::Invalid;
  for (auto d: _domains)
    {
      if (d->managed_kern_dma_space())
        {
          d_printf(DBG_ERR, "error: conflicting DMA remapping assignment "
                            "(mamanged DMA domain in group)\n");
          return -L4_EBUSY;
        }

      if (!set)
        continue;

      if (d->kern_dma_space() && kds && !me->cap_equal(kern_dma_space(), kds).label())
        {
          d_printf(DBG_ERR, "error: conflicting DMA remapping assignment "
                            "(conflicting DMA domain assignment)\n");
          return -L4_EBUSY;
        }

      if (d->kern_dma_space())
        kds = d->kern_dma_space();
    }

  for (auto d: _domains)
    {
      int res = 0;
      if (set && !d->kern_dma_space())
        res = d->set_dma_task(true, dma_task);
      else if (!set && d->kern_dma_space())
        res = d->set_dma_task(false, dma_task);

      if (res < 0)
        return res;
    }

  if (set)
    _kern_dma_space = dma_task;
  else
    _kern_dma_space = L4::Cap<L4::Task>::Invalid;

  return 0;
}
