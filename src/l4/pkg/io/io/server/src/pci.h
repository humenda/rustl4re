/*
 * (c) 2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include "hw_device.h"
#include <l4/sys/cxx/types>
#include <l4/cxx/bitfield>
#include <l4/cxx/type_traits>
#include "irqs.h"

#include <pthread.h>
#include <cassert>


namespace Hw { namespace Pci {

enum Cfg_width
{
  Cfg_long  = 2,
  Cfg_short = 1,
  Cfg_byte  = 0,
};

inline Cfg_width cfg_w_to_o(int width)
{
  switch (width)
    {
    default:
    case 32: return Cfg_long;
    case 16: return Cfg_short;
    case 8:  return Cfg_byte;
    }
}

template<unsigned S> struct _cfg_width;
template<> struct _cfg_width<1> { static const Cfg_width width = Cfg_byte; };
template<> struct _cfg_width<2> { static const Cfg_width width = Cfg_short; };
template<> struct _cfg_width<4> { static const Cfg_width width = Cfg_long; };
template<typename T> struct cfg_width { static const Cfg_width width = _cfg_width<sizeof(T)>::width; };

template< Cfg_width w > struct Cfg_type;
template<> struct Cfg_type<Cfg_byte>  { typedef l4_uint8_t Type; };
template<> struct Cfg_type<Cfg_short> { typedef l4_uint16_t Type; };
template<> struct Cfg_type<Cfg_long>  { typedef l4_uint32_t Type; };

class Cfg_addr
{
public:

  // we store the config space address as specified in the PCI Express spec
  Cfg_addr(unsigned char bus, unsigned char dev, unsigned char fn, unsigned reg)
  : _a(  ((l4_uint32_t)bus << 20)
       | ((l4_uint32_t)dev << 15)
       | ((l4_uint32_t)fn  << 12)
       | ((l4_uint32_t)reg))
  {}

  Cfg_addr(Cfg_addr const &dev, unsigned reg)
  : _a((dev._a & ~0xfff) | (reg & 0xfff))
  {}

  l4_uint32_t to_compat_addr() const
  { return (_a & 0xff) | ((_a >> 4) & 0xffff00); }

  l4_uint32_t reg_offs(Cfg_width w = Cfg_byte) const
  { return (_a & 3) & (~0UL << (unsigned long)w); }

  unsigned bus() const { return (_a >> 20) & 0xff; }
  unsigned dev() const { return (_a >> 15) & 0x1f; }
  unsigned fn() const { return (_a >> 12) & 0x7; }
  unsigned devfn() const { return (_a >> 12) & 0xff; }
  unsigned reg() const { return _a & 0xfff; }

  Cfg_addr operator + (unsigned reg_offs) const
  { return Cfg_addr(_a + reg_offs); }

  l4_uint32_t addr() const { return _a; }

private:
  explicit Cfg_addr(l4_uint32_t raw) : _a(raw) {}

  l4_uint32_t _a;
};

class Bus : public virtual Hw::Dev_feature
{
public:
  unsigned char num;
  unsigned char subordinate;
  enum Bus_type : unsigned char { Pci_bus, Pci_express_bus };
  Bus_type bus_type = Pci_bus;
  Resource *irq_router = 0;

  explicit Bus(unsigned char bus, Bus_type bus_type)
  : num(bus), subordinate(bus), bus_type(bus_type), irq_router(0)
  {}

  virtual int cfg_read(Cfg_addr addr, l4_uint32_t *value, Cfg_width) = 0;
  virtual int cfg_write(Cfg_addr addr, l4_uint32_t value, Cfg_width) = 0;

  int cfg_read(unsigned bus, l4_uint32_t devfn, l4_uint32_t reg, l4_uint32_t *value, Cfg_width w)
  { return cfg_read(Cfg_addr(bus, devfn >> 16, devfn & 0xffff, reg), value, w); }

  int cfg_write(unsigned bus, l4_uint32_t devfn, l4_uint32_t reg, l4_uint32_t value, Cfg_width w)
  { return cfg_write(Cfg_addr(bus, devfn >> 16, devfn & 0xffff, reg), value, w); }

  bool discover_bus(Hw::Device *host);
  void dump(int) const;

  virtual void increase_subordinate(int s) = 0;

  virtual ~Bus() {}
};


class If : public virtual Dev_feature
{
public:

  virtual Resource *bar(int) const = 0;
  virtual Resource *rom() const = 0;
  virtual bool is_64bit_high_bar(int) const = 0;
  virtual bool supports_msi() const = 0;
  virtual bool is_bridge() const = 0;

  virtual int cfg_read(l4_uint32_t reg, l4_uint32_t *value, Cfg_width) = 0;
  virtual int cfg_write(l4_uint32_t reg, l4_uint32_t value, Cfg_width) = 0;
  template<typename T>
  int cfg_read(l4_uint32_t reg, T *val)
  {
    union
    {
      l4_uint32_t v;
      T t;
    } x;

    int r = cfg_read(reg, &x.v, cfg_width<T>::width);
    *val = x.t;
    return r;
  }

  template<typename T>
  int cfg_write(l4_uint32_t reg, T const &val)
  {
    union
    {
      l4_uint32_t v;
      T t;
    } x;

    x.t = val;
    return cfg_write(reg, x.v, cfg_width<T>::width);
  }

  template<typename T> T cfg_read(unsigned reg) { T v; cfg_read(reg, &v); return v; }

  virtual l4_uint32_t vendor_device_ids() const = 0;
  virtual l4_uint32_t class_rev() const = 0;
  virtual l4_uint32_t subsys_vendor_ids() const = 0;
  virtual l4_uint32_t recheck_bars(unsigned decoders) = 0;
  virtual l4_uint32_t checked_cmd_read() = 0;
  virtual l4_uint16_t checked_cmd_write(l4_uint16_t mask, l4_uint16_t cmd) = 0;
  virtual bool enable_rom() = 0;

  virtual unsigned bus_nr() const = 0;
  virtual unsigned devfn() const = 0;
  unsigned device_nr() const { return devfn() >> 3; }
  unsigned function_nr() const { return devfn() & 7; }
  virtual unsigned phantomfn_bits() const = 0;
  virtual Bus *bus() const = 0;
  virtual Io_irq_pin::Msi_src *get_msi_src() = 0;

  virtual ~If() = 0;
};

inline If::~If() {}

struct Pm_cap
{
  struct Pmc
  {
    l4_uint16_t raw;
    CXX_BITFIELD_MEMBER( 0,  2, version, raw);
    CXX_BITFIELD_MEMBER( 3,  3, pme_clock, raw);
    CXX_BITFIELD_MEMBER( 5,  5, dsi, raw);
    CXX_BITFIELD_MEMBER( 6,  8, aux_current, raw);
    CXX_BITFIELD_MEMBER( 9,  9, d1, raw);
    CXX_BITFIELD_MEMBER(10, 10, d2, raw);
    CXX_BITFIELD_MEMBER(11, 15, pme, raw);
    CXX_BITFIELD_MEMBER(11, 11, pme_d0, raw);
    CXX_BITFIELD_MEMBER(12, 12, pme_d1, raw);
    CXX_BITFIELD_MEMBER(13, 13, pme_d2, raw);
    CXX_BITFIELD_MEMBER(14, 14, pme_d3hot, raw);
    CXX_BITFIELD_MEMBER(15, 15, pme_d3cold, raw);
  };

  struct Pmcsr
  {
    l4_uint16_t raw;
    CXX_BITFIELD_MEMBER( 0,  2, state, raw);
    CXX_BITFIELD_MEMBER( 3,  3, no_soft_reset, raw);
    CXX_BITFIELD_MEMBER( 8,  8, pme_enable, raw);
    CXX_BITFIELD_MEMBER( 9, 12, data_sel, raw);
    CXX_BITFIELD_MEMBER(13, 14, data_scale, raw);
    CXX_BITFIELD_MEMBER(15, 15, pme_status, raw);
  };

  static l4_uint32_t pmcsr_reg(l4_uint32_t cap)
  { return cap + 4; }

  static l4_uint32_t pmc_reg(l4_uint32_t cap)
  { return cap + 2; }
};


/**
 * \brief Encapsulate the config space of a PCI device.
 */
