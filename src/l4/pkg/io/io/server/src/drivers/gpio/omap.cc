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

struct Omap_gpio_base
{
  enum : unsigned
  {
    Revision  = 0x000,  // GPIO_REVISION
    Sysconfig = 0x010   // GPIO_SYSCONFIG
  };

  enum Pull_selection : unsigned
  {
    None    = 0,
    Down    = 0,
    Enable  = 1,
    Up      = 2,
  };
};

struct Omap3_gpio : Omap_gpio_base
{
  // Multiplexing modes, Omap3 TRM, table 7-2
  enum Pad_mode : unsigned
  {
    Gpio = 4,
    Safe = 7
  };

  // Gpio chip register offsets, Omap3 TRM, table 24-7
  // register name written as comment behind enum member definition
  enum : unsigned
  {
    Irq_status        = 0x018,  // GPIO_IRQSTATUS1
    Irq_enable        = 0x01c,  // GPIO_IRQENABLE1
    Wkup_enable       = 0x020,  // GPIO_WAKEUPENABLE
    Ctrl              = 0x030,  // GPIO_CTRL
    Direction         = 0x034,  // GPIO_OE
    Data_in           = 0x038,  // GPIO_DATAIN
    Data_out          = 0x03c,  // GPIO_DATAOUT
    Level_detect_low  = 0x040,  // GPIO_LEVELDETECT0
    Level_detect_high = 0x044,  // GPIO_LEVELDETECT1
    Rising_detect     = 0x048,  // GPIO_RISINGDETECT
    Falling_detect    = 0x04c,  // GPIO_FALLINGDETECT
    Debounce_enable   = 0x050,  // GPIO_DEBOUNCENABLE
    Debounce_time     = 0x054,  // GPIO_DEBOUNCINGTIME
    Clr_irq_enable    = 0x060,  // GPIO_CLEARIRQENABLE1
    Set_irq_enable    = 0x064,  // GPIO_SETIRQENABLE1
    Clr_data_out      = 0x090,  // GPIO_CLRDATAOUT
    Set_data_out      = 0x094,  // GPIO_SETDATAOUT
  };
};

struct Omap4_gpio : Omap_gpio_base
{
  // Multiplexing modes, Omap4 TRM, table 18-6
  enum Pad_mode : unsigned
  {
    Gpio = 3,
    Safe = 7
  };

  // Gpio chip register offsets, Omap4 TRM, table 25-18
  // register name written as comment behind enum member definition
  enum : unsigned
  {
    Irq_status        = 0x02c,  // GPIO_IRQSTATUS_0
    Irq_enable        = 0x034,  // GPIO_IRQSTATUS_SET_0
    Wkup_enable       = 0x120,  // GPIO_WAKEUPENABLE
    Ctrl              = 0x130,  // GPIO_CTRL
    Direction         = 0x134,  // GPIO_OE
    Data_in           = 0x138,  // GPIO_DATAIN
    Data_out          = 0x13c,  // GPIO_DATAOUT
    Level_detect_low  = 0x140,  // GPIO_LEVELDETECT0
    Level_detect_high = 0x144,  // GPIO_LEVELDETECT1
    Rising_detect     = 0x148,  // GPIO_RISINGDETECT
    Falling_detect    = 0x14c,  // GPIO_FALLINGDETECT
    Debounce_enable   = 0x150,  // GPIO_DEBOUNCENABLE
    Debounce_time     = 0x154,  // GPIO_DEBOUNCINGTIME
    Clr_irq_enable    = 0x03c,  // GPIO_IRQSTATUS_CLR_0
    Set_irq_enable    = 0x034,  // GPIO_IRQSTATUS_SET_0
    Clr_data_out      = 0x190,  // GPIO_CLRDATAOUT
    Set_data_out      = 0x194,  // GPIO_SETDATAOUT
  };
};

struct Omap5_gpio : Omap_gpio_base
{
  // Multiplexing modes, Omap5 TRM, table 18-6
  enum Pad_mode : unsigned
  {
    Gpio = 6,
    Safe = 7
  };

