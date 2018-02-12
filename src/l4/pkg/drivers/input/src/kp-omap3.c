/*
 * OMAP keypad driver
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <pthread.h>
#include <pthread-l4.h>

#include <l4/input/drv_reg.h>
#include <l4/io/io.h>
#include <l4/re/c/namespace.h>
#include <l4/re/c/util/cap_alloc.h>
#include <l4/re/event_enums.h>
#include <l4/util/util.h>
#include <l4/sys/irq.h>
#include <l4/sys/thread.h>
#include <l4/sys/debugger.h>
#include <l4/vbus/vbus.h>
#include <l4/vbus/vbus_i2c.h>

#include "kp-omap.h"


#define NUM_ROWS 5
#define NUM_COLS 4

static unsigned char kp_keycode[16] = {

    L4RE_KEY_1, L4RE_KEY_2, L4RE_KEY_3, L4RE_KEY_4,
    L4RE_KEY_5, L4RE_KEY_6, L4RE_KEY_7, L4RE_KEY_8,
    L4RE_KEY_9, L4RE_KEY_0, L4RE_KEY_A, L4RE_KEY_B,
    L4RE_KEY_C, L4RE_KEY_D, L4RE_KEY_E, L4RE_KEY_F,
};

static Input_handler kp_handler;
static void* kp_priv;
static pthread_t _pthread;

static l4_cap_idx_t vbus = L4_INVALID_CAP;
static l4vbus_device_handle_t i2c_handle = 0;

/* I2C slave ID */
#define TWL4030_SLAVENUM_NUM0 0x48
#define TWL4030_SLAVENUM_NUM1 0x49
#define TWL4030_SLAVENUM_NUM2 0x4a
#define TWL4030_SLAVENUM_NUM3 0x4b

/* Module Mapping */
struct twl4030mapping
{
  unsigned char sid;	/* Slave ID */
  unsigned char base;	/* base address */
};

/* mapping the module id to slave id and base address */
static struct twl4030mapping twl4030_map[TWL4030_MODULES + 1] = {
      { TWL4030_SLAVENUM_NUM0, TWL4030_BASE_USB },
      { TWL4030_SLAVENUM_NUM1, TWL4030_BASE_AUDIO_VOICE },
      { TWL4030_SLAVENUM_NUM1, TWL4030_BASE_GPIO },
      { TWL4030_SLAVENUM_NUM1, TWL4030_BASE_INTBR },
      { TWL4030_SLAVENUM_NUM1, TWL4030_BASE_PIH },
      { TWL4030_SLAVENUM_NUM1, TWL4030_BASE_TEST },
      { TWL4030_SLAVENUM_NUM2, TWL4030_BASE_KEYPAD },
      { TWL4030_SLAVENUM_NUM2, TWL4030_BASE_MADC },
      { TWL4030_SLAVENUM_NUM2, TWL4030_BASE_INTERRUPTS },
      { TWL4030_SLAVENUM_NUM2, TWL4030_BASE_LED },
      { TWL4030_SLAVENUM_NUM2, TWL4030_BASE_MAIN_CHARGE },
      { TWL4030_SLAVENUM_NUM2, TWL4030_BASE_PRECHARGE },
      { TWL4030_SLAVENUM_NUM2, TWL4030_BASE_PWM0 },
      { TWL4030_SLAVENUM_NUM2, TWL4030_BASE_PWM1 },
      { TWL4030_SLAVENUM_NUM2, TWL4030_BASE_PWMA },
      { TWL4030_SLAVENUM_NUM2, TWL4030_BASE_PWMB },
      { TWL4030_SLAVENUM_NUM3, TWL4030_BASE_BACKUP },
      { TWL4030_SLAVENUM_NUM3, TWL4030_BASE_INT },
      { TWL4030_SLAVENUM_NUM3, TWL4030_BASE_PM_MASTER },
      { TWL4030_SLAVENUM_NUM3, TWL4030_BASE_PM_RECIEVER },
      { TWL4030_SLAVENUM_NUM3, TWL4030_BASE_RTC },
      { TWL4030_SLAVENUM_NUM3, TWL4030_BASE_SECURED_REG },
};