class Config
{
public:
  enum Regs
  {
    /* Generic config */
    Vendor         = 0x00,
    Device         = 0x02,
    Command        = 0x04,
    Status         = 0x06,
    Class_rev      = 0x08,
    Cacheline_size = 0x0c,
    Latency_timer  = 0x0d,
    Header_type    = 0x0e,
    BIST           = 0x0f,

    /* Header type 0 config, normal PCI devices */
    Bar_0          = 0x10,
    Cardbus_cis    = 0x28,
    Subsys_vendor  = 0x2c,
    Subsys         = 0x2e,
    Rom_address    = 0x30,
    Capability_ptr = 0x34,
    Irq_line       = 0x3c,
    Irq_pin        = 0x3d,
    Min_gnt        = 0x3e,
    Max_lat        = 0x3f,

    /* Header type 1, PCI-PCI bridges */
    Primary           = 0x18,
    Secondary         = 0x19,
    Subordinate       = 0x1a,
    Secondary_latency = 0x1b,
    Io_base           = 0x1c,
    Io_limit          = 0x1d,
    Secondary_status  = 0x1e,
    Mem_base          = 0x20,
    Mem_limit         = 0x22,
    Pref_mem_base     = 0x24,
    Pref_mem_limit    = 0x26,
    Pref_mem_base_hi  = 0x28,
    Pref_mem_limit_hi = 0x2c,
    Io_base_hi        = 0x30,
    Io_limit_hi       = 0x32,
    Rom_address_1     = 0x38,
    Bridge_control    = 0x3e,

