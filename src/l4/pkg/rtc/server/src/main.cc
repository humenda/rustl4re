/**
 * \file    rtc/server/src/main.cc
 * \brief   Initialization and main server loop
 *
 * \date    09/23/2003
 * \author  Frank Mehnert <fm3@os.inf.tu-dresden.de>
 * \author  Adam Lackorzynski <adam@os.inf.tu-dresden.de> */

/*
 * (c) 2003-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/rtc/rtc>
#if defined ARCH_x86 || defined ARCH_amd64
#include <l4/util/rdtsc.h>
#endif
#include <l4/re/env>
#include <l4/re/inhibitor>
#include <l4/re/util/object_registry>
#include <l4/re/util/br_manager>
#include <l4/re/event>
#include <l4/re/event_enums.h>
#include <l4/re/util/event>
#include <l4/vbus/vbus>
#include <l4/vbus/vbus_inhibitor.h>
#include <l4/cxx/exceptions>
#include <l4/re/error_helper>
#include <l4/re/util/cap_alloc>
#include <l4/sys/factory>
#include <l4/sys/cxx/ipc_epiface>
#include <l4/cxx/l4iostream>
#include <l4/cxx/iostream>

#include <l4/sys/ipc_gate>
#include <l4/sys/irq>

#include <cstdlib>
#include <cstdio>
#include <vector>
#include <l4/cxx/hlist>

#include "rtc.h"

cxx::H_list_t<Rtc> Rtc::_rtcs(true); // allocate in BSS w/o ctor

struct Time_update_observer : cxx::H_list_item_t<Time_update_observer>
{
  virtual void update_time() = 0;
  virtual ~Time_update_observer() = 0;
};

inline Time_update_observer::~Time_update_observer() {}

class Clock
{
private:
  l4_uint64_t _offset_1970;
  bool _valid;
  Rtc *_rtc;
  cxx::H_list_t<Time_update_observer> _observers;

public:
  explicit Clock(Rtc *rtc)
  : _offset_1970(0), _valid(false), _rtc(rtc)
  {
    update_clock();
  }

  void add_observer(Time_update_observer *o) { _observers.add(o); }

  l4_uint64_t get_offset_1970() const
  { return _offset_1970; }

  void update_clock()
  {
    l4_uint64_t offset;
    int err = _rtc->get_time(&offset);
    _valid = false;
    if (err < 0)
      printf("error: could not read hardware clock: %d\n", err);
    else
      {
        _offset_1970 = offset;
        _valid = true;
        for (auto o = _observers.begin(); o != _observers.end(); ++o)
          o->update_time();
      }
  }

  void set_offset_1970(l4_uint64_t offset)
  {
    _offset_1970 = offset;
    int err = _rtc->set_time(offset);
    if (err < 0)
      {
        printf("error: could not set hardware clock: %d\n", err);
        _valid = false;
      }

    for (auto o = _observers.begin(); o != _observers.end(); ++o)
      o->update_time();
  }

  bool valid() const { return _valid; }
};


class Rtc_svr :
  public L4::Epiface_t<Rtc_svr, L4rtc::Rtc>,
  public Time_update_observer
{
private:
  Clock *_clock;
  void add_client(L4::Cap<L4::Irq> irq)
  { _client_irqs.push_back(irq); }

public:
  explicit
  Rtc_svr(Clock *clock) : _clock(clock)
  { _clock->add_observer(this); }

  void notify_clients()
  {
    for (auto i: _client_irqs)
      i->trigger();
  }

  void update_time() { notify_clients(); }

  long op_get_timer_offset(L4rtc::Rtc::Rights, L4rtc::Rtc::Time &offset)
  {
    offset = _clock->get_offset_1970();
    return _clock->valid() ? 0 : 1;
  }

  long op_set_timer_offset(L4rtc::Rtc::Rights rights, L4rtc::Rtc::Time offset)
  {
    if (!(rights & L4_CAP_FPAGE_W))
      return -L4_EPERM;

    _clock->set_offset_1970(offset);
    return _clock->valid() ? 0 : 1;
  }

  long op_bind(L4::Icu::Rights, unsigned irqnum,
               L4::Ipc::Snd_fpage irq)
  {
    if (irqnum != 0)
      return -L4_ERANGE;

    if (!irq.cap_received())
      return -L4_EINVAL;

    L4::Cap<L4::Irq> irqc = server_iface()->rcv_cap<L4::Irq>(0);
    if (!irqc.is_valid())
      return -L4_EINVAL; // server internal error, anyway

    l4_msgtag_t msg = L4Re::Env::env()->task()->cap_valid(irqc);
    if (msg.label() == 0)
      return -L4_EINVAL; // no cap received, bail out

    add_client(irqc);
    server_iface()->realloc_rcv_cap(0);
    return L4_EOK;
  }

  long op_unbind(L4::Icu::Rights , unsigned, L4::Ipc::Snd_fpage)
  {
    return 0; // hw we should implement this somehow
  }

  long op_info(L4::Icu::Rights, L4::Icu::_Info &info)
  {
    info.features = 0;
    info.nr_irqs = 1;
    info.nr_msis = 0;
    return 0;
  }

  long op_msi_info(L4::Icu::Rights, l4_umword_t, l4_uint64_t,
                   l4_icu_msi_info_t &)
  {
    return -L4_ENOSYS;
  }

  long op_mask(L4::Icu::Rights, unsigned)
  {
    return -L4_ENOREPLY;
  }

  long op_unmask(L4::Icu::Rights, unsigned)
  {
    return -L4_ENOREPLY;
  }

  long op_set_mode(L4::Icu::Rights, unsigned, l4_umword_t)
  {
    return 0;
  }

private:
  std::vector<L4::Cap<L4::Irq> > _client_irqs;
};

#if 0
    case L4RTC_OPCODE_register_irq:
      {
        L4::Cap<L4::Irq> irqc = server_iface()->rcv_cap<L4::Irq>(0);
        if (!irqc.is_valid())
          return -L4_EINVAL;
        msg = L4Re::Env::env()->task()->cap_valid(irqc);
        if (msg.label() == 0)
          return -L4_EINVAL;
        client_manager.add_client(irqc);
        server_iface()->realloc_rcv_cap(0);
        return L4_EOK;
      }
    default:
      return -L4_ENOSYS;
    }
}
#endif

class System_state_tracker :
  public L4::Irqep_t<System_state_tracker>,
  public Clock
{
public:
  explicit
  System_state_tracker(Rtc *rtc);

  void handle_irq();

  L4::Cap<L4::Irq> event_irq() const
  { return L4::cap_cast<L4::Irq>(_vbus_event.irq()); }

private:
  L4Re::Util::Event _vbus_event;
  L4::Cap<L4Re::Inhibitor> _vbus_inhibitor;
};

System_state_tracker::System_state_tracker(Rtc *rtc) : Clock(rtc)
{
  // get vbus (contains rtc ioports on x86, might be empty on arm)
  // We need the vbus for the inhibitors!
  L4::Cap<L4vbus::Vbus> vbus = L4Re::chkcap(L4Re::Env::env()->get_cap<L4vbus::Vbus>("vbus"),
                                            "Did not find cap 'vbus'");
  _vbus_inhibitor = vbus;
  _vbus_inhibitor->acquire(L4VBUS_INHIBITOR_SUSPEND,
                           "I need to invalidate my timestamp upon system suspend");
  _vbus_inhibitor->acquire(L4VBUS_INHIBITOR_WAKEUP,
                           "I need to get a fresh timestamp on system wakeup");

  _vbus_event.init<L4::Irq>(vbus);
}

inline void
System_state_tracker::handle_irq()
{
  L4Re::Event_buffer::Event *e;
  while ((e = _vbus_event.buffer().next()) != NULL)
    {
      auto type = e->payload.type;
      auto code = e->payload.code;

      e->free();

      if (type != L4RE_EV_PM)
        continue;

      switch (code)
        {
          case L4VBUS_INHIBITOR_SUSPEND:
            printf("Received suspend event.\n");
            _vbus_inhibitor->release(L4VBUS_INHIBITOR_SUSPEND);
            break;
          case L4VBUS_INHIBITOR_WAKEUP:
            printf("Received wakeup event.\n");
            _vbus_inhibitor->acquire(L4VBUS_INHIBITOR_SUSPEND,
                                     "I need to invalidate my timestamp upon system suspend");
            update_clock();
            break;
          default:
            break;
        }
    }
}

static L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks> server;

int
main()
{
  Rtc *rtc;
  if (!(rtc = Rtc::find_rtc()))
    {
      printf("RTC: Initialization failed, exiting\n");
      return 1;
    }

  try
    {
      static System_state_tracker tracker(rtc);
      static Rtc_svr rtc_server(&tracker);

      L4Re::chkcap(server.registry()->register_obj(&tracker, tracker.event_irq()),
                   "Could not register state tracker");
      L4Re::chkcap(server.registry()->register_obj(&rtc_server, "rtc"),
                   "Could not register RTC server. 'rtc' cap missing?");
    }
  catch (L4::Runtime_error const &e)
    {
      L4::cerr << e << "TERMINATED\n";
      abort();
    }

  server.loop();
}

