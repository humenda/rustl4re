#include "debug.h"
#include "pm.h"

#include <cerrno>


Pm::Pm_list Pm::_online(true);
Pm::Pm_list Pm::_suspended(true);

int
Pm::pm_suspend_all()
{
  while (Pm *c = _online.front())
    {
      int res = c->pm_suspend();
      if (res < 0)
        {
          d_printf(DBG_ERR, "error: pm_suspend failed for %p: %d, aborting\n", c, res);
          return res;
        }

      if (_online.front() == c || c->pm_is_online())
        {
          d_printf(DBG_ERR, "error: pm_suspend failed for %p, not dequeued from online list, aborting\n", c);
          return -EBUSY;
        }
    }
  return 0;
}

int
Pm::pm_resume_all()
{
  while (Pm *c = _suspended.front())
    {
      int res = c->pm_resume();
      if (res < 0)
        d_printf(DBG_ERR, "error: pm_resume failed for %p: %d\n", c, res);

      if (_suspended.front() == c)
        {
          d_printf(DBG_ERR, "error: pm_resume failed for %p, not dequeued from suspended list\n", c);
          _suspended.remove(c);
          c->pm_set_state(Pm_failed);
        }
    }
  return 0;
}
