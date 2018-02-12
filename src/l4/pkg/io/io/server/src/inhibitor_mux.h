/*
 * (c) 2014 Steffen Liebergeld <steffen.liebergeld@kernkonzept.com>
 *     2013 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <l4/cxx/hlist>
#include <l4/vbus/vbus_inhibitor.h>
#include <l4/sys/types.h>

#include "debug.h"

class Inhibitor_mux;

/**
 * \brief Provider for inhibitor locks.
 *
 * This is a base class of vbus. It implements inhibitor locks and the
 * corresponding signals for each vbus.
 */
class Inhibitor_provider : public cxx::H_list_item_t<Inhibitor_provider>
{
public:
  explicit Inhibitor_provider(Inhibitor_mux *mux);

protected:
  /**
   * \brief Is this inhibitor taken?
   *
   * \param id Inhibitor.
   *
   * \returns true if the client acquired this inhibitor, false otherwise
   */
  bool inhibitor_acquired(l4_umword_t id) const
  { return _inhibitor_acquired[id]; }

  void inhibitor_acquire(l4_umword_t id, char const *reason);
  void inhibitor_release(l4_umword_t id);

  virtual ~Inhibitor_provider() = 0;

private:
  friend class Inhibitor_mux;

  /**
   * \brief Send a signal to the client.
   *
   * \param if Id of signal to send.
   */
  virtual void inhibitor_signal(l4_umword_t id) = 0;

  /**
   * \brief Return a name for this inhibitor provider.
   *
   * \returns The name of the client (in our case the name of the vbus)
   */
  virtual char const *inhibitor_name() const = 0;

  /**
   * \brief Return the reason the client gave when taking the lock.
   *
   * \param id Id of the lock.
   *
   * \returns The reason why the lock was taken.
   */
  char const *inhibitor_reason(l4_umword_t id) const
  { return _inhibitor_reason[id]; }

  Inhibitor_mux *const _mux;
  /// inhibitor locks held by the client
  bool  _inhibitor_acquired[L4VBUS_INHIBITOR_MAX];
  /// the reason why the client holds the lock
  char _inhibitor_reason[L4VBUS_INHIBITOR_MAX][80];
};

/**
 * \brief Inhibitor lock multiplexer.
 *
 * This class implements all logic needed to send all clients of IO that hold
 * an inhibitor lock a signal.
 */
class Inhibitor_mux
{
public:
  Inhibitor_mux()
  {
    for (unsigned i = 0; i < L4VBUS_INHIBITOR_MAX; ++i)
      _inhibitors_acquired[i] = 0;
  }

  virtual ~Inhibitor_mux() = 0;

  /**
   * \brief Determine if all inhibitor locks of a given id are free.
   *
   * \param id Id of inhibitor to check.
   *
   * \returns true if no client holds a lock of that id, false otherwise.
   */
  bool inhibitors_free(l4_umword_t id) const
  { return _inhibitors_acquired[id] == 0; }

  /**
   * \brief Send a signal to all clients that hold an inhibitor lock.
   *
   * \param id Id of signal to emit (identical to the lock id).
   */
  void inhibitor_signal(l4_umword_t id)
  {
    for (Client_list::Iterator i = _clients.begin(); i != _clients.end(); ++i)
      (*i)->inhibitor_signal(id);
  }

  /**
   * \brief Print a list of lock holders and their reasons.
   *
   * This is used when a suspend/shutdown/reboot operation times out.
   *
   * \param id Id of the lock to show info about.
   */
  void print_lock_holders(l4_umword_t id)
  {
    for (Client_list::Iterator i = _clients.begin(); i != _clients.end(); ++i)
      if ((*i)->inhibitor_acquired(id))
        d_printf(DBG_INFO, "Lock held by '%s': reason '%s'\n",
                 (*i)->inhibitor_name(), (*i)->inhibitor_reason(id));
  }

private:
  friend class Inhibitor_provider;
  typedef cxx::H_list_t<Inhibitor_provider> Client_list;

  void inhibitor_acquire(l4_umword_t id);
  void inhibitor_release(l4_umword_t id);

  /**
   * \brief All inhibitor locks of this id were freed.
   *
   * This is called by inhibitor_release when the last lock of a type was
   * released. In this function IO can trigger an action that was inhibited by
   * the lock.
   *
   * \param id Id of lock that is no longer held by any client.
   */
  virtual void all_inhibitors_free(l4_umword_t id) = 0;

  // list of Inhibitor_providers
  Client_list _clients;
  // data structure holding a count for each Inhibitor lock
  unsigned _inhibitors_acquired[L4VBUS_INHIBITOR_MAX];
};

inline Inhibitor_mux::~Inhibitor_mux() {}