  // Gpio chip register offsets, Omap5 TRM, table 25-18
  // register name written as comment behind enum member definition
  enum : unsigned
  {
    Irq_status        = 0x02c,  // GPIO_IRQSTATUS_0
    Irq_enable        = 0x034,  // GPIO_IRQSTATUS_SET_0
    Wkup_enable       = 0x120,  // GPIO_WAKEUPENABLE
    Ctrl              = 0x130,  // GPIO_CTRL
    Direction         = 0x134,  // GPIO_OE
    Data_in           = 0x138,  // GPIO_DATAIN
    Data_out          = 0x13c,  // GPIO_DATAOUT
    Level_detect_low  = 0x140,  // GPIO_LEVELDETECT0
    Level_detect_high = 0x144,  // GPIO_LEVELDETECT1
    Rising_detect     = 0x148,  // GPIO_RISINGDETECT
    Falling_detect    = 0x14c,  // GPIO_FALLINGDETECT
    Debounce_enable   = 0x150,  // GPIO_DEBOUNCENABLE
    Debounce_time     = 0x154,  // GPIO_DEBOUNCINGTIME
    Clr_irq_enable    = 0x03c,  // GPIO_IRQSTATUS_CLR_0
    Set_irq_enable    = 0x034,  // GPIO_IRQSTATUS_SET_0
    Clr_data_out      = 0x190,  // GPIO_CLRDATAOUT
    Set_data_out      = 0x194,  // GPIO_SETDATAOUT
  };
};

class Scm_omap : public Hw::Device
{
  Hw::Register_block<16> _regs;

public:
  Scm_omap() { add_cid("scm-omap"); }

  void init()
  {
    Hw::Device::init();

    Resource *regs = resources()->find("regs");
    if (!regs || regs->type() != Resource::Mmio_res)
      {
        d_printf(DBG_ERR, "error: %s: no base address set for device: Scm_omap\n"
                          "       missing or wrong 'regs' resource\n"
                          "       the SCM will not work at all!\n", name());
        return;
      }

    l4_addr_t phys_base = regs->start();
    l4_addr_t size = regs->size();

    if (size < 0x100 || size > (1 << 12))
      {
        d_printf(DBG_ERR, "error: %s: invalid mmio size (%lx) for device: Scm_omap\n"
                          "       the chip will not work at all!\n", name(), size);
        return;
      }

    l4_addr_t vbase = res_map_iomem(phys_base, size);
    if (!vbase)
      {
        d_printf(DBG_ERR, "error: %s: cannot map registers for Scm_omap\n"
                          "       phys=%lx-%lx",
                 name(), phys_base, phys_base + size - 1);
        return;
      }

    d_printf(DBG_DEBUG2, "%s: Scm_omap: mapped registers to %08lx\n",
             name(), vbase);

    _regs = new Hw::Mmio_register_block<16>(vbase);
  }

  void set_mode(l4_int32_t offset, unsigned mode)
  {
    if (offset < 0)
      throw -L4_EINVAL;

    _regs[offset].modify(0x7, mode & 0x7);
  }

  void set_pull(l4_int32_t offset, unsigned value)
  {
    if (offset < 0)
      throw -L4_EINVAL;

    // bits 3 (enable) and 4 (type) are for pull mode
    // also enable bidirectional mode in bit 8
    _regs[offset].modify(0x118, ((value & 0x3) << 3) | 0x100);
  }
};

static Hw::Device_factory_t<Scm_omap> __hw_scm_factory("Scm_omap");

class Scm_property : public Property
{
private:
  enum : int { Num_offsets = 32 };

  Scm_omap *_scm;
  l4_int32_t _offsets[Num_offsets];

public:
  Scm_property()
  {
    // fill offsets with -1 to be save if user supplies less than 32 values
    std::fill_n(_offsets, Num_offsets, -1);
  }

  int set(int, std::string const &) { return -EINVAL; }

  int set(int k, l4_int64_t i)
  {
    // check for correct index, lua tables start with index 1
    // and the first entry is defined to be the device reference
    // we expect 32 values
    if (k > (Num_offsets + 1) || k < 2)
      return -EINVAL;

    // assume the mmio size of the Scm to be <= 0x1000
    if (i >= 0x1000)
      return -EINVAL;

    // do not forget to reduce the index, it is offset by 2
    _offsets[k - 2] = i;
    return 0;
  }

  /* the device reference has to be the first entry in the table
   * note: lua tables start at index 1 */
  int set(int k, Generic_device *d)
  {
    if (k != 1)
      return -EINVAL;

    _scm = dynamic_cast<Scm_omap *>(d);
    if (!_scm)
      return -EINVAL;

    return 0;
  }

  int set(int, Resource *) { return -EINVAL; }

  Scm_omap *dev() { return _scm; }
  l4_int32_t offset(int index)
  {
    if (index < 0 || index >= Num_offsets)
      return -ERANGE;

    return _offsets[index];
  }
};

template<typename IMPL>
class Omap_irq_base : public Gpio_irq_base_t<IMPL>
{
protected:
  Hw::Register_block<32> _regs;

  Omap_irq_base(unsigned pin, Hw::Register_block<32> const &regs)
  : Gpio_irq_base_t<IMPL>(pin), _regs(regs)
  {}