#if 1
static int twl4030_i2c_write_u8(unsigned mod_no, l4_uint8_t value, l4_uint8_t reg)
{
  return l4vbus_i2c_write(vbus, i2c_handle, twl4030_map[mod_no].sid, twl4030_map[mod_no].base + reg, &value, 1);
}
#endif
#if 0
static int twl4030_i2c_read_u8(unsigned mod_no, l4_uint8_t *value, l4_uint8_t reg)
{
  unsigned long size = 1;
  return l4vbus_i2c_read(vbus, i2c_handle, twl4030_map[mod_no].sid, twl4030_map[mod_no].base + reg, value, &size);
}
#endif

static int twl4030_i2c_read_u32(int mod_no, l4_uint8_t *value, l4_uint8_t reg)
{
  unsigned long size = 4;
  return l4vbus_i2c_read(vbus, i2c_handle, twl4030_map[mod_no].sid, twl4030_map[mod_no].base + reg, value, &size);
}

static int kp_read(int reg, l4_uint8_t *val)
{
  unsigned long size = 1;
  return l4vbus_i2c_read(vbus, i2c_handle, twl4030_map[TWL4030_MODULE_KEYPAD].sid,
                         twl4030_map[TWL4030_MODULE_KEYPAD].base + reg, val, &size);
}

static int kp_write(int reg, l4_uint8_t val)
{
  return l4vbus_i2c_write(vbus, i2c_handle, twl4030_map[TWL4030_MODULE_KEYPAD].sid,
                          twl4030_map[TWL4030_MODULE_KEYPAD].base + reg, &val, 1);
}

#if 1
static int twl_init_irq(void)
{
  int ret = 0;

  /* PWR_ISR1 */
  ret |= twl4030_i2c_write_u8(TWL4030_MODULE_INT, 0xFF, 0x00);
  /* PWR_ISR2 */
  ret |= twl4030_i2c_write_u8(TWL4030_MODULE_INT, 0xFF, 0x02);
  /* PWR_IMR1 */
  ret |= twl4030_i2c_write_u8(TWL4030_MODULE_INT, 0xFF, 0x1);
  /* PWR_IMR2 */
  ret |= twl4030_i2c_write_u8(TWL4030_MODULE_INT, 0xFF, 0x3);
  /* PWR_ISR1 */
  ret |= twl4030_i2c_write_u8(TWL4030_MODULE_INT, 0xFF, 0x00);
  /* PWR_ISR2 */
  ret |= twl4030_i2c_write_u8(TWL4030_MODULE_INT, 0xFF, 0x02);
  
  /* BCIIMR1_1 */
  ret |= twl4030_i2c_write_u8(TWL4030_MODULE_INTERRUPTS, 0xFF, 0x3);
  /* BCIIMR1_2 */
  ret |= twl4030_i2c_write_u8(TWL4030_MODULE_INTERRUPTS, 0xFF, 0x4);
  /* BCIIMR2_1 */
  ret |= twl4030_i2c_write_u8(TWL4030_MODULE_INTERRUPTS, 0xFF, 0x7);
  /* BCIIMR2_2 */
  ret |= twl4030_i2c_write_u8(TWL4030_MODULE_INTERRUPTS, 0xFF, 0x8);
  
  /* MADC */
  /* MADC_IMR1 */
  ret |= twl4030_i2c_write_u8(TWL4030_MODULE_MADC, 0xFF, 0x62);
  /* MADC_IMR2 */
  ret |= twl4030_i2c_write_u8(TWL4030_MODULE_MADC, 0xFF, 0x64);
  /* GPIO_IMR1A */
  ret |= twl4030_i2c_write_u8(TWL4030_MODULE_GPIO, 0xFF, 0x1C);
  /* GPIO_IMR2A */
  ret |= twl4030_i2c_write_u8(TWL4030_MODULE_GPIO, 0xFF, 0x1D);
  /* GPIO_IMR3A */
  ret |= twl4030_i2c_write_u8(TWL4030_MODULE_GPIO, 0xFF, 0x1E);
  /* GPIO_IMR1B */
  ret |= twl4030_i2c_write_u8(TWL4030_MODULE_GPIO, 0xFF, 0x22);
  /* GPIO_IMR2B */
  ret |= twl4030_i2c_write_u8(TWL4030_MODULE_GPIO, 0xFF, 0x23);
  /* GPIO_IMR3B */
  ret |= twl4030_i2c_write_u8(TWL4030_MODULE_GPIO, 0xFF, 0x24);

  return ret;
}
#endif

