#include <l4/util/util.h>

#include <l4/cxx/unique_ptr>

#include "main.h"
#include "irqs.h"
#include "debug.h"
#include "gpio"
#include "hw_device.h"
#include "hw_mmio_register_block.h"
#include "hw_irqs.h"
#include "server.h"
#include "gpio_irq.h"

namespace {

/*
 * Registers with one bit per pin and 3 registers
 * of each kind.
 */
enum Regs
{
  Set     = 0x1c,
  Clr     = 0x28,
  Lev     = 0x34,
  Eds     = 0x40,
  Ren     = 0x4c,
  Fen     = 0x58,
  Hen     = 0x64,
  Len     = 0x70,
  Aren    = 0x7c,
  Afen    = 0x88,
  Pudclk  = 0x98,
};

class Gpio_irq_server;

class Gpio_irq_pin : public Gpio_irq_base_t<Gpio_irq_pin>
{
  Hw::Register_block<32> _regs;

  void write_reg_pin(unsigned reg, unsigned val)
  {
    if (val & 1)
      _regs[reg].set(1 << pin());
    else
      _regs[reg].clear(1 << pin());
  }

public:
  Gpio_irq_pin(unsigned vpin, Hw::Register_block<32> const &regs)
  : Gpio_irq_base_t<Gpio_irq_pin>(vpin), _regs(regs)
  {}

  void do_mask()
  {
    switch (mode())
      {
      case L4_IRQ_F_LEVEL_HIGH:
        write_reg_pin(Hen, 0);
        return;
      case L4_IRQ_F_LEVEL_LOW:
        write_reg_pin(Len, 0);
        return;
      case L4_IRQ_F_POS_EDGE:
        write_reg_pin(Ren, 0);
        return;
      case L4_IRQ_F_NEG_EDGE:
        write_reg_pin(Fen, 0);
        return;
      case L4_IRQ_F_BOTH_EDGE:
        write_reg_pin(Ren, 0);
        write_reg_pin(Fen, 0);
        return;
      }
  }

  void do_unmask()
  {
    switch (mode())
      {
      case L4_IRQ_F_LEVEL_HIGH:
        write_reg_pin(Hen, 1);
        return;
      case L4_IRQ_F_LEVEL_LOW:
        write_reg_pin(Len, 1);
        return;
      case L4_IRQ_F_POS_EDGE:
        write_reg_pin(Ren, 1);
        return;
      case L4_IRQ_F_NEG_EDGE:
        write_reg_pin(Fen, 1);
        return;
      case L4_IRQ_F_BOTH_EDGE:
        write_reg_pin(Ren, 1);
        write_reg_pin(Fen, 1);
        return;
      }
  }

  bool do_set_mode(unsigned mode)
  {
    // this operation touches multiple mmio registers and is thus
    // not atomic, that's why we first mask the IRQ and if it was
    // enabled we unmask it after we have changed the mode
    // masking the Irq also clears the Irq mode on the BCM2835
    if (enabled())
      do_mask();
    _mode = mode;
    if (enabled())
      do_unmask();

    return true;
  }

  int clear() override
  {
    l4_uint32_t e = _regs[Eds] & (1UL << pin());
    if (e)
      _regs[Eds] = e;

    return Io_irq_pin::clear() + (e >> pin());
  }

};

class Gpio_irq_server : public Irq_demux_t<Gpio_irq_server>
{
  l4_uint32_t _pins_mask;
  Hw::Register_block<32> _regs;

public:
  Gpio_irq_server(int irq, unsigned pins, Hw::Register_block<32> const &regs)
  : Irq_demux_t<Gpio_irq_server>(irq, 0, pins),
    _pins_mask(pins >= 32 ? ~l4_uint32_t(0) : (1UL << pins) - 1),
    _regs(regs)
  { enable(); }