  void write_reg_pin(unsigned reg, int value)
  {
    if (value & 1)
      _regs[reg].set(1 << this->pin());
    else
      _regs[reg].clear(1 << this->pin());
  }
};

template<class REGS>
class Gpio_irq_pin_t : public Omap_irq_base<Gpio_irq_pin_t<REGS>>
{
public:
  Gpio_irq_pin_t(unsigned pin, Hw::Register_block<32> const &regs)
  : Omap_irq_base<Gpio_irq_pin_t<REGS>>(pin, regs)
  {}

  void do_mask()
  {
    this->_regs[REGS::Clr_irq_enable] = 1 << this->pin();
  }

  void do_unmask()
  {
    this->_regs[REGS::Set_irq_enable] = 1 << this->pin();
  }

  bool do_set_mode(unsigned mode)
  {
    int values[4] = {0, 0, 0, 0};

    switch(mode)
      {
      case L4_IRQ_F_LEVEL_HIGH:
        values[3] = 1;
        break;
      case L4_IRQ_F_LEVEL_LOW:
        values[2] = 1;
        break;
      case L4_IRQ_F_POS_EDGE:
        values[0] = 1;
        break;
      case L4_IRQ_F_NEG_EDGE:
        values[1] = 1;
        break;
      case L4_IRQ_F_BOTH_EDGE:
        values[0] = 1;
        values[1] = 1;
        break;

      default:
        return false;
      }

    // this operation touches multiple mmio registers and is thus
    // not atomic, that's why we first mask the IRQ and if it was
    // enabled we unmask it after we have changed the mode
    if (this->enabled())
      do_mask();

    this->_regs[REGS::Direction].set(1 << this->pin());

    this->write_reg_pin(REGS::Rising_detect, values[0]);
    this->write_reg_pin(REGS::Falling_detect, values[1]);
    this->write_reg_pin(REGS::Level_detect_low, values[2]);
    this->write_reg_pin(REGS::Level_detect_high, values[3]);

    if (this->enabled())
      do_unmask();

    return true;
  }

  int clear() override
  {
    l4_uint32_t status = this->_regs[REGS::Irq_status] & (1UL << this->pin());
    if (status)
      this->_regs[REGS::Irq_status] = status;

    return Io_irq_pin::clear() + (status >> this->pin());
  }

};

template<class REGS>
class Gpio_irq_server_t : public Irq_demux_t<Gpio_irq_server_t<REGS>>
{
  friend class Gpio_irq_pin_t<REGS>;
  typedef Gpio_irq_pin_t<REGS> Gpio_irq_pin;
  typedef Irq_demux_t<Gpio_irq_server_t<REGS>> Base;

  Hw::Register_block<32> _regs;

public:
  Gpio_irq_server_t(int irq, unsigned pins, Hw::Register_block<32> const &regs)
  : Base(irq, 0, pins), _regs(regs)
  {
    this->enable();
  }

  void handle_irq()
  {
    // I think it is sufficient to read irqstatus as we only use the first
    // hw irq per chip
    unsigned status = _regs[REGS::Irq_status];
    unsigned reset = status;

    if (!status) // usually never happens
      {
        this->enable();
        return;
      }

    l4_uint32_t mask_irqs = 0, pin_bit = 1;
    for (unsigned pin = 0; status; ++pin, status >>= 1, pin_bit <<= 1)
      {
        if (!(status & pin))
          continue;

        Gpio_irq_base *p = this->_pins[pin];
        if (p)
          {
            switch (p->mode())
              {
              case L4_IRQ_F_LEVEL_HIGH: mask_irqs |= pin_bit; break;
              case L4_IRQ_F_LEVEL_LOW: mask_irqs |= pin_bit; break;
              }
            p->trigger();
          }
        else
          // this is strange as this would mean an unassigned IRQ is unmasked
          mask_irqs |= pin_bit;
      }

    // do the mask for level triggered IRQs
    if (mask_irqs)
      _regs[REGS::Clr_irq_enable] = mask_irqs;

    _regs[REGS::Irq_status] = reset;
    this->enable();
  }

};

template<class REGS>
class Gpio_omap_chip : public Hw::Gpio_device
{
private:
  typedef Gpio_irq_server_t<REGS> Gpio_irq_server;
  Hw::Register_block<32> _regs;
  unsigned _nr_pins;
  Gpio_irq_server *_irq_svr;
  Scm_property _scm;

  static l4_uint32_t _pin_bit(unsigned pin)
  { return 1 << (pin & 31); }

  static unsigned _pin_shift(unsigned pin)
  { return pin % 32; }

