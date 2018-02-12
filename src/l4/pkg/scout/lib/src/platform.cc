#include "platform.h"

Platform::Pf_factory *Platform::Pf_factory::first;

Platform *Platform::get(Rect const &r)
{
  Platform *p;
  for (Pf_factory *f = Pf_factory::first; f; f = f->next)
    {
      p = f->create(r);
      if (p)
	return p;
    }
  return 0;
}