    /* header type 2, cardbus bridge */
    Cb_capability_ptr   = 0x14,
    Cb_secondary_status = 0x16,
    Cb_primary          = 0x18,
    Cb_cardbus          = 0x19,
    Cb_subordinate      = 0x1a,
    Cb_latency_timer    = 0x1b,
    Cb_mem_base_0       = 0x1c,
    Cb_mem_limit_0      = 0x20,
    Cb_mem_base_1       = 0x24,
    Cb_mem_limit_1      = 0x28,
    Cb_io_base_0        = 0x2c,
    Cb_io_base_0_hi     = 0x2e,
    Cb_io_limit_0       = 0x30,
    Cb_io_limit_0_hi    = 0x32,
    Cb_io_base_1        = 0x34,
    Cb_io_base_1_hi     = 0x36,
    Cb_io_limit_1       = 0x38,
    Cb_io_limi_1_hi     = 0x3a,
    Cb_bridge_control   = 0x3e,
    Cb_subsystem_vendor = 0x40,
    Cb_subsystem        = 0x42,
    Cb_legacy_mode_base = 0x44,
  };

  Config(Cfg_addr addr, Bus *bus) : _addr(addr), _bus(bus) {}
  Config() : _addr(0, 0, 0, 0), _bus(0) {}

  bool is_valid() const { return _bus; }

  int read(unsigned reg, l4_uint32_t *value, Cfg_width w)
  { return _bus->cfg_read(_addr + reg, value, w); }

  int write(unsigned reg, l4_uint32_t value, Cfg_width w)
  { return _bus->cfg_write(_addr + reg, value, w); }

  template<typename T>
  int read(unsigned reg, T *val)
  {
    union
    {
      l4_uint32_t v;
      T t;
    } x;

    int r = read(reg, &x.v, cfg_width<T>::width);
    *val = x.t;
    return r;
  }

  template<typename T> T read(unsigned reg) { T v; read(reg, &v); return v; }

  template<typename T>
  int write(unsigned reg, T const &val)
  {
    union
    {
      l4_uint32_t v;
      T t;
    } x;

    x.t = val;
    return write(reg, x.v, cfg_width<T>::width);
  }

  Config operator + (unsigned offset) const
  { return Config(_addr + offset, _bus); }

  Bus *bus() const { return _bus; }
  Cfg_addr addr() const { return _addr; }

private:
  Cfg_addr _addr;
  Bus *_bus;
};

class Cap : public Config
{
public:
  enum Types
  {
    Pm    = 0x01,
    Msi   = 0x05,
    Pcix  = 0x07,
    Vndr  = 0x09,
    Pcie  = 0x10,
    Msi_x = 0x11
  };

  l4_uint8_t id() { l4_uint8_t r; read(0, &r); return r; }
  Cap next()
  {
    l4_uint8_t r;
    read(1, &r);
    if (r)
      return Cap(Cfg_addr(addr(), r), bus());
    return Cap();
  }

  Cap() = default;
  Cap(Cfg_addr addr, Bus *bus)
  : Config(addr, bus)
  {}
};


class Saved_cap : public cxx::H_list_item_t<Saved_cap>
{
public:
  Saved_cap(l4_uint8_t type, unsigned pos) : _type(type), _reg(pos) {}
  virtual ~Saved_cap() = 0;
  void save(Config cfg) { _save(cfg + _reg); }
  void restore(Config cfg) { _restore(cfg + _reg); }