  unsigned _reg_offset_check(unsigned pin_offset) const
  {
    switch (pin_offset)
      {
      case 0:
        return 0;

      default:
        throw -L4_EINVAL;
      }
  }

  void config(unsigned pin, unsigned func)
  {
    if (_scm.dev())
      _scm.dev()->set_mode(_scm.offset(pin), func);
  }

public:
  Gpio_omap_chip() : _nr_pins(32), _irq_svr(0)
  {
    add_cid("gpio");
    add_cid("gpio-omap");

    register_property("scm", &_scm);
  }

  unsigned nr_pins() const { return _nr_pins; }

  void request(unsigned) {}
  void free(unsigned) {}

  int get(unsigned pin)
  {
    if (pin >= _nr_pins)
      throw -L4_EINVAL;

    return (_regs[REGS::Data_out] >> _pin_shift(pin)) & 1;
  }

  void set(unsigned pin, int value)
  {
    if (pin >= _nr_pins)
      throw -L4_EINVAL;

    unsigned reg_set = value ? REGS::Set_data_out : REGS::Clr_data_out;
    _regs[reg_set] = _pin_bit(pin);
  }

  unsigned multi_get(unsigned offset)
  {
    _reg_offset_check(offset);
    return _regs[REGS::Data_out];
  }

  void multi_set(Pin_slice const &mask, unsigned data)
  {
    _reg_offset_check(mask.offset);
    if (mask.mask & data)
      _regs[REGS::Set_data_out] = (mask.mask & data);
    if (mask.mask & ~data)
      _regs[REGS::Clr_data_out] = (mask.mask & ~data);
  }

  void setup(unsigned pin, unsigned mode, int value)
  {
    if (pin >= _nr_pins)
      throw -L4_EINVAL;

    switch (mode)
      {
    case Input:
      _regs[REGS::Direction].set(_pin_bit(pin));
      _scm.dev()->set_mode(_scm.offset(pin), REGS::Pad_mode::Gpio);
      return;
    case Output:
      _regs[REGS::Direction].clear(_pin_bit(pin));
      _scm.dev()->set_mode(_scm.offset(pin), REGS::Pad_mode::Gpio);
      set(pin, value);
      return;
    case Irq:
      d_printf(DBG_WARN, "warning: Gpio_omap_chip: trying to setup pin as Irq\n"
               "         This mode is not supported. Setting mode to input\n"
               "         Use to_irq() to configure Irq\n");
      _regs[REGS::Direction].set(_pin_bit(pin));
      return;
    default:
      // although setup is part of the generic Gpio API we allow
      // hardware specific modes as well
      mode &= 0x7;
      break;
      }

    config(pin, mode);
  }

  void config_pad(unsigned pin, unsigned reg, unsigned value)
  {
    if (pin >= _nr_pins)
      throw -L4_EINVAL;

    switch (reg)
      {
    case REGS::Irq_status:       // hmm, allow user to reset the irq status?
    case REGS::Irq_enable:       // hmm, allow user to enable irq this way?
    case REGS::Wkup_enable:
    case REGS::Direction:
    case REGS::Data_out:
    case REGS::Level_detect_low:  // hmm, allow user to configure IRQ this way?
    case REGS::Level_detect_high: // hmm, allow user to configure IRQ this way?
    case REGS::Rising_detect:     // hmm, allow user to configure IRQ this way?
    case REGS::Falling_detect:    // hmm, allow user to configure IRQ this way?
    case REGS::Debounce_enable:
    case REGS::Clr_irq_enable:   // hmm, allow user to disable IRQ this way?
    case REGS::Clr_data_out:
    case REGS::Set_data_out:
      _regs[reg].modify(_pin_bit(pin), value ? _pin_bit(pin) : 0);
      break;

    default:
      // cannot allow the following registers, they have security implications
      // Sysconfig, Ctrl, Debounce_time
      // the following registers are read-only
      // Revision, Sysstatus (also security), Data_in
      throw -L4_EINVAL;
      }
  }

