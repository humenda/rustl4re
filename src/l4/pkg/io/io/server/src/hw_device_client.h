#pragma once

#include <l4/cxx/hlist>
#include <string>

namespace Hw {

struct Device_client : cxx::H_list_item_t<Device_client>
{
  typedef cxx::H_list_t<Device_client> Client_list;

  virtual void dump(int) const = 0;
  virtual bool check_conflict(Device_client const *other) const = 0;
  virtual std::string get_full_name() const = 0;
  virtual void notify(unsigned type, unsigned event, unsigned value) = 0;
  virtual ~Device_client() = 0;
};

inline Device_client::~Device_client() {}

}
