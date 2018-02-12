#include <l4/drivers/of.h>
#include <stdio.h>

namespace L4_drivers
{
class Of_if : public Of
{
private:
  phandle_t _chosen;
  ihandle_t _root;

  phandle_t get_device(const char*, const char *prop = "device_type");
  unsigned long cpu_detect(const char *prop);

public:
  Of_if() : Of(), _chosen(0), _root(0) {}
  unsigned long detect_ramsize();
  unsigned long detect_cpu_freq();
  unsigned long detect_bus_freq();
  unsigned long detect_time_freq();
  bool detect_devices(unsigned long *start_addr, unsigned long *length);
  void boot_finish();

  phandle_t get_chosen()
    {
      if(handle_valid(_chosen)) return _chosen;
      _chosen = (phandle_t)prom_call("finddevice", 1, 1, "/chosen");
      return _chosen;
    }

  ihandle_t get_root()
    {
      if(handle_valid(_root)) return _root;
      _root = (ihandle_t)prom_call("finddevice", 1, 1, "/");
      return _root;
    }

  void vesa_set_mode(int mode);
};
}