static int init_keypad(void)
{
  int ret = 0;

  twl_init_irq();

  // Enable software mode and keypad power on
  ret = kp_write(REG_KEYP_CTRL_REG, 0x43);
  // Mask all interrupts
  ret |= kp_write(REG_KEYP_IMR1, 0x0f);
    {
      /* Clear ISR */
      l4_uint8_t v;
      kp_read(REG_KEYP_ISR1, &v);
      kp_read(REG_KEYP_ISR1, &v);
    }
  // Trigger interrupts on rising edge
  ret |= kp_write(REG_KEYP_EDR, 0xaa);
  // Set pre scalar field
  ret |= kp_write(REG_LK_PTV_REG, 4 << 5);
  // Set key debounce time 
  ret |= kp_write(REG_KEY_DEB_REG, 0x3f);
  // Enable COR-mode
  ret |= kp_write(REG_KEYP_SIH_CTRL, 0x4);
  // unmask all interrupts
  ret |= kp_write(REG_KEYP_IMR1, 0);

  return ret;
}

#if 0
static void drive_vibr(void)
{
  // VIBRATOR_CFG
  l4_uint8_t v = (1<<3)|(1<<2)|1;
  unsigned long s = 1;
  l4vbus_i2c_write(vbus, i2c_handle, 0x4b, 0x60, &v, 1);
  // configure mux
  //v = 0x10;
  //l4vbus_i2c_write(vbus, i2c_handle, 0x49, 0x92, &v, 1);
  
  l4_uint16_t w = (0x4 << 13)|(0x1 << 12)|(0x1 << 4)|(0xe);
  l4_uint8_t *p = (l4_uint8_t *)(&w);
  l4vbus_i2c_write(vbus, i2c_handle, 0x4b, 0x4b, &(p[1]), 1);
  l4vbus_i2c_write(vbus, i2c_handle, 0x4b, 0x4c, &(p[0]), 1);
  printf("WRITE:%x %x\n", p[1], p[0]);
  
  l4vbus_i2c_read(vbus, i2c_handle, 0x4b, 0x73, &v, &s);
  printf("VAUX1_TYPE:%x\n", v);
  l4vbus_i2c_read(vbus, i2c_handle, 0x4b, 0x74, &v, &s);
  printf("VAUX1_REMAP:%x\n", v);
  l4vbus_i2c_read(vbus, i2c_handle, 0x4b, 0x75, &v, &s);
  printf("VAUX1_DEDICATED:%x\n", v);
  l4vbus_i2c_read(vbus, i2c_handle, 0x4b, 0x76, &v, &s);
  printf("VAUX1_DEV_GRP:%x\n", v);
}
#endif

static void reset_keypad(void)
{
  kp_write(REG_KEYP_IMR1, 0xf);
}

static l4_uint8_t old_state[NUM_ROWS] = { 0 };

static int scan_key(void)
{
  l4_uint8_t new_state[NUM_ROWS];
  //unsigned long size = NUM_ROWS;

  twl4030_i2c_read_u32(TWL4030_MODULE_KEYPAD, new_state, REG_FULL_CODE_7_0);
  printf("[KEYP] state:%x %x %x %x %x\n",
      new_state[0], new_state[1], new_state[2], new_state[3], new_state[4]);
	
  /* check for changes and print those */
  int row = 0;
  for (row = 0; row < NUM_ROWS; row++)
    {
      int changed = new_state[row] ^ old_state[row];

      if (!changed)
	continue;

      int col = 0;
      for (col = 0; col < NUM_COLS; col++)
	{
	  if (!(changed & (1 << col)))
	    continue;
	  int key_pressed = new_state[row] & (1 << col);

	  printf("***********************key %s:row:%d col:%d\n", key_pressed ? "pressed" : "released", row, col);
	  Input_event ev = { L4RE_EV_KEY, kp_keycode[row * NUM_COLS + col], key_pressed };
	  kp_handler(ev, kp_priv);
      }
  }

  memcpy(old_state, new_state, NUM_ROWS);

  return 0;
}

