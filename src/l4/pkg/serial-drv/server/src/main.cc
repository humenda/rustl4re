/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>,
 *          Torsten Frenzel <frenzel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/drivers/uart_pl011.h>
#include <l4/drivers/uart_omap35x.h>
#include <l4/io/io.h>
#include <l4/re/env>
#include <l4/re/error_helper>
#include <l4/re/namespace>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/object_registry>
#include <l4/re/util/icu_svr>
#include <l4/re/util/vcon_svr>
#include <l4/re/util/br_manager>
#include <l4/sys/irq>
#include <l4/util/util.h>

#include <cstdlib>
#include <cstdio>
#include <stdarg.h>

#include <l4/cxx/ipc_server>
#include <l4/sys/cxx/ipc_legacy>


using L4Re::Env;
using L4Re::Util::Registry_server;

static Registry_server<L4Re::Util::Br_manager_hooks> server(l4_utcb(),
                                          Env::env()->main_thread(),
                                          Env::env()->factory());

using L4Re::Util::Vcon_svr;
using L4Re::Util::Icu_cap_array_svr;

class Serial_drv :
  public Vcon_svr<Serial_drv>,
  public Icu_cap_array_svr<Serial_drv>,
  public L4::Server_object_t<L4::Vcon>
{
public:
  Serial_drv();
  virtual ~Serial_drv() throw() {}

  bool running() const { return _running; }

  int vcon_write(const char *buffer, unsigned size);
  unsigned vcon_read(char *buffer, unsigned size);

  int handle_irq();

  bool init();

  // FIXME: this breaks IRQ handling need an extra object for this!
  L4_RPC_LEGACY_DISPATCH(L4::Vcon);
  int dispatch(l4_umword_t obj, L4::Ipc::Iostream &ios)
  { return dispatch<L4::Ipc::Iostream>(obj, ios); }

  void serprintf(char const *fmt, ...)
    __attribute__((format(printf, 2, 3)));

private:
  bool _running;
  L4::Uart *_uart;
  L4::Cap<L4::Irq> _uart_irq;
  Icu_cap_array_svr<Serial_drv>::Irq _irq;
};

Serial_drv::Serial_drv()
  : Icu_cap_array_svr<Serial_drv>(1, &_irq),
    _running(false), _uart(0), _uart_irq(L4_INVALID_CAP),
    _irq()
{
  if (init())
    _running = true;
}

int
Serial_drv::vcon_write(const char *buffer, unsigned size)
{
  _uart->write(buffer, size);
  return -L4_EOK;
}

unsigned
Serial_drv::vcon_read(char *buffer, unsigned size)
{
  unsigned i = 0;
  while (_uart->char_avail() && size)
    {
      int c = _uart->get_char(false);
      if (c >= 0)
	{
	  buffer[i++] = (char)c;
	  size--;
	}
      else
	break;
    }
  // if there still some data available send this info to the client
  if (_uart->char_avail())
    i++;
  else
    _uart_irq->unmask();
  return i;
}

int
Serial_drv::handle_irq()
{
  if (_irq.cap().is_valid())
    _irq.cap()->trigger();

  //_uart_irq->unmask();

  return L4_EOK;
}

void
Serial_drv::serprintf(const char *fmt, ...)
{
  va_list l;
  va_start(l, fmt);
  char buf[200];
  unsigned len = vsnprintf(buf, sizeof(buf), fmt, l);
  buf[sizeof(buf) - 1] = 0;

  if (running())
    _uart->write(buf, len > sizeof(buf) ? sizeof(buf) : len);
  va_end(l);
}

bool
Serial_drv::init()
{
  int irq_num = 37;
  l4_addr_t phys_base = 0x1000a000;
#if 0
  int irq_num = 74;
  l4_addr_t phys_base = 0x49020000;
#endif
  l4_addr_t virt_base = 0;

  if (l4io_request_iomem(phys_base, 0x1000, L4IO_MEM_NONCACHED, &virt_base))
    {
      printf("serial-drv: request io-memory from l4io failed.\n");
      return false;
    }
  printf("serial-drv: virtual base at:%lx\n", virt_base);

  L4::Io_register_block_mmio *regs = new L4::Io_register_block_mmio(virt_base);
  _uart = new (malloc(sizeof(L4::Uart_pl011))) L4::Uart_pl011(24019200);
  //_uart = new (malloc(sizeof(L4::Uart_omap35x))) L4::Uart_omap35x;
  _uart->startup(regs);

  _uart_irq = L4Re::Util::cap_alloc.alloc<L4::Irq>();
  if (!_uart_irq.is_valid())
    {
      serprintf("serial-drv: Alloc capability for uart-irq failed.\n");
      return false;
    }

  if (l4io_request_irq(irq_num, _uart_irq.cap()))
    {
      serprintf("serial-drv: request uart-irq from l4io failed\n");
      return false;
    }

  /* setting IRQ type to L4_IRQ_F_POS_EDGE seems to be wrong place */
  if (l4_error(_uart_irq->attach((l4_umword_t)static_cast<L4::Server_object *>(this),
	  L4Re::Env::env()->main_thread())))
    {
      serprintf("serial-drv: attach to uart-irq failed.\n");
      return false;
    }

  if ((l4_ipc_error(_uart_irq->unmask(), l4_utcb())))
    {
      serprintf("serial-drv: unmask uart-irq failed.\n");
      return false;
    }
  _uart->enable_rx_irq(true);

  return true;
}

#if 0
int
Serial_drv::dispatch(l4_umword_t obj, L4::Ipc::Iostream &ios)
{
  l4_msgtag_t tag;
  ios >> tag;
  switch (tag.label())
    {
    case L4_PROTO_IRQ:
        {
          int r = L4Re::Util::Icu_svr<Serial_drv>::dispatch(obj, ios);
          if (r)
            return r;
	  return handle_irq();
        }
    case L4_PROTO_LOG:
      return L4Re::Util::Vcon_svr<Serial_drv>::dispatch(obj, ios);
    default:
      return -L4_EBADPROTO;
    }
}
#endif

int main()
{
  Serial_drv serial_drv;

  if (!server.registry()->register_obj(&serial_drv, "vcon"))
    {
      printf("Failed to register serial driver; Aborting.\n");
      return 1;
    }

  if (!serial_drv.running())
    {
      printf("Failed to initialize serial driver; Aborting.\n");
      return 1;
    }

  printf("Starting server loop\n");
  server.loop();

  return 0;
}