  void config_get(unsigned pin, unsigned reg, unsigned *value)
  {
    if (pin >= _nr_pins)
      throw -L4_EINVAL;

    switch (reg)
      {
    case REGS::Revision:
      *value = _regs[REGS::Revision] & 0xff;
      break;
    case REGS::Sysconfig:
      *value = _regs[REGS::Sysconfig] & 0x1f;
      break;
    case REGS::Ctrl:
      *value = _regs[REGS::Ctrl] & 0x7;
      break;
    case REGS::Debounce_time:
      *value = _regs[REGS::Debounce_time] & 0xff;
      break;
    case REGS::Irq_status:
    case REGS::Irq_enable:
    case REGS::Wkup_enable:
    case REGS::Direction:
    case REGS::Data_in:
    case REGS::Data_out:
    case REGS::Level_detect_low:
    case REGS::Level_detect_high:
    case REGS::Rising_detect:
    case REGS::Falling_detect:
    case REGS::Debounce_enable:
    case REGS::Clr_irq_enable:
    case REGS::Clr_data_out:
    case REGS::Set_data_out:
      *value = (_regs[reg] >> _pin_shift(pin)) & 1;
      break;

    default:
      throw -L4_EINVAL;
      }
  }

  void config_pull(unsigned pin, unsigned mode)
  {
    if (pin >= _nr_pins)
      throw -L4_EINVAL;

    switch (mode)
      {
    case Pull_none:
      mode = REGS::Pull_selection::None;
      break;
    case Pull_up:
      mode = REGS::Pull_selection::Up | REGS::Pull_selection::Enable;
      break;
    case Pull_down:
      mode = REGS::Pull_selection::Down | REGS::Pull_selection::Enable;
      break;
    default:
      d_printf(DBG_WARN, "warning: %s: invalid PUD mode for pin %u. "
                         "Ignoring.\n", name(), pin);
      throw -L4_EINVAL;
      }

    if (_scm.dev())
      _scm.dev()->set_pull(_scm.offset(pin), mode);
  }

  Io_irq_pin *get_irq(unsigned pin)
  {
    if (pin >= _nr_pins)
      throw -L4_EINVAL;

    return _irq_svr->template get_pin<Gpio_irq_pin_t<REGS>>(pin, _regs);
  }

  void multi_config_pad(Pin_slice const &mask, unsigned func, unsigned value)
  {
    unsigned m = mask.mask;
    for (unsigned pin = mask.offset; pin < _nr_pins; ++pin, m >>= 1)
      if (m & 1)
        config_pad(pin, func, value);
  }

  void multi_setup(Pin_slice const &mask, unsigned mode, unsigned outvalues)
  {
    unsigned m = mask.mask;
    for (unsigned pin = mask.offset; pin < _nr_pins; ++pin, m >>= 1, outvalues >>= 1)
      if (m & 1)
        setup(pin, mode, outvalues & 1);
  }

  void init()
  {
    Gpio_device::init();

    Resource *regs = resources()->find("regs");
    if (!regs || regs->type() != Resource::Mmio_res)
      {
        d_printf(DBG_ERR, "error: %s: no base address set for device: Gpio_omap_chip\n"
                          "       missing or wrong 'regs' resource\n"
                          "       the chip will not work at all!\n", name());
        return;
      }

    l4_addr_t phys_base = regs->start();
    l4_addr_t size = regs->size();

    if (size < 0x100 || size > (1 << 12))
      {
        d_printf(DBG_ERR, "error: %s: invalid mmio size (%lx) for device: Gpio_omap_chip\n"
                          "       the chip will not work at all!\n",
                 name(), size);
        return;
      }

    l4_addr_t vbase = res_map_iomem(phys_base, size);
    if (!vbase)
      {
        d_printf(DBG_ERR, "error: %s: cannot map registers for Gpio_omap_chip\n"
                          "       phys=%lx-%lx",
                 name(), phys_base, phys_base + size - 1);
        return;
      }

    d_printf(DBG_DEBUG2, "%s: Gpio_omap_chip: mapped registers to %08lx\n",
             name(), vbase);

    _regs = new Hw::Mmio_register_block<32>(vbase);

    Resource *irq = resources()->find("irq");
    if (irq && irq->type() == Resource::Irq_res)
      _irq_svr = new Gpio_irq_server(irq->start(), _nr_pins, _regs);
    else
      d_printf(DBG_WARN, "warning: %s: Gpio_omap_chip no irq configured\n",
               name());

    if (!_scm.dev())
      d_printf(DBG_WARN, "warning: %s: no Scm for device: Gpio_omap_chip\n"
                         "         'scm' property in device tree not set?\n"
                         "         Setting pin and PUD modes will be disabled\n",
               name());
  }
};


static Hw::Device_factory_t<Gpio_omap_chip<Omap3_gpio>>
       __hw_gpio_omap35x_factory("Gpio_omap35x_chip");
static Hw::Device_factory_t<Gpio_omap_chip<Omap4_gpio>>
       __hw_gpio_omap44x_factory("Gpio_omap44x_chip");
static Hw::Device_factory_t<Gpio_omap_chip<Omap5_gpio>>
       __hw_gpio_omap54x_factory("Gpio_omap54x_chip");
}