  void handle_irq()
  {
    l4_uint32_t eds = _regs[Eds];

    if (L4_UNLIKELY(!eds))
      {
        // this usually never happens
        enable();
        return;
      }

    l4_uint32_t reset = eds;
    l4_uint32_t pin_bit = 1, clear_hen = 0, clear_len = 0;

    // mask all out-of-bounds pins for IRQ delivery
    // however, we assume that this never happens
    if (L4_UNLIKELY(eds & ~_pins_mask))
      {
        d_printf(DBG_ERR, "GPIO-BCM2835: IRQ for invalid pin(s): 0x%x, "
                          "maybe you use an invalid hardware config\n",
                 eds & ~_pins_mask);
        eds &= _pins_mask;
      }

    for (unsigned pin = 0; eds; ++pin, pin_bit <<= 1, eds >>= 1)
      if ((eds & 1) && _pins[pin])
        {
          switch (_pins[pin]->mode())
            {
            case L4_IRQ_F_LEVEL_HIGH: clear_hen |= pin_bit; break;
            case L4_IRQ_F_LEVEL_LOW:  clear_len |= pin_bit; break;
            }

          _pins[pin]->trigger();
        }

    // do the mask for level triggered IRQs
    if (clear_hen)
      _regs[Hen].clear(clear_hen);
    if (clear_len)
      _regs[Len].clear(clear_len);
    _regs[Eds] = reset;

    enable();
  }
};

class Gpio_bcm2835_chip : public Hw::Gpio_device
{
private:
  /*
   * Registers with special layout
   */
  enum Regs
  {
    Fsel    =  0x0,
    Pud     = 0x94,
  };

  Hw::Register_block<32> _regs[2];
  Gpio_irq_server *_irq_svr[2];

  Int_property _nr_pins;

  static unsigned _func_reg(unsigned pin)
  {
    /* There is room for 10 pins in each function select register,
     * since each pin takes 3 bits */
    return Fsel + (pin / 10) * (sizeof (l4_uint32_t));
  }

  static unsigned _func_reg_shift(unsigned pin)
  {
    /* There is room for 10 pins in each function select register,
     * since each pin takes 3 bits */
    return (pin % 10) * 3;
  }

  static unsigned _register(unsigned reg, unsigned pin)
  {
    /* There is room for 32 pins in each register,
     * since each pin takes 1 bit */
    return reg + (pin / 32) * (sizeof (l4_uint32_t));
  }

  static l4_uint32_t _pin_bit(unsigned pin)
  { return 1 << (pin & 31); }

  static unsigned _pin_shift(unsigned pin)
  {
    /* There is room for 32 pins in each register,
     * since each pin takes 1 bit */
    return pin % 32;
  }

  unsigned _reg_offset_check(unsigned pin_offset) const
  {
    switch (pin_offset)
      {
      case 0:
        return 0;

      case 32:
        if (_nr_pins <= 32)
          throw -L4_ERANGE;
        return 4;

      default:
        throw -L4_EINVAL;
      }
  }

public:
  Gpio_bcm2835_chip() : _nr_pins(0)
  {
    add_cid("gpio");
    add_cid("gpio-bcm2835");

    register_property("pins", &_nr_pins);
  }

  unsigned nr_pins() const { return _nr_pins; }

  void request(unsigned) {}
  void free(unsigned) {}
  void setup(unsigned pin, unsigned mode, int value = 0);
  void config_pull(unsigned pin, unsigned mode);
  int get(unsigned pin);
  void set(unsigned pin, int value);
  void config_pad(unsigned pin, unsigned func, unsigned value);
  void config_get(unsigned pin, unsigned func, unsigned *value);
  Io_irq_pin *get_irq(unsigned pin);

  void multi_setup(Pin_slice const &mask, unsigned mode, unsigned outvalues = 0);
  void multi_config_pad(Pin_slice const &mask, unsigned func, unsigned value = 0);
  void multi_set(Pin_slice const &mask, unsigned data);
  unsigned multi_get(unsigned offset);

  void init();

private:
  enum
  {
    Func_in  = 0,
    Func_out = 1,
  };

