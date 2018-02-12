%module Io

%include <typemaps.i>
%include <lua_typemap.i>
%include "std_string.i"
%include "lua_typeinfo.i"

%{

#include "virt/vdevice.h"
#include "virt/vbus_factory.h"
#include "hw_device.h"
#include "gpio"

int luaopen_Io(lua_State *);
int add_vbus(Vi::Device *dev);
void dump_devs(Device *d);

%}

%ignore Resource_space;
%ignore Resource_list;
%ignore Resource_provider;
%ignore Root_resource;
%ignore Vi::Dev_feature;
%ignore Hw::Dev_feature;
%ignore Mmio_data_space::_r;
%ignore Resource::map_iomem;
%ignore Resource::validate;
%ignore Resource::contains;
%ignore Resource::provider_device_handle;
%ignore Resource::provider_device_handle;
%ignore Resource::start_end;
%ignore Resource::start_size;
%ignore Resource::end;
%ignore Resource::set_empty;
%ignore Resource::lt_compare;
%ignore Mmio_data_space::alloc_ram;


%include "resource.h"

%apply SWIGTYPE *DISOWN { Hw::Device *disown };
%apply SWIGTYPE *DISOWN { Vi::Device *disown };
%apply SWIGTYPE *DISOWN { Resource *disown };

%nodefaultctor;
%rename(Vi_dev_factory) Vi::Dev_factory;
%rename(Vi_device) Vi::Device;
%rename(Hw_dev_factory) Hw::Device_factory;
%rename(Hw_device) Hw::Device;

%nodefaultdtor Vi::Dev_factory;
%nodefaultdtor Hw::Device_factory;
%nodefaultdtor Property;

class Device
{};

class Generic_device : public Device
{
public:
  void add_resource(Resource *disown);
  Property *property(std::string const &name);
};

%extend Generic_device
{
  int nresources() const
  {
    return self->resources()->size();
  }

  Resource *resource(int idx) const
  {
    return self->resources()->at(idx);
  }
};

class Property
{
public:
  int set(int, std::string const &);
  int set(int, l4_uint64_t);
  int set(int, Generic_device *);
  int set(int, Resource *);
};

namespace Vi {

class Device : public Generic_device
{
public:
  void add_child(Device *disown);
  void name(char const *);
  char const *name() const;
  Device *parent() const;
  Device *children() const;
  Device *next() const;
  int depth() const;
  int add_filter(cxx::String const &, cxx::String const &);
  int add_filter(cxx::String const &, unsigned long long);
  int add_filter(cxx::String const &, unsigned long long, unsigned long long);

  void dump(int) const;
};

class Dev_factory
{
public:
  virtual Device *vcreate() = 0;
  virtual Device *do_match(Hw::Device *f) = 0;

  static Device *create(std::string const &_class);
  static Device *create(Hw::Device *f);
};
}

%extend Vi::Device
{
  void set_name(char const *name) { self->name(name); }
};

namespace Hw {
class Device : public Generic_device
{
public:
  enum Device_flags
  {
    DF_dma_supported = 1 << 0,
    DF_multi_vbus    = 1 << 1,
  };

  Property *property(char const *name);
  void add_child(Device *disown);
  void add_cid(char const *);
  void set_name(char const *);
  char const *name() const;
  bool match_cid(char const *cid) const;
  Device *parent() const;
  Device *children() const;
  Device *next() const;
  int depth() const;
  void plugin();

  void dump(int) const;
};

class Device_factory
{
public:
  virtual Device *create() = 0;
  static Device *create(char const *name);
};

}

%extend Hw::Device
{
  Hw::Device *find_by_name(std::string const &name) const
  {
    for (Hw::Device::iterator c = self->begin(0);
         c != self->end(); ++c)
      if (name == (*c)->name())
        return *c;

    return 0;
  }

  Hw::Device *__getitem(std::string const &name) const
  {
    for (Hw::Device::iterator c = self->begin(0);
         c != self->end(); ++c)
      if (name == (*c)->name())
        return *c;

    return 0;
  }

  void __setitem(std::string const &name, Hw::Device *dev)
  {
    dev->set_name(name);
    self->add_child(dev);
    if (self->parent() || self == system_bus())
      dev->plugin();
  }
};

Hw::Device *system_bus();
void dump_devs(Device *d);
int add_vbus(Vi::Device *dev);


class Gpio_resource : public Resource
{
public:
  explicit Gpio_resource(Hw::Device *dev, unsigned start, unsigned end)
    throw(char const *);
};

