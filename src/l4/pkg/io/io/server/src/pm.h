/*
 * (c) 2013 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <l4/cxx/hlist>

struct Pm : public cxx::H_list_item_t<Pm>
{
  enum Pm_state
  {
    Pm_disabled,
    Pm_failed,
    Pm_suspended,
    Pm_online
  };

  Pm() : _state(Pm_disabled) {}

  Pm_state pm_power_state() const { return _state; }
  bool pm_is_disabled() const { return _state == Pm_disabled; }
  bool pm_is_suspended() const { return _state == Pm_suspended; }
  bool pm_is_online() const { return _state == Pm_online; }
  bool pm_is_failed() const { return _state == Pm_failed; }

  virtual int pm_init() = 0;
  virtual int pm_suspend() = 0;
  virtual int pm_resume() = 0;
  virtual ~Pm() = 0;

  static int pm_suspend_all();
  static int pm_resume_all();

protected:
  void pm_set_state(Pm_state state)
  {
    Pm_state old = _state;

    if (old == state)
      return;

    _state = state;

    switch (old)
      {
      case Pm_suspended: _suspended.remove(this); break;
      case Pm_online:    _online.remove(this); break;
      default: break;
      }

    switch (state)
      {
      case Pm_suspended: _suspended.push_front(this); break;
      case Pm_online:    _online.push_front(this); break;
      default: break;
      }
  }

  typedef cxx::H_list_t<Pm> Pm_list;

  static Pm_list _online;
  static Pm_list _suspended;

private:
  Pm_state _state;
};

inline Pm::~Pm() {}
