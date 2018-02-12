/**
 * \file
 * \brief Event handling routines.
 */
/*
 * (c) 2008-2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 *
 * As a special exception, you may use this file as part of a free software
 * library without restriction.  Specifically, if other files instantiate
 * templates or use macros or inline functions from this file, or you compile
 * this file and link it with other files to produce an executable, this
 * file does not by itself cause the resulting executable to be covered by
 * the GNU General Public License.  This exception does not however
 * invalidate any other reasons why the executable file might be covered by
 * the GNU General Public License.
 */

#include <l4/event/event>


namespace Event {


int
Event::wait()
{
  return l4_error(_irq->down());
}

Event_loop::Event_loop(L4::Cap<L4::Semaphore> irq, int prio)
  : Event_base(irq), _pthread(0)
{
  pthread_attr_t a;
  pthread_attr_init(&a);
  if (prio != -1)
    {
      sched_param sp;
      sp.sched_priority = prio;
      pthread_attr_setschedpolicy(&a, SCHED_L4);
      pthread_attr_setschedparam(&a, &sp);
      pthread_attr_setinheritsched(&a, PTHREAD_EXPLICIT_SCHED);
    }
  else
    pthread_attr_setinheritsched(&a, PTHREAD_INHERIT_SCHED);

  if (pthread_create(&_pthread, &a, event_loop, this))
    {
      _irq = L4::Cap<void>::Invalid;
      return;
    }
}

void
Event_loop::start()
{
}

Event_loop::~Event_loop()
{
  if (_pthread)
    pthread_cancel(_pthread);
}

void *
Event_loop::event_loop(void *data)
{
  Event_loop *e = reinterpret_cast<Event_loop *>(data);
  while (1)
    {
      l4_msgtag_t res = e->_irq->down();
      if (l4_ipc_error(res, l4_utcb()))
        continue;

      e->handle();
    }
  return 0;
}

}
