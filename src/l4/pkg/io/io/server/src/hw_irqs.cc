#include "hw_irqs.h"
#include <map>

namespace {

typedef std::map<unsigned, Kernel_irq_pin *> Irq_map;
static Irq_map _real_irqs;

}


Io_irq_pin *
Hw::Irqs::real_irq(unsigned n)
{
  Irq_map::const_iterator f = _real_irqs.find(n);
  Kernel_irq_pin *r;
  if (f == _real_irqs.end() || !(*f).second)
    _real_irqs[n] = r = new Kernel_irq_pin(n);
  else
    r = (*f).second;

  return r;
}