  l4_uint8_t type() const { return _type; }
  unsigned cap_offset() const { return _reg; }

private:
  l4_uint8_t _type;
  unsigned   _reg;

  virtual void _save(Config cfg) = 0;
  virtual void _restore(Config cfg) = 0;

};

class Saved_config
{
public:
  template<typename T>
  T read(unsigned reg)
  {
    switch (cfg_width<T>::width)
      {
      case Cfg_byte:  return _regs.b[reg];
      case Cfg_short: return _regs.h[reg >> 1];
      case Cfg_long:  return _regs.w[reg >> 2];
      }
  }

  void save(Config cfg);
  void restore(Config cfg);

  Saved_cap *find_cap(l4_uint8_t type);
  void add_cap(Saved_cap *cap)
  { _caps.push_front(cap); }

private:
  typedef cxx::H_list_t<Saved_cap> Cap_list;
  union
  {
    l4_uint32_t w[16];
    l4_uint16_t h[16*2];
    l4_uint8_t  b[16*4];
  } _regs;

  Cap_list _caps;
};

inline Saved_cap::~Saved_cap() {};

class Root_bridge : public Bus
{
private:
  Hw::Device *_host;

public:
  explicit Root_bridge(int segment, unsigned bus_nr,
                       Bus_type bus_type, Hw::Device *host)
  : Bus(bus_nr, bus_type), _host(host), segment(segment)
  {}


  void set_host(Hw::Device *host) { _host = host; }

  Hw::Device *host() const { return _host; }

  void setup(Hw::Device *host);

  void increase_subordinate(int x)
  {
    if (x > subordinate)
      subordinate = x;
  }

  int segment;
};

struct Transparent_msi
{
  virtual l4_uint32_t filter_cmd_read(l4_uint32_t cmd) = 0;
  virtual l4_uint16_t filter_cmd_write(l4_uint16_t cmd, l4_uint16_t ocmd) = 0;
  virtual ~Transparent_msi() = default;
};

