/*
 * (c) 2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/re/env>
#include <l4/re/namespace>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/object_registry>
#include <l4/re/dataspace>
#include <l4/cxx/ipc_server>

#include <l4/sys/typeinfo_svr>

#include <cstring>
#include <cstdio>
#include <unistd.h>

#include "interface.h"

/**
 * A very simple server object, just providing the
 * shared memory data space and an IRQ object to send a notification.
 */
class My_server_obj : public L4::Server_object_t<L4::Kobject>
{
private:
  /**
   * The capability to the shared memory.
   */
  L4::Cap<L4Re::Dataspace> _shm;
  L4::Cap<L4::Irq> _irq;

public:
  /**
   * Create a new object for the given data space.
   */
  explicit My_server_obj(L4::Cap<L4Re::Dataspace> shm, L4::Cap<L4::Irq> irq)
  : _shm(shm), _irq(irq)
  {}

  /**
   * Dispatch function, dealing with remote requests.
   */
  int dispatch(l4_umword_t obj, L4::Ipc::Iostream &ios);
};


int My_server_obj::dispatch(l4_umword_t obj, L4::Ipc::Iostream &ios)
{
  // we don't care about the original object reference, however
  // we could read out the access rights from the lowest 2 bits
  (void) obj;

  l4_msgtag_t t;
  ios >> t; // extract the tag

  switch (t.label())
    {
    case L4::Meta::Protocol:
      // handle the meta protocol requests, implementing the
      // runtime dynamic type system for L4 objects.
      return L4::Util::handle_meta_request<My_interface>(ios);
    case 0:
      // since we have just one operation we have no opcode dispatch,
      // and just return the data-space and the notifier IRQ capabilities
      ios << _shm << _irq;
      return 0;
    default:
      // every other protocol is not supported.
      return -L4_EBADPROTO;
    }
}


/**
 * A simple Server object attached to a notifier IRQ.
 * This provides a kind of interrupt handler integrated in our
 * server.
 */
class Shm_observer : public L4::Irq_handler_object
{
private:
  /**
   * The pointer to the shared memory.
   */
  char *_shm;

public:
  /**
   * Create a new object for the given shared memory.
   */
  explicit Shm_observer(char *shm)
  : _shm(shm)
  {}

  /**
   * Dispatch function, dealing with remote requests.
   * This is the ISR.
   */
  int dispatch(l4_umword_t obj, L4::Ipc::Iostream &ios);
};

int Shm_observer::dispatch(l4_umword_t obj, L4::Ipc::Iostream &ios)
{
  // We don't care about the original object reference, however
  // we could read out the access rights from the lowest 2 bits
  (void)obj;

  // Since we end up here in this function, we got a 'message' from the IRQ
  // that is bound to us. The 'ios' stream won't contain any valuable info.
  (void)ios;

  printf("Client sent us: %s\n", _shm);

  return 0;
}

/**
 * The singleton for implementing the generic server logic for the
 * main thread. The factory is used for creating IPC gates for new server
 * objects, however in this example no IPC gate is created, instead an IPC gate
 * provided by the parent is used.
 */
static L4Re::Util::Registry_server<> server;

enum
{
  DS_SIZE = 4 << 12,
};

static char *get_ds(L4::Cap<L4Re::Dataspace> *_ds)
{
  *_ds = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
  if (!(*_ds).is_valid())
    {
      printf("Dataspace allocation failed.\n");
      return 0;
    }

  int err =  L4Re::Env::env()->mem_alloc()->alloc(DS_SIZE, *_ds, 0);
  if (err < 0)
    {
      printf("mem_alloc->alloc() failed.\n");
      L4Re::Util::cap_alloc.free(*_ds);
      return 0;
    }

  /*
   * Attach DS to local address space
   */
  char *_addr = 0;
  err =  L4Re::Env::env()->rm()->attach(&_addr, (*_ds)->size(),
                                        L4Re::Rm::Search_addr,
                                        L4::Ipc::make_cap_rw(*_ds));
  if (err < 0)
    {
      printf("Error attaching data space: %s\n", l4sys_errtostr(err));
      L4Re::Util::cap_alloc.free(*_ds);
      return 0;
    }

  /*
   * Success! Write something to DS.
   */
  printf("Attached DS\n");
  static char const * const msg = "[DS] Hello from server!";
  snprintf(_addr, strlen(msg) + 1, msg);

  return _addr;
}

int main()
{
  L4::Cap<L4Re::Dataspace> ds;
  char *addr;

  if (!(addr = get_ds(&ds)))
    return 2;

  // First the IRQ handler, because we need it in the My_server_obj object
  Shm_observer observer(addr);

  // Registering the observer as an IRQ handler, this allocates an
  // IRQ object using the factory of our server.
  L4::Cap<L4::Irq> irq = server.registry()->register_irq_obj(&observer);

  // Now the initial server object shared with the client via our parent.
  // it provides the data-space and the IRQ capabilities to a client.
  My_server_obj server_obj(ds, irq);

  // Registering the server object to the capability 'shm' in our the L4Re::Env.
  // This capability must be provided by the parent. (see the shared_ds.lua)
  server.registry()->register_obj(&server_obj, "shm");

  // Run our server loop.
  server.loop();
  return 0;
}
