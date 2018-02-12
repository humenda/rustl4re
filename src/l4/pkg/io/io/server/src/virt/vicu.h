/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <l4/sys/capability>
#include <l4/sys/irq>
#include <l4/sys/icu>

#include <l4/sys/cxx/ipc_epiface>
#include <l4/cxx/avl_tree>
#include <l4/cxx/list>

#include <l4/re/util/cap_alloc>

#include "irqs.h"
#include "vdevice.h"

namespace Vi {

class Sw_icu :
  public Device,
  public Dev_feature,
  public L4::Epiface_t<Sw_icu, L4::Icu>
{
public:
  Sw_icu();
  virtual ~Sw_icu();

  l4_uint32_t interface_type() const override
  { return 1 << L4VBUS_INTERFACE_ICU; }

  using L4::Epiface::dispatch;
  int dispatch(l4_umword_t, l4_uint32_t func, L4::Ipc::Iostream &ios) override;

  char const *hid() const override
  { return "L40009"; }

  int op_bind(L4::Icu::Rights, l4_umword_t irqnum,
              L4::Ipc::Snd_fpage irq_fp)
  { return bind_irq(irqnum, irq_fp); }

  int op_unbind(L4::Icu::Rights, l4_umword_t irqnum,
                L4::Ipc::Snd_fpage irq_fp)
  { return unbind_irq(irqnum, irq_fp); }

  int op_info(L4::Icu::Rights, L4::Icu::_Info &ii)
  {
    ii.features = ii.nr_msis = ii.nr_irqs = 0;
    if (_irqs.rbegin() != _irqs.rend())
      ii.nr_irqs = (*_irqs.rbegin()).irqn() + 1;

    if (Int_property *p = dynamic_cast<Int_property*>(get_root()->property("num_msis")))
      {
        _num_msis = ii.nr_msis = p->val();
        ii.features |= L4::Icu::F_msi;
      }
    return 0;
  }

  int op_msi_info(L4::Icu::Rights, l4_umword_t irqnum, l4_uint64_t source,
                  l4_icu_msi_info_t &info);

  int op_mask(L4::Icu::Rights, l4_umword_t)
  { return -L4_ENOSYS; }

  int op_unmask(L4::Icu::Rights, l4_umword_t irqnum)
  {
    unmask_irq(irqnum);
    return -L4_ENOREPLY;
  }

  int op_set_mode(L4::Icu::Rights, l4_umword_t irqnum, l4_umword_t mode)
  { return set_mode(irqnum, mode); }

  bool match_hw_feature(Hw::Dev_feature const *) const override
  { return false; }

  bool add_irqs(Resource const *r);
  bool add_irq(unsigned n, unsigned flags, Io_irq_pin *be);
  int alloc_irq(unsigned flags, Io_irq_pin *be);
  bool irqs_allocated(Resource const *r);

private:
  int bind_irq(unsigned irqn, L4::Ipc::Snd_fpage const &irqc);
  int unbind_irq(unsigned irqn, L4::Ipc::Snd_fpage const &irqc);
  int unmask_irq(unsigned irqn);
  int set_mode(unsigned irqn, l4_umword_t mode);


  class Sw_irq_pin : public cxx::Avl_tree_node
  {
  private:
    enum
    {
      S_unmask_via_icu = 2,
    };

    unsigned _state;
    unsigned _irqn;
    Io_irq_pin *_master;
    typedef  L4Re::Util::Ref_cap<L4::Triggerable>::Cap Triggerable;
    Triggerable _irq;

    int _direct_bind(Triggerable const &irq);

  public:
    enum Irq_type
    {
      S_irq_type_mask  = Resource::Irq_type_mask,
    };

    enum
    {
      S_allow_set_mode = 4,
      S_user_mask = S_irq_type_mask | S_allow_set_mode
    };

    typedef unsigned Key_type;

    static unsigned key_of(Sw_irq_pin const *o) { return o->_irqn; }

    Sw_irq_pin(Io_irq_pin *master, unsigned irqn, unsigned flags)
    : _state(flags & S_user_mask), _irqn(irqn), _master(master)
    {
      if ((flags & Resource::Irq_type_mask) == Resource::Irq_type_none)
        _state |= S_allow_set_mode;

      master->add_sw_irq();
    }

    unsigned irqn() const { return _irqn; }
    L4::Cap<L4::Triggerable> irq() const { return _irq.get(); }

    bool bound();
    bool unmask_via_icu() const { return _state & S_unmask_via_icu; }
    unsigned type() const { return _state & S_irq_type_mask; }
    unsigned l4_type() const;
    int bind(L4::Cap<void> rc);
    int unmask() { return _master->unmask(); }
    int unbind();
    int set_mode(l4_umword_t mode);
    int msi_info(::Io_irq_pin::Msi_src *source, l4_icu_msi_info_t *data)
    { return _master->msi_info(source, data); }
    int trigger() const;

  protected:
    int _unbind(bool deleted);
//    int share(L4Re::Util::Auto_cap<L4::Irq>::Cap const &irq);
    L4::Cap<L4::Irq_mux> allocate_master_irq();
  };

  Sw_irq_pin *get_msi_pin(unsigned msin);

  typedef cxx::Avl_tree<Sw_irq_pin, Sw_irq_pin> Irq_set;
  Irq_set _irqs;
  unsigned _num_msis = 0;
  Irq_set _msis;

public:

  enum
  {
    S_allow_set_mode = Sw_irq_pin::S_allow_set_mode,
  };

  static void *irq_loop(void*);
  void set_host(Device *d) override
  { _host = d; }

  Device *host() const override
  { return _host; }

private:
  Device *_host;
};

}