class Dev :
  public virtual If,
  private Io_irq_pin::Msi_src
{
public:
  class Flags
  {
    l4_uint16_t _raw;

  public:
    CXX_BITFIELD_MEMBER(1, 1, msi, _raw);
    CXX_BITFIELD_MEMBER(2, 2, state_saved, _raw);

    Flags() : _raw(0) {}
  };

  Msi_src *get_msi_src() override
  {
    assert (bus());

    if (bus()->bus_type == Bus::Pci_express_bus)
      return this;

    // block MSI as we have a host-bridge that is not PCIe
    Dev *bus_dev = dynamic_cast<Dev *>(bus());
    if (!bus_dev || !bus_dev->bus())
      return 0;

    if (bus_dev->bus()->bus_type == Bus::Pci_express_bus)
      return this;

    return bus_dev->get_msi_src();
  }

  l4_uint64_t get_src_info(Msi_mgr *mgr) override
  {
    assert (mgr);
    assert (bus());

    _msi_mgrs.add(mgr);
    if (bus()->bus_type == Bus::Pci_express_bus)
      return 0x40000 | (bus()->num << 8) | devfn() | (_phantomfn_bits << 16);

    return 0x80000 | (bus()->num << 8) | bus()->subordinate;
  }

protected:
  cxx::H_list_t<Msi_mgr> _msi_mgrs;

  Hw::Device *_host;
  Bus *_bus;

public:
  l4_uint32_t vendor_device;
  l4_uint32_t cls_rev;
  l4_uint32_t subsys_ids;
  l4_uint8_t  hdr_type;
  l4_uint8_t  irq_pin;
  Flags flags;

private:
  unsigned char _phantomfn_bits = 0;
  Resource *_bars[6];
  Resource *_rom;

  l4_uint8_t _pm_cap;
  Transparent_msi *_transp_msi = 0;

  Saved_config _saved_state;

  void parse_msi_cap(Cfg_addr cap_ptr);

public:
  enum Cfg_status
  {
    CS_cap_list = 0x10, // ro
    CS_66_mhz   = 0x20, // ro
    CS_fast_back2back_cap        = 0x00f0,
    CS_master_data_paritey_error = 0x0100,
    CS_devsel_timing_fast        = 0x0000,
    CS_devsel_timing_medium      = 0x0200,
    CS_devsel_timing_slow        = 0x0400,
    CS_sig_target_abort          = 0x0800,
    CS_rec_target_abort          = 0x1000,
    CS_rec_master_abort          = 0x2000,
    CS_sig_system_error          = 0x4000,
    CS_detected_parity_error     = 0x8000,
  };

  enum Cfg_command
  {
    CC_io          = 0x0001,
    CC_mem         = 0x0002,
    CC_bus_master  = 0x0004,
    CC_serr        = 0x0100,
    CC_int_disable = 0x0400,
  };



  Resource *bar(int b) const override
  {
    if (is_64bit_high_bar(b))
      return _bars[b-1];
    else
      return _bars[b];
  }

  Resource *rom() const override
  { return _rom; }

  l4_uint32_t recheck_bars(unsigned decoders) override;
  l4_uint32_t checked_cmd_read() override;
  l4_uint16_t checked_cmd_write(l4_uint16_t mask, l4_uint16_t cmd) override;
  bool enable_rom() override;

  bool is_64bit_high_bar(int b) const override
  {
    return l4_addr_t(_bars[b]) == 1;
  }

  explicit Dev(Hw::Device *host, Bus *bus, l4_uint8_t hdr_type)
  : _host(host), _bus(bus), vendor_device(0), cls_rev(0), hdr_type(hdr_type),
    _rom(0), _pm_cap(0)
  {
    for (unsigned i = 0; i < sizeof(_bars)/sizeof(_bars[0]); ++i)
      _bars[i] = 0;
  }

  Hw::Device *host() const { return _host; }

  bool supports_msi() const override
  { return flags.msi(); }

  Cfg_addr cfg_addr(unsigned reg = 0) const;
  Cap find_pci_cap(unsigned char id);

  bool is_bridge() const override
  { return (cls_rev >> 16) == 0x0604 && (hdr_type & 0x7f) == 1; }

  using If::cfg_read;
  using If::cfg_write;
  int cfg_read(l4_uint32_t reg, l4_uint32_t *value, Cfg_width) override;
  int cfg_write(l4_uint32_t reg, l4_uint32_t value, Cfg_width) override;

  l4_uint32_t vendor_device_ids() const override;
  l4_uint32_t class_rev() const override;
  l4_uint32_t subsys_vendor_ids() const override;
  unsigned bus_nr() const override;
  Bus *bus() const override
  { return _bus; }

  void setup(Hw::Device *host) override;
  virtual void discover_resources(Hw::Device *host);
  virtual void discover_bus(Hw::Device *) {}

  bool match_cid(cxx::String const &cid) const override;
  void dump(int indent) const override;

  int vendor() const { return vendor_device & 0xffff; }
  int device() const { return (vendor_device >> 16) & 0xffff; }

  unsigned devfn() const override
  {
    unsigned x = _host->adr();
    return ((x >> 13) & 0xf8) | (x & 7);
  }

  unsigned phantomfn_bits() const override
  { return _phantomfn_bits; }

  void set_phantomfn_bits(unsigned char bits)
  { _phantomfn_bits = bits & 3; }

  unsigned disable_decoders();
  void restore_decoders(unsigned cmd);

  bool check_pme_status();

  void pm_save_state(Hw::Device *) override;
  void pm_restore_state(Hw::Device *) override;

private:
  int discover_bar(int bar);
  void discover_expansion_rom();
  void discover_pci_caps();
};


class Driver
{
public:
  virtual int probe(Dev *) = 0;
  virtual ~Driver() = 0;

  bool register_driver_for_class(l4_uint32_t device_class);
  bool register_driver(l4_uint16_t vendor, l4_uint16_t device);
  static Driver* find(Dev *);
};

inline Driver::~Driver() {}

class Irq_router : public Resource
{
public:
  Irq_router() : Resource(Irq_res) {}
  void dump(int) const;
  bool compatible(Resource *consumer, bool = true) const
  {
    // only relative CPU IRQ lines are compatible with IRQ routing
    // global IRQs must be allocated at a higher level
    return consumer->type() == Irq_res && consumer->flags() & F_relative;
  }

};

template< typename RES_SPACE >
class Irq_router_res : public Irq_router
{
protected:
  typedef RES_SPACE Irq_rs;
  mutable Irq_rs _rs;

public:
  template< typename ...ARGS >
  Irq_router_res(ARGS && ...args) : _rs(cxx::forward<ARGS>(args)...) {}

  RES_SPACE *provided() const { return &_rs; }
};


class Pci_pci_bridge_irq_router_rs : public Resource_space
{
public:
  bool request(Resource *parent, ::Device *, Resource *child, ::Device *cdev);
  bool alloc(Resource *, ::Device *, Resource *, ::Device *, bool)
  { return false; }

