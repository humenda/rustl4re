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

#include "main.h"
#include "debug.h"

#include <l4/cxx/avl_map>
#include <l4/cxx/hlist>
#include <string>
#include <typeinfo>

#include "hw_device.h"
#include "type_matcher.h"
#include "vdevice.h"

namespace Vi {

template< typename VI, typename HW >
class Generic_type_factory
: public Type_matcher<Generic_type_factory<VI, HW>, VI*>
{
private:
  typedef Generic_type_factory<VI, HW> Self;

public:
  virtual VI *do_match(HW *f) = 0;

protected:
  explicit Generic_type_factory(std::type_info const *type)
  : Type_matcher<Self, VI*>(type) {}
};


typedef Generic_type_factory<Dev_feature, Hw::Dev_feature> Feature_factory;
typedef Generic_type_factory<Resource, Resource> Resource_factory;

class Dev_factory : public Type_matcher<Dev_factory, Device *>
{
public:
  virtual Device *vcreate() = 0;
  virtual Device *do_match(Hw::Device *f) = 0;

  typedef cxx::Avl_map<std::string, Dev_factory *> Name_map;

protected:
  explicit Dev_factory(std::type_info const *type)
  : Type_matcher<Dev_factory, Device *>(type)
  {}

  static Name_map &name_map()
  {
    static Name_map _name_map;
    return _name_map;
  }

public:
  static Device *create(std::string const &_class);
  static Device *create(Hw::Device *f)
  { return match(f); }

private:
  Dev_factory(Dev_factory const &);
  void operator = (Dev_factory const &);
};


template< typename VI,  typename HW_BASE, typename HW, typename BASE >
class Generic_factory_t : public BASE
{
public:

  Generic_factory_t() : BASE(&typeid(HW)) {}

  VI *do_match(HW_BASE *dev)
  {
    VI *d = 0;
    if (HW* h = dynamic_cast<HW*>(dev))
      d = new VI(h);
    return d;
  }

  VI *vcreate()
  { return 0; }

};

template< typename VI, typename HW >
class Feature_factory_t
: public Generic_factory_t<VI, Hw::Dev_feature, HW, Feature_factory >
{};

template< typename VI, typename HW >
class Resource_factory_t
: public Generic_factory_t<VI, Resource, HW, Resource_factory >
{};

template< typename V_DEV, typename HW_DEV = void >
class Dev_factory_t :  public Dev_factory
{
public:
  typedef HW_DEV Hw_dev;
  typedef V_DEV  V_dev;

  Dev_factory_t() : Dev_factory(&typeid(Hw_dev))
  { }


  Device *do_match(Hw::Device *dev)
  {
    if (dev->ref_count())
      if (!dev->is_multi_vbus_dev())
        printf("WARNING: device '%s' already assigned to another virtual bus.\n",
               dev->name());

    if (!dynamic_cast<HW_DEV const*>(dev))
      return 0;

    Device *d = new V_dev(static_cast<Hw_dev*>(dev));
    dev->inc_ref_count();
    return d;
  }

  Device *vcreate()
  { return 0; }

};

template< typename V_DEV >
class Dev_factory_t<V_DEV, void> :  public Dev_factory
{
public:
  typedef void  Hw_dev;
  typedef V_DEV V_dev;

  explicit Dev_factory_t(std::string const &_class) : Dev_factory(0)
  { name_map()[_class] = this; }


  Device *do_match(Hw::Device *)
  { return 0; }

  Device *vcreate()
  { return new V_dev; }

};


}

