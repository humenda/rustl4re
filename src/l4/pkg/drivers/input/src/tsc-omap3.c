#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <pthread.h>
#include <pthread-l4.h>

#include <l4/input/drv_reg.h>
#include <l4/re/c/dataspace.h>
#include <l4/re/c/mem_alloc.h>
#include <l4/re/c/rm.h>
#include <l4/re/c/namespace.h>
#include <l4/re/c/util/cap_alloc.h>
#include <l4/re/event_enums.h>
#include <l4/io/io.h>
#include <l4/sys/icu.h>
#include <l4/sys/irq.h>
#include <l4/sys/thread.h>
#include <l4/sys/debugger.h>
#include <l4/util/util.h>
#include <l4/vbus/vbus.h>
#include <l4/vbus/vbus_gpio.h>
#include <l4/vbus/vbus_mcspi.h>

#include "tsc-omap3.h"

typedef struct {
    int x;
    int y;
} Tsc_coord;

typedef struct {
    Tsc_coord ul;	// upper left coordinate
    Tsc_coord lr;	// lower right coordinate
} Tsc_disp;

static int get_width(Tsc_disp *d)
{ return d->lr.x - d->ul.x; }

static int get_height(Tsc_disp *d)
{ return d->lr.y - d->ul.y; }

static int is_valid(Tsc_disp *d, int x, int y)
{
  if ((x >= d->ul.x) && (x <= d->lr.x) &&
      (y >= d->ul.y) && (y <= d->lr.y))
    return 1;
  else
    return 0;
}

static char *omap_dev_name = "OMAP_TSC";

static Input_handler tsc_handler;
static void *tsc_priv;
static pthread_t _pthread;

static l4_cap_idx_t vbus = L4_INVALID_CAP;
static l4vbus_device_handle_t tsc_handle;
static l4vbus_device_handle_t gpio_handle;
static l4vbus_device_handle_t mcspi_handle;
static unsigned irq = 175;
static unsigned channel = 0; // This is fixed

// FIXME: the physical display depends on the touchscreen device
static Tsc_disp disp_phys = {{ 3856, 3856}, { 176, 176 }};
// FIXME: the virtual display depends on the LCD device
static Tsc_disp disp_virt = {{ 0, 0 }, { 480, 640 }};

static int tsc_init(void)
{
  vbus = l4re_env_get_cap("vbus");

  if (l4_is_invalid_cap(vbus))
    {
      printf("Failed to query vbus\n");
      return -1;
    }

  if (l4vbus_get_device_by_hid(vbus, 0, &tsc_handle, omap_dev_name, 0, 0))
    {
      printf("[TSC] Cannot find TSC device\n");
      return -L4_ENODEV;
    }

  if (l4vbus_get_device_by_hid(vbus, 0, &gpio_handle, "gpio", 0, 0))
    {
      printf("[TSC] Cannot find GPIO bus\n");
      return -L4_ENODEV;
    }

  if (l4vbus_get_device_by_hid(vbus, 0, &mcspi_handle, "mcspi", 0, 0))
    {
      printf("[TSC] Cannot find McSPI bus\n");
      return -L4_ENODEV;
    }

  return 0;
}

#if 0
static void tsc_get_pen_position(int *x, int *y)
{
  l4_umword_t data1 = 0, data2 = 0, data3 = 0, data4 = 0;

  l4vbus_bus_read(2, 0x8000, &data1);
  l4vbus_bus_read(2, 0xd300, &data2);
  l4vbus_bus_read(2, 0x9300, &data3);
  l4vbus_bus_read(2, 0x8000, &data4);
  
  // convert physical display coordinates to virtual display coordinates
  *x = ((*x - disp_phys.ul.x) * get_width(&disp_virt))/get_width(&disp_phys) + disp_virt.ul.x;
  *y = ((*y - disp_phys.ul.y) * get_height(&disp_virt))/get_height(&disp_phys) + disp_virt.ul.y;
}
#endif

static void tsc_get_pen_position(int *x, int *y)
{
  l4_umword_t data1 = 0, data2 = 0, data3 = 0, data4 = 0;

  l4vbus_mcspi_write(vbus, mcspi_handle, channel, 0x8000);
  l4vbus_mcspi_read(vbus, mcspi_handle, channel, &data1);

  l4vbus_mcspi_write(vbus, mcspi_handle, channel, 0xd300);
  l4vbus_mcspi_read(vbus, mcspi_handle, channel, &data2);

  l4vbus_mcspi_write(vbus, mcspi_handle, channel, 0x9300);
  l4vbus_mcspi_read(vbus, mcspi_handle, channel, &data3);

  l4vbus_mcspi_write(vbus, mcspi_handle, channel, 0x8000);
  l4vbus_mcspi_read(vbus, mcspi_handle, channel, &data4);

  *x = ((data2 & 0x7f) << 5) | ((data3 & 0xf800) >> 11);
  *y = ((data3 & 0x7f) << 5) | ((data4 & 0xf800) >> 11);

  // XXX convert physical display coordinates to virtual display coordinates
  *x = ((*x - disp_phys.ul.x) * get_width(&disp_virt))/get_width(&disp_phys) + disp_virt.ul.x;
  *y = ((*y - disp_phys.ul.y) * get_height(&disp_virt))/get_height(&disp_phys) + disp_virt.ul.y;
  
  //printf ("[TSC] Info: (x,y)=(%d,%d)\n", *x, *y);
}