  void assign(Resource *, Resource *)
  {
    d_printf(DBG_ERR, "internal error: cannot assign to root Pci_pci_bridge_irq_router_rs\n");
  }

  bool adjust_children(Resource *)
  {
    d_printf(DBG_ERR, "internal error: cannot adjust root Pci_pci_bridge_irq_router_rs\n");
    return false;
  }
};


class Pci_pci_bridge_basic : public Bus, public Dev
{
public:
  unsigned char pri;

  using Dev::cfg_write;
  using Dev::cfg_read;
  using Bus::cfg_write;
  using Bus::cfg_read;

  /**
   * Constructor to create a new Pci_pci_bridge_basic object
   *
   * \param[in] host      Parent device
   * \param[in] bus       PCI bus
   * \param     hdr_type  Header type that defines the layout of the PCI config
   *                      header.
   */
  Pci_pci_bridge_basic(Hw::Device *host, Bus *bus, l4_uint8_t hdr_type);

  void increase_subordinate(int x)
  {
    if (subordinate < x)
      {
	subordinate = x;
	cfg_write(Config::Subordinate, x, Cfg_byte);
	_bus->increase_subordinate(x);
      }
  }

  int cfg_read(Cfg_addr addr, l4_uint32_t *value, Cfg_width width)
  { return _bus->cfg_read(addr, value, width); }

  int cfg_write(Cfg_addr addr, l4_uint32_t value, Cfg_width width)
  { return _bus->cfg_write(addr, value, width); }

  void discover_bus(Hw::Device *host)
  {
    Bus::discover_bus(host);
    Dev::discover_bus(host);
  }

  void dump(int indent) const
  {
    Dev::dump(indent);
    Bus::dump(indent);
  }

};


class Pci_pci_bridge : public Pci_pci_bridge_basic
{
public:
  Resource *mmio;
  Resource *pref_mmio;
  Resource *io;

  explicit Pci_pci_bridge(Hw::Device *host, Bus *bus, l4_uint8_t hdr_type)
  : Pci_pci_bridge_basic(host, bus, hdr_type), mmio(0), pref_mmio(0), io(0)
  {}

  void setup_children(Hw::Device *host);
  void discover_resources(Hw::Device *host);
};

struct Port_root_bridge : public Root_bridge
{
  explicit Port_root_bridge(int segment, unsigned bus_nr,
                            Bus_type bus_type, Hw::Device *host)
  : Root_bridge(segment, bus_nr, bus_type, host) {}

  int cfg_read(Cfg_addr addr, l4_uint32_t *value, Cfg_width);
  int cfg_write(Cfg_addr addr, l4_uint32_t value, Cfg_width);

private:
  pthread_mutex_t _cfg_lock = PTHREAD_MUTEX_INITIALIZER;
};

struct Mmio_root_bridge : public Root_bridge
{
  explicit Mmio_root_bridge(int segment, unsigned bus_nr,
                            Bus_type bus_type, Hw::Device *host,
                            l4_uint64_t phys_base, unsigned num_busses)
  : Root_bridge(segment, bus_nr, bus_type, host)
  {
    _mmio = res_map_iomem(phys_base, num_busses * (1 << 20));
    if (!_mmio)
      throw("ne");
  }

  int cfg_read(Cfg_addr addr, l4_uint32_t *value, Cfg_width);
  int cfg_write(Cfg_addr addr, l4_uint32_t value, Cfg_width);

  l4_addr_t a(Cfg_addr addr) const { return _mmio + addr.addr(); }

  l4_addr_t _mmio;
};

Root_bridge *root_bridge(int segment);
Root_bridge *find_root_bridge(int segment, int bus);
int register_root_bridge(Root_bridge *b);


// IMPLEMENTATION ------------------------------------------------------

inline
int
Dev::cfg_read(l4_uint32_t reg, l4_uint32_t *value, Cfg_width width)
{
  return _bus->cfg_read(cfg_addr(reg), value, width);
}

inline
int
Dev::cfg_write(l4_uint32_t reg, l4_uint32_t value, Cfg_width width)
{
  return _bus->cfg_write(cfg_addr(reg), value, width);
}

inline
Hw::Pci::Cfg_addr
Dev::cfg_addr(unsigned reg) const
{
  return Cfg_addr(bus()->num, host()->adr() >> 16, host()->adr() & 0xff, reg);
}


}}