static int kp_irq_func(void)
{
  l4_cap_idx_t irq_cap = l4re_util_cap_alloc();
  l4_cap_idx_t thread_cap = pthread_l4_cap(_pthread);
  l4_msgtag_t tag;

  l4_debugger_set_object_name(thread_cap, "kp-omap3.irq");
  
  if (l4io_request_irq(7, irq_cap) < 0)
    return -2;
  // was L4_IRQ_F_LEVEL_LOW
  tag = l4_rcv_ep_bind_thread(irq_cap, thread_cap, 0);
  if (l4_ipc_error(tag, l4_utcb()))
    return -3;

  while (1)
    {
      tag = l4_irq_receive(irq_cap, L4_IPC_NEVER);
      if (l4_ipc_error(tag, l4_utcb()))
	{
	  printf("[KEYP] Error: Receive irq failed\n");
	  continue;
	}

      kp_write(REG_KEYP_IMR1, 0xf);
     
      if (kp_handler)
	scan_key();

      l4_uint8_t value = 0;
      kp_read(REG_KEYP_ISR1, &value);
      kp_write(REG_KEYP_IMR1, 0x0);
    }
}

static void* __irq_func(void *data)
{
  (void)data;
  int ret = kp_irq_func();
  printf("[KEYP] Warning: irq handler returned with:%d\n", ret);
  l4_sleep_forever();
}

static
int kp_init(void)
{
  vbus = l4re_env_get_cap("vbus");

  if (l4_is_invalid_cap(vbus))
    {
      printf("[KEYP] Failed to query vbus\n");
      return -1;
    }

  if (l4vbus_get_device_by_hid(vbus, 0, &i2c_handle, "i2c", 0, 0))
    {
      printf("[KEYP] ##### Cannot find I2C\n");
    }

  return init_keypad();
}

static const char *kp_get_info(void)
{ return "ARM OMAP3EVM Keypad"; }

static int kp_probe(const char *name)
{
  if (strcmp("OMAP_KP", name))
    {
      printf("[KEYP] I'm not the right driver for %s\n", name);
      return 0;
    }
  return !l4io_lookup_device("OMAP_KP", NULL, 0, 0);
}

static void kp_attach(Input_handler handler, void *priv)
{ 
  kp_handler = handler;
  kp_priv = priv;
  pthread_attr_t thread_attr;

  int err;
  if ((err = pthread_attr_init(&thread_attr)) != 0)
    printf("[KEYP] Error: Initializing pthread attr: %d\n", err);

  struct sched_param sp;
  sp.sched_priority = 0x20;
  pthread_attr_setschedpolicy(&thread_attr, SCHED_L4);
  pthread_attr_setschedparam(&thread_attr, &sp);
  pthread_attr_setinheritsched(&thread_attr, PTHREAD_EXPLICIT_SCHED);
  
  err = pthread_create(&_pthread, &thread_attr, __irq_func, 0);
  if (err != 0)
    printf("[KEYP] Error: Creating thread\n");
}

static void kp_enable(void)
{
  if (kp_init())
    {
      printf("[KEYP] Error: Init failed!\n");
      return;
    }
}

static void kp_disable(void)
{
  reset_keypad();
}

static struct arm_input_ops arm_kp_ops_omap3 = {
    .get_info           = kp_get_info,
    .probe              = kp_probe,
    .attach             = kp_attach,
    .enable             = kp_enable,
    .disable            = kp_disable,
};

arm_input_register(&arm_kp_ops_omap3);