l4_cap_idx_t get_icu(void);

static void create_motion_event(void)
{
  int x = 0, y = 0;
  tsc_get_pen_position(&x, &y);
  if (is_valid(&disp_virt, x, y))
    {
      Input_event ev_x = { L4RE_EV_ABS, L4RE_ABS_X, x };
      tsc_handler(ev_x, tsc_priv);
      Input_event ev_y = { L4RE_EV_ABS, L4RE_ABS_Y, y };
      tsc_handler(ev_y, tsc_priv);
    }
}

static int tsc_irq_func(void)
{
  l4_cap_idx_t irq_cap = l4re_util_cap_alloc();
  l4_cap_idx_t thread_cap = pthread_l4_cap(_pthread);
  l4_msgtag_t tag;

  l4_debugger_set_object_name(thread_cap, "tsc-omap3.irq");
  
#if 0
  if (l4io_request_irq2(irq, irq_cap, L4_IRQ_F_NEG_EDGE) < 0)
    return -2;
#endif
  // was L4_IRQ_F_LEVEL_HIGH
  tag = l4_rcv_ep_bind_thread(irq_cap, thread_cap, 0);
  if (l4_ipc_error(tag, l4_utcb()))
    return -3;


  while (1)
    {
      tag = l4_irq_receive(irq_cap, L4_IPC_NEVER);
      if (l4_ipc_error(tag, l4_utcb()))
	{
	  printf("[TSC] Error: Receive irq failed\n");
	  continue;
	}

      if (!tsc_handler)
	continue;

      create_motion_event();
      // generate touch start event;
      Input_event ev = { L4RE_EV_KEY, L4RE_BTN_LEFT, 1 };
      tsc_handler(ev, tsc_priv);

      int pen_up = 0;
      if ((pen_up = l4vbus_gpio_get(vbus, gpio_handle, irq)) < 0)
	return -6;
      while (!pen_up)
	{
	  create_motion_event();
	  l4_usleep(2);

	  if ((pen_up = l4vbus_gpio_get(vbus, gpio_handle, irq)) < 0)
	    return -6;
	}

      // generate touch end event;
      Input_event ev2 = { L4RE_EV_KEY, L4RE_BTN_LEFT, 0 };
      tsc_handler(ev2, tsc_priv);
  
#if 0
      l4_umword_t label = 0;
      l4_icu_unmask(get_icu(), irq, &label, L4_IPC_NEVER);
#endif
    }
}

static void* __irq_func(void *data)
{
  (void)data;
  int ret = tsc_irq_func();
  printf("[TSC] Warning: irq handler returned with:%d\n", ret);
  l4_sleep_forever();
}

static const char *tsc_get_info(void)
{ return "ARM OMAP3EVM TSC"; }

static int tsc_probe(const char *name)
{
  if (strcmp(omap_dev_name, name)) {
    printf("[TSC] I'm not the right driver for [%s]\n", name);
    return 0;
  }
  return !l4io_lookup_device(omap_dev_name, NULL, 0, 0);
}

static void tsc_attach(Input_handler handler, void *priv)
{ 
  tsc_handler = handler;
  tsc_priv = priv;
  pthread_attr_t thread_attr;

  int err;
  if ((err = pthread_attr_init(&thread_attr)) != 0)
    printf("[TSC] Error: Initializing pthread attr: %d", err);

  struct sched_param sp;
  sp.sched_priority = 0x20;
  pthread_attr_setschedpolicy(&thread_attr, SCHED_L4);
  pthread_attr_setschedparam(&thread_attr, &sp);
  pthread_attr_setinheritsched(&thread_attr, PTHREAD_EXPLICIT_SCHED);
  
  err = pthread_create(&_pthread, &thread_attr, __irq_func, 0);
  if (err != 0)
    printf("[TSC] Error: Creating thread");
}

static void tsc_enable(void)
{
  if (tsc_init())
    {
      printf("[TSC] Init failed!\n");
      return;
    }
}

static void tsc_disable(void)
{}

static struct arm_input_ops arm_tsc_ops_omap3 = {
    .get_info           = tsc_get_info,
    .probe              = tsc_probe,
    .attach             = tsc_attach,
    .enable             = tsc_enable,
    .disable            = tsc_disable,
};

arm_input_register(&arm_tsc_ops_omap3);
