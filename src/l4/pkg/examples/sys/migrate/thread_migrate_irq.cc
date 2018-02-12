/**
 * \file
 * \brief Example of thread migration.
 * \author Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *         Matthias Lange <matthias.lange@kernkonzept.com>
 *
 * This example shows how to migrate threads on CPUs. First, it queries the
 * available CPUs, creates a couple of threads, and then shuffles the
 * threads on the available CPUs.
 */
/*
 * (c) 2008-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/sys/scheduler>
#include <l4/sys/cxx/ipc_epiface>
#include <l4/re/env>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/object_registry>
#include <l4/re/util/br_manager>
#include <l4/re/error_helper>
#include <l4/cxx/exceptions>
#include <l4/cxx/unique_ptr>

#include <pthread-l4.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

/* Function for the threads. The content is not really relevant, so lets
 * just sleep around a bit. */
static void *thread_fn(void *)
{
  while (1)
    sleep(1);

  return 0;
}

class Migrate_server : public L4::Ipc_svr::Timeout_queue::Timeout,
                       public L4::Irqep_t<Migrate_server>
{
private:
  enum { NR_THREADS = 12 };

public:
  Migrate_server(L4::Ipc_svr::Server_iface *sif, unsigned interval_us)
    : _sif(sif), _interval(interval_us), _start_cpu(0),
      _current_set(l4_sched_cpu_set(0, 0))
  {
    check_cpus();

    create_threads();

    _sif->add_timeout(this, l4_kip_clock(l4re_kip()) + _interval);
  }

  void expired()
  {
    unsigned c = _start_cpu;
    for (unsigned t = 0; t < NR_THREADS; ++t)
      {
        c = get_next_cpu(c);
        l4_sched_param_t sp = l4_sched_param(20);
        sp.affinity = l4_sched_cpu_set(c, 0);
        L4Re::chksys(L4Re::Env::env()->scheduler()->run_thread(threads[t], sp),
                     "Migrating thread");

        printf("Migrated Thread%02d -> CPU%02d\n", t, c);
      }

    _start_cpu++;
    if (_start_cpu == _cpu_nrs)
      _start_cpu = 0;

    // migrate in _interval ms again
    _sif->add_timeout(this, timeout() + _interval);
  }

  void handle_irq()
  {
    l4_umword_t cpu_nrs;
    l4_sched_cpu_set_t cs = l4_sched_cpu_set(0, 0);

    L4Re::chksys(L4Re::Env::env()->scheduler()->info(&cpu_nrs, &cs),
                 "Get scheduler info");

    // the maximal number of supported CPUs does not change dynamically
    if (cpu_nrs != _cpu_nrs)
      printf("Warning: maximum number of CPUs has changed.\n"
             "         Ignoring excess CPUs.\n");

    // see what has changed
    l4_umword_t old = _current_set.map;
    l4_umword_t n = cs.map;
    for (unsigned i = 0; i < L4_MWORD_BITS; ++i)
      {
        // old online CPU now offline
        if ((old & 1UL) && !(n & 1UL))
          printf("CPU%02u went offline\n", i);

        // offline CPU now online
        if (!(old & 1UL) && (n & 1UL))
          printf("CPU%02u came online\n", i);

        old >>= 1UL;
        n >>= 1UL;
        if (!(old | n))
          break;
      }

    // update internal data structure
    _current_set.map = cs.map;
  }

private:
  L4::Ipc_svr::Server_iface *_sif;
  unsigned _interval;
  unsigned _start_cpu;
  L4::Cap<L4::Thread> threads[NR_THREADS];
  l4_umword_t _cpu_nrs;
  l4_sched_cpu_set_t _current_set;


  void check_cpus() throw()
  {
    L4Re::chksys(L4Re::Env::env()->scheduler()->info(&_cpu_nrs, &_current_set),
                 "Get scheduler info");

    printf("CPU map 0x%lx, gran=%u, offset=%u\n", _current_set.map,
                                                  _current_set.granularity(),
                                                  _current_set.offset());

    printf("%ld maximum number of supported CPUs.\n", _cpu_nrs);
    if (_cpu_nrs >= L4_MWORD_BITS)
      {
        printf("Will only handle %ld CPUs.\n", _cpu_nrs);
        _cpu_nrs = L4_MWORD_BITS;
      }
    else if (_cpu_nrs == 1)
      throw L4::Runtime_error(-L4_EINVAL, "Only found 1 CPU.");
  }

  /* Create a couple of threads and store their capabilities in an array */
  void create_threads() throw()
  {
    for (unsigned i = 0; i < NR_THREADS; ++i)
      {
        pthread_t t;

        if (pthread_create(&t, NULL, thread_fn, NULL))
          throw L4::Runtime_error(-L4_ENOSYS, "Could not create thread.");

        threads[i] = L4::Cap<L4::Thread>(pthread_l4_cap(t));
      }
    printf("Created %d threads.\n", NR_THREADS);
  }

  /* Helper function to get the next CPU */
  unsigned get_next_cpu(unsigned c)
  {
    unsigned x = c;
    for (;;)
      {
        x = (x + 1) % _cpu_nrs;
        // test if CPU x is online
        if ((1UL << x) & _current_set.map)
          return x;
        if (x == c)
          return c;
      }
  }

};

static L4Re::Util::Registry_server<L4Re::Util::Br_manager_timeout_hooks> server;

int main(void)
{
  try
    {
      // create a migration server object that migrates threads every second
      cxx::unique_ptr<Migrate_server> svr =
        cxx::make_unique<Migrate_server>(&server, 1000000);

      L4::Cap<L4::Irq> _sched_irq =
        server.registry()->register_irq_obj(svr.get());

      // bind our irq object to the Icu of the scheduler object
      L4Re::chksys(L4Re::Env::env()->scheduler()->bind(0, _sched_irq),
                   "Bind scheduler irq");

      L4Re::Env::env()->scheduler()->unmask(1);

      svr.release();

      server.loop();
    }
  catch (L4::Runtime_error &e)
    {
      printf("Runtime error: %s. Reason: %s.\n", e.str(), e.extra_str());
    }

  return 0;
}
