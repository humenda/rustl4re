/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include "device.h"
#include "event_source.h"
#include "pm.h"
#include "type_matcher.h"
#include "hw_device_client.h"
#include "dma_domain.h"
#include <l4/cxx/avl_map>

#include <cerrno>

#include <string>
#include <vector>
#include <l4/cxx/unique_ptr>
#include <l4/re/dma_space>

namespace Hw {

class Device_factory;
class Device;

class Dma_domain_factory
{
public:
  virtual Dma_domain *create(Hw::Device *bridge, Hw::Device *dev) = 0;
  virtual ~Dma_domain_factory() = default;
};

/** \brief A device feature extends a generic device with some extra
 *         functionality at runtime.
 *
 *  A device feature is an extension of a generic device, adding some device
 *  specific or bus specific functionality.
 *
 *  Features are for example things like being an PCI device or an ACPI device
 *  etc.
 *
 *  Each device can be extended with an arbitrary number of features.
 */
class Dev_feature
{
public:
  virtual ~Dev_feature() = 0;
  virtual bool match_cid(cxx::String const &) const { return false; }
  virtual void dump(int) const {}

  virtual void setup(Hw::Device *) {}
  virtual void setup_children(Hw::Device *) {}

  virtual void pm_save_state(Hw::Device *) {}
  virtual void pm_restore_state(Hw::Device *) {}

  virtual void enable_notifications(Hw::Device *) {}
  virtual void disable_notifications(Hw::Device *) {}
};

inline Dev_feature::~Dev_feature() {}

/** \brief A feature manager can be called every time a matching feature is
 *         added to a device.
 *
 * This class is not intended to be used directly, use Feature_manager instead.
 *
 * This base class is inherited by all feature managers that shall be tried
 * whenever Device::add_feaure() is called.
 */
struct Feature_manager_base : Type_matcher<Feature_manager_base>
{
  virtual bool do_match(Device *dev, Dev_feature *f) const = 0;
  virtual ~Feature_manager_base() = 0;

  Feature_manager_base(std::type_info const *type)
  : Type_matcher<Feature_manager_base>(type) {}
};

inline Feature_manager_base::~Feature_manager_base() {}

/// A generic hardware device.
class Device :
  public Generic_device,
  public Device_tree_mixin<Device>,
  public Pm
{
private:
  unsigned long _ref_cnt;
  l4_umword_t _uid;
  Int_property _adr;
  Int_property _flags;

public:
  enum Status
  {
    Disabled,
    Active
  };

  enum Device_flags
  {
    DF_dma_supported = 1 << 0,
    DF_multi_vbus    = 1 << 1,
  };

  unsigned long ref_count() const { return _ref_cnt; }
  void inc_ref_count() { ++_ref_cnt; }
  void dec_ref_count() { --_ref_cnt; }

  Device(l4_umword_t uid, l4_uint32_t adr)
  : _ref_cnt(0), _uid(uid), _adr(adr), _flags(0), _sta(Disabled)
  { register_properties(); }

  explicit Device(l4_uint32_t adr)
  : _ref_cnt(0), _uid((l4_umword_t)this), _adr(adr), _flags(0), _sta(Disabled)
  { register_properties(); }

  Device()
  : _ref_cnt(0), _uid((l4_umword_t)this), _adr(~0), _flags(0), _sta(Disabled)
  { register_properties(); }



  l4_umword_t uid() const { return _uid; }
  l4_uint32_t adr() const { return _adr.val(); }

  bool resource_allocated(Resource const *r) const;

  Device *get_child_dev_adr(l4_uint32_t adr, bool create = false);
  Device *get_child_dev_uid(l4_umword_t uid, l4_uint32_t adr, bool create = false);

  Device *parent() const { return _dt.parent(); }
  Device *children() const { return _dt.children(); }
  Device *next() const { return _dt.next(); }
  int depth() const { return _dt.depth(); }


  typedef Device_tree<Device>::iterator iterator;
  using Device_tree_mixin<Device>::begin;
  using Device_tree_mixin<Device>::end;


  typedef std::vector<Dev_feature *> Feature_list;
  Feature_list const *features() const { return &_features; }
  void add_feature(Dev_feature *f);

  template<typename T>
  T *find_feature()
  {
    for (Feature_list::const_iterator i = _features.begin();
         i != _features.end();
         ++i)
      if (T *r = dynamic_cast<T*>(*i))
        return r;

    return 0;
  }

  void plugin();
  bool setup();
  void setup_children();

  int pm_init();
  int pm_suspend();
  int pm_resume();

  virtual void init();
  Status status() const { return _sta; }

  void dump(int indent) const;

  char const *name() const { return _name.c_str(); }
  char const *hid() const { return _hid.val().c_str(); }

  void set_name(char const *name) { _name = name; }
  void set_name(std::string const &name) { _name = name; }
  void set_hid(char const *hid) { _hid.set(-1, hid); }
  void set_uid(l4_umword_t uid) { _uid = uid; }

  void add_cid(char const *cid) { _cid.push_back(cid); }

  bool match_cid(cxx::String const &cid) const;

  void add_client(Device_client *client);
  void check_conflicts() const;

  void notify(unsigned type, unsigned event, unsigned value);

  Io::Event_source_infos const *get_event_infos() const
  { return _event_source_infos.get(); }

  Io::Event_source_infos *get_event_infos(bool alloc = false)
  {
    if (!alloc || _event_source_infos.get())
      return _event_source_infos.get();

    _event_source_infos = cxx::make_unique<Io::Event_source_infos>();
    return _event_source_infos.get();
  }

  /**
   * \param dev  The device for which the DMA domain is requested,
   *             if `dev` is NULL a single DMA domain for all devices
   *             downstream of this device is requested.
   */
  Dma_domain *dma_domain_for(Device *dev)
  {
    Dma_domain *d = 0;
    for (Device *c = this; c; c = c->parent())
      {
        if (c->_downstream_dma_domain)
          {
            d = c->_downstream_dma_domain;
            break;
          }

        if (auto *f = c->_dma_domain_factory)
          {
            d = f->create(this, dev);
            break;
          }
      }

    if (dev && d)
      {
        dev->_dma_domain = d;
        dev->add_resource_rq(d);
      }

    return d;
  }

  void set_dma_domain_factory(Dma_domain_factory *dma)
  { _dma_domain_factory = dma; }

  Dma_domain *dma_domain()
  { return _dma_domain; }

  void set_downstream_dma_domain(Dma_domain *dma)
  { _downstream_dma_domain = dma; }

  /**
   * Get the DMA domain that **must** be used for our child devices.
   *
   * If this function returns NULL the caller must descend the device tree down
   * to the root and check until it finds a non-NULL secondary DMA domain.
   */
  Dma_domain *downstream_dma_domain() const
  { return _downstream_dma_domain; }

  bool is_multi_vbus_dev() const { return _flags & DF_multi_vbus; }

private:
  typedef std::vector<std::string> Cid_list;

  void register_properties()
  {
    register_property("hid", &_hid);
    register_property("adr", &_adr);
    register_property("flags", &_flags);
  }

protected:
  Status _sta;

private:
  std::string _name;
  String_property _hid;
  Cid_list _cid;

  Feature_list _features;
  Device_client::Client_list _clients;
  cxx::unique_ptr<Io::Event_source_infos> _event_source_infos;
  /// The DMA domain to be used for this device itself.
  Dma_domain *_dma_domain = 0;

  /// DMA domain that **must** be used for all children of this device.
  Dma_domain *_downstream_dma_domain = 0;
  Dma_domain_factory *_dma_domain_factory = 0;
};


/**
 * \brief A generic factory for creating hardware device objects of a specific
 *        type.
 *
 * \see Device_factory_t for more information.
 */
class Device_factory
{
public:
  static void dump();
  static Device *create(cxx::String const &name);
  static void register_factory(char const *name, Device_factory *f)
  { nm()[name] = f; }