  void config(unsigned pin, unsigned func)
  {
    if (func & ~0x7U)
      {
        d_printf(DBG_WARN, "Gpio_bcm2835_chip::config(func 0x%x): Wrong function\n",
                 func);
        throw -L4_EINVAL;
      }

    unsigned shift = _func_reg_shift(pin);
    _regs[0][_func_reg(pin)].modify(0x7U << shift, func << shift);
  }
};

int
Gpio_bcm2835_chip::get(unsigned pin)
{
  if (pin >= _nr_pins)
    throw -L4_EINVAL;

  l4_uint32_t val = _regs[0][_register(Lev, pin)];
  return (val >> _pin_shift(pin)) & 1;
}

unsigned
Gpio_bcm2835_chip::multi_get(unsigned offset)
{
  return _regs[0][Lev + _reg_offset_check(offset)];
}

void
Gpio_bcm2835_chip::set(unsigned pin, int value)
{
  if (pin >= _nr_pins)
    throw -L4_EINVAL;

  unsigned reg_set = value ? Set : Clr;
  _regs[0][_register(reg_set, pin)] = _pin_bit(pin);
}

void
Gpio_bcm2835_chip::multi_set(Pin_slice const &mask, unsigned data)
{
  unsigned roffs = _reg_offset_check(mask.offset);
  if (mask.mask & data)
    _regs[0][Set + roffs] = (mask.mask & data);
  if (mask.mask & ~data)
    _regs[0][Clr + roffs] = (mask.mask & ~data);
}

void
Gpio_bcm2835_chip::setup(unsigned pin, unsigned mode, int value)
{
  if (pin >= _nr_pins)
    throw -L4_EINVAL;

  switch (mode)
    {
    case Input:
      mode = Func_in;
      break;
    case Output:
      mode = Func_out;
      break;
    case Irq:
      // the BCM2835 GPIO does not have a dedicated Irq mode
      // just set the function to Input
      mode = Func_in;
      break;
    default:
      // although setup is part of the generic Gpio API we allow
      // hardware specific modes as well
      mode &= 0x7;
      break;
    }

  config(pin, mode);

  if (mode == Func_out)
    set(pin, value);
}

void
Gpio_bcm2835_chip::config_pull(unsigned pin, unsigned mode)
{
  if (pin >= _nr_pins)
    throw -L4_EINVAL;

  // NOTE: This function is not reentrant, uses the global
  //       PUD register.

  switch (mode)
    {
    case Pull_none:
      _regs[0][Pud] = 0;
      break;
    case Pull_up:
      _regs[0][Pud] = 0x2;
      break;
    case Pull_down:
      _regs[0][Pud] = 0x1;
      break;
    default:
      d_printf(DBG_WARN, "warning: %s: invalid PUD mode for pin %u. "
               "Ignoring.\n", name(), pin);
      throw -L4_EINVAL;
    }

  /* We should wait at least 150 cycles according to the manual */
  // FIXME: We usually MUST never sleep synchronously
  l4_usleep(10);
  _regs[0][_register(Pudclk, pin)] = _pin_bit(pin);
  /* We should wait at least 150 cycles according to the manual */
  // FIXME: We usually MUST never sleep synchronously
  l4_usleep(10);

  _regs[0][Pud] = 0;
  _regs[0][_register(Pudclk, pin)] = 0;
}

void
Gpio_bcm2835_chip::config_pad(unsigned pin, unsigned reg, unsigned value)
{
  d_printf(DBG_DEBUG2, "Gpio_bcm2835_chip::config_pad(%u, reg=%x, value=%x)\n",
           pin, reg, value);

  if (pin >= _nr_pins)
    throw -L4_EINVAL;

  switch (reg)
    {
      case Fsel:
        config(pin, value);
        break;
      case Set:
      case Clr:
      case Lev:
      case Eds:
      case Ren:
      case Fen:
      case Hen:
      case Len:
      case Aren:
      case Afen:
      case Pudclk:
        _regs[0][_register(reg, pin)].modify(_pin_bit(pin), value ? _pin_bit(pin) : 0);
        break;

      default:
      case Pud: // cannot allow the global PUD register here (would need locks)
        throw -L4_EINVAL;
    }
}

void
Gpio_bcm2835_chip::config_get(unsigned pin, unsigned reg, unsigned *value)
{
  d_printf(DBG_DEBUG2, "Gpio_bcm2835_chip::config_get(%u, reg=%x)\n",
           pin, reg);

  if (pin >= _nr_pins)
    throw -L4_EINVAL;

  switch (reg)
    {
      case Fsel:
        *value = (_regs[0][_func_reg(pin)] >> _func_reg_shift(pin)) & 0x7;
        return;
      case Set:
      case Clr:
      case Lev:
      case Eds:
      case Ren:
      case Fen:
      case Hen:
      case Len:
      case Aren:
      case Afen:
      case Pudclk:
        *value = (_regs[0][_register(reg, pin)] >>  _pin_shift(pin)) & 1;
        return;

      default:
      case Pud:
        // cannot allow access to the global PUD register,
        // it seems reading this register is useless anyway
        throw -L4_EINVAL;
    }
}

Io_irq_pin *
Gpio_bcm2835_chip::get_irq(unsigned pin)
{
  if (pin >= _nr_pins)
    throw -L4_EINVAL;

  unsigned svr = pin / 32;
  return _irq_svr[svr]->get_pin<Gpio_irq_pin>(pin % 32, _regs[svr]);
}

void
Gpio_bcm2835_chip::multi_config_pad(Pin_slice const &mask, unsigned reg, unsigned val)
{
  d_printf(DBG_DEBUG2, "Gpio_bcm2835_chip::multi_config_pad(%x, r=%x, v=%x)\n",
           mask.mask, reg, val);

  unsigned m = mask.mask;
  for (unsigned pin = mask.offset; pin < _nr_pins; ++pin, m >>= 1)
    if (m & 1)
      config_pad(pin, reg, val);
}

void
Gpio_bcm2835_chip::multi_setup(Pin_slice const &mask, unsigned mode, unsigned outval)
{
  unsigned m = mask.mask;
  for (unsigned pin = mask.offset; pin < _nr_pins; ++pin, m >>= 1, outval >>= 1)
    if (m & 1)
      setup(pin, mode, outval & 1);
}

void
Gpio_bcm2835_chip::init()
{
  Gpio_device::init();

  Resource *regs = resources()->find("regs");
  if (!regs || regs->type() != Resource::Mmio_res)
    {
      d_printf(DBG_ERR, "error: %s: no base address set for device: Gpio_bcm2835_chip\n"
                        "       missing or wrong 'regs' resource\n"
                        "       the chip will not work at all!\n", name());
      return;
    }

  l4_addr_t phys_base = regs->start();
  l4_addr_t size = regs->size();

  if (size < 0xb4 || size > (1 << 20))
    {
      d_printf(DBG_ERR, "error: %s: invalid mmio size (%lx) for device: Gpio_bcm2835_chip\n"
                        "       the chip will not work at all!\n", name(), size);
      return;
    }

  if (_nr_pins <= 0)
    {
      d_printf(DBG_ERR, "error: %s: Gpio_bcm2835_chip configured for 0 pins\n"
                        "       forgot to set 'pins' property in your config?\n",
               name());
      return;
    }

  l4_addr_t vbase = res_map_iomem(phys_base, size);
  if (!vbase)
    {
      d_printf(DBG_ERR, "error: %s: cannot map registers for Gpio_bcm2835_chip\n"
                        "       phys=%lx-%lx\n",
               name(), phys_base, phys_base + size - 1);
      return;
    }

  d_printf(DBG_DEBUG2, "%s: Gpio_bcm2835: mapped registers to %08lx\n",
           name(), vbase);

  _regs[0] = new Hw::Mmio_register_block<32>(vbase);
  _regs[1] = new Hw::Mmio_register_block<32>(vbase + 4);

  Resource *irq0 = resources()->find("int0");
  if (irq0 && irq0->type() == Resource::Irq_res)
    _irq_svr[0] = new Gpio_irq_server(irq0->start(),
                                      cxx::min<unsigned>(32, _nr_pins), _regs[0]);
  else
    d_printf(DBG_WARN, "warning: %s: Gpio_bcm2835 no 'int0' configured\n"
                       "         no IRQs for pins < 32\n", name());

  if (_nr_pins <= 32)
    return;

  Resource *irq2 = resources()->find("int2");
  if (irq2 && irq2->type() == Resource::Irq_res)
    _irq_svr[1] = new Gpio_irq_server(irq2->start(),
                                      cxx::min<unsigned>(32, _nr_pins - 32), _regs[1]);
  else
    d_printf(DBG_WARN, "warning: %s: Gpio_bcm2835 no 'int1' configured\n"
                       "         no IRQs for pins 32-%u\n",
             name(), (unsigned)_nr_pins - 1);

}

static Hw::Device_factory_t<Gpio_bcm2835_chip> __hw_pf_factory("Gpio_bcm2835_chip");

}
