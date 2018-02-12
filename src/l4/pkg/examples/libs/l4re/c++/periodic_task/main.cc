/**
 * (c) 2014 Steffen Liebergeld <steffen.liebergeld@kernkonzept.com>
 *
 * This file is licensed under the terms of the GNU General Public License 2.
 * See the file COPYING-GPL-2 for details.
 */
#include <l4/re/error_helper>
#include <l4/re/util/object_registry>
#include <l4/cxx/ipc_timeout_queue>

#include <cstdio>

class My_loop_hooks :
  public L4::Ipc_svr::Timeout_queue_hooks<My_loop_hooks>,
  public L4::Ipc_svr::Ignore_errors
{
public:
  /**
   * This function is required by Timeout_queue_hooks to get current time.
   */
  l4_kernel_clock_t now()
  {
    return l4_kip_clock(l4re_kip());
  }
};

L4Re::Util::Registry_server<My_loop_hooks> server;

/**
 * This class implements the code that needs to be run periodically.
 */
class Periodic_task : public L4::Ipc_svr::Timeout_queue::Timeout
{
public:
  Periodic_task(int iterations, L4::Ipc_svr::Server_iface *sif)
  : _iterations(iterations), _sif(sif)
  {
    // Add this to the IPC server's Timeout_queue with an absolute timeout.
    // The timeout will expire in 5 seconds from now.
    _sif->add_timeout(this, l4_kip_clock(l4re_kip()) + 5000000);
  }

  /**
   * This function is called by the Timeout_queue when its timeout expired.
   */
  void expired()
  {
    printf("Timeout expired.\n");
    if (!--_iterations)
      printf("Bye bye from ex_periodic_task!\n");
    else
      // We add ourselves back into the Timeout_queue with a new absolute timeout
      // that is 5 seconds after the previous timeout.
      _sif->add_timeout(this, timeout() + 5000000);
  }

private:
  int _iterations;
  L4::Ipc_svr::Server_iface *_sif;
};

static Periodic_task task(5, &server);

int main()
{
  printf("Hello from ex_periodic_task\n");

  server.loop();
}