  virtual Device *create() = 0;
  virtual ~Device_factory() = 0;

protected:
  typedef cxx::Avl_map<std::string, Device_factory *> Name_map;

  static Name_map &nm()
  {
    static Name_map _factories;
    return _factories;
  }
};

inline Device_factory::~Device_factory() {}


/**
 * \brief Template to fabricate a hardware device object of type \a HW_DEV.
 */
template< typename HW_DEV >
class Device_factory_t : public Device_factory
{
public:
  /**
   * \brief Create instantiate a factory for a given name.
   *
   * Those factories are used from the IO scripts to instantiate device nodes
   * for a given device class.
   */
  Device_factory_t(char const *name) { register_factory(name, this); }

  Device *create() { return new HW_DEV(); }
};

/**
 * \brief Helper code for the Feature_manager.
 */
namespace __Feature_manager_helper {

template<typename T>
struct Tw { T *v; Tw() : v(0) {} };

template<typename ...Features>
struct Fm;

template<typename F1, typename ...Features>
struct Fm<F1, Features...> : Tw<F1>, Fm<Features...>
{
  typedef F1 First_feature;

  template<typename F>
  F *get() const { return Tw<F>::v; }

  template<typename F>
  void set(F *f) { Tw<F>::v = f; }

  bool have_all() const
  {
    if (!Tw<F1>::v)
      return false;

    return Fm<Features...>::have_all();
  }

  template<typename G>
  bool add(G *g)
  {
    if (Tw<F1>::v)
      return Fm<Features...>::add(g);

    if (F1 *x = dynamic_cast<F1 *>(g))
      {
        Tw<F1>::v = x;
        return true;
      }

    return Fm<Features...>::add(g);
  }
};

template<>
struct Fm<>
{
  bool have_all() const { return true; }
  template<typename G>
  bool add(G *) { return false; }
};

}

/**
 * \brief A manager for a given combination of features (\a Features).
 *
 * To trigger an action for a specific set of features (\a Features) one needs
 * to inherit from Feature_manager and provide a setup() method that gets
 * called whenever a Device first has all of the features given in \a Features.
 */
template<typename ...Features>
struct Feature_manager : Feature_manager_base
{
  /** \brief override this function with your action. */
  virtual bool setup(Device *, Features *...) const = 0;

  bool do_match(Device *dev, Dev_feature *_f) const
  {
    if (!_f || !dev)
      return false;

    __Feature_manager_helper::Fm<Features...> h;
    if (!h.add(_f))
      return false;

    for (Device::Feature_list::const_iterator i = dev->features()->begin();
         !h.have_all() && i != dev->features()->end();
         ++i)
      h.add(*i);

    if (!h.have_all())
      return false;

    setup(dev, h.__Feature_manager_helper::template Tw<Features>::v...);
    return false;
  }

  Feature_manager()
  : Feature_manager_base(&typeid(typename __Feature_manager_helper::Fm<Features...>::First_feature))
  {}
};


}
