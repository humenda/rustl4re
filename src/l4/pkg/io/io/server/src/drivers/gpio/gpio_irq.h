/*
 * (c) 2015 Alexander Warg <alexander.warg@kernkonzept.com>
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#pragma once

#include "irqs.h"
#include "debug.h"
#include "main.h"
#include <l4/sys/cxx/ipc_epiface>
#include <l4/cxx/unique_ptr>
#include <l4/sys/irq>

class Gpio_irq_base : public Io_irq_pin
{
private:
  unsigned const _pin;

protected:
  unsigned _mode = L4_IRQ_F_NONE;
  unsigned _enabled = 0;

public:
  explicit Gpio_irq_base(unsigned pin) : _pin(pin) {}
  unsigned pin() const { return _pin; }
  unsigned mode() const { return _mode; }
  bool enabled() const { return _enabled; }

  void trigger() { irq()->trigger(); }

  int bind(Triggerable const &irq, unsigned) override
  {
    set_shareable(false);

    if (_mode == L4_IRQ_F_NONE)
      {
        d_printf(DBG_ERR, "error: Gpio_irq_pin: invalid Irq mode.\n"
                          "       Set Irq mode before bind.\n");
        throw -L4_EINVAL;
      }

    Io_irq_pin::bind(irq, 0);

    return 1;
  }

  int unbind(bool deleted) override
  {
    this->mask();
    Io_irq_pin::unbind(deleted);
    return 0;
  }
};

template<typename IMPL>
class Gpio_irq_base_t : public Gpio_irq_base
{
public:
  explicit Gpio_irq_base_t(unsigned pin) : Gpio_irq_base(pin) {}
  int mask() override
  {
    _enabled = false;
    static_cast<IMPL*>(this)->do_mask();
    return 0;
  }

  int unmask() override
  {
    // make sure client has selected an Irq mode
    // because GPIO irqs don't have a default mode
    if (_mode == L4_IRQ_F_NONE)
      d_printf(DBG_WARN, "warning: Gpio_irq_pin: No Irq mode set.\n"
                         "         You will not receive any Irqs.\n");

    _enabled = true;
    static_cast<IMPL*>(this)->do_unmask();
    return 0;
  }

  int set_mode(unsigned mode) override
  {
    if (mode == L4_IRQ_F_NONE || _mode == mode)
      return _mode;

    if (static_cast<IMPL*>(this)->do_set_mode(mode))
      _mode = mode;

    return _mode;
  }

};

class Irq_demux
{
protected:
  cxx::unique_ptr<Gpio_irq_base*[]> _pins;
  unsigned _hw_irq_num;

public:
  Irq_demux(unsigned hw_irq_num, unsigned mode, unsigned npins)
  : _hw_irq_num(hw_irq_num)
  {
    _pins = cxx::make_unique<Gpio_irq_base*[]>(npins);

    if (l4_error(system_icu()->icu->set_mode(_hw_irq_num, mode)) < 0)
      {
        d_printf(DBG_ERR, "error: Irq_demux: failed to set hw irq mode.\n");
        return;
      }
  }

  void disable()
  { system_icu()->icu->mask(_hw_irq_num); }

  void enable()
  { system_icu()->icu->unmask(_hw_irq_num); }

  template<typename PIN, typename ...ARGS>
  Gpio_irq_base *get_pin(unsigned pin, ARGS &&...args)
  {
    if (_pins[pin])
      return _pins[pin];

    _pins[pin] = new PIN(pin, cxx::forward<ARGS>(args)...);
    return _pins[pin];
  }
};


template<typename IMPL>
class Irq_demux_t : public L4::Irqep_t<IMPL>, public Irq_demux
{
public:
  Irq_demux_t(unsigned hw_irq_num, unsigned mode, unsigned npins)
  : Irq_demux(hw_irq_num, mode, npins)
  {
    registry->register_irq_obj(this);

    // FIXME: should test for unmask via ICU (result of bind ==1)
    if (l4_error(system_icu()->icu->bind(_hw_irq_num, this->obj_cap())) < 0)
      {
        d_printf(DBG_ERR, "error: Irq_demux: failed to bind hw irq\n");
        return;
      }
  }
};

