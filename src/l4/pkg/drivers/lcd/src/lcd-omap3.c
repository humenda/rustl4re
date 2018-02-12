/*
 * OMAP3 CLCD driver
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <l4/drivers/lcd.h>
#include <l4/io/io.h>
#include <l4/re/c/dataspace.h>
#include <l4/re/c/mem_alloc.h>
#include <l4/re/c/namespace.h>
#include <l4/re/c/rm.h>
#include <l4/re/c/util/cap_alloc.h>
#include <l4/util/util.h>
#include <l4/vbus/vbus.h>
#include <l4/vbus/vbus_gpio.h>
#include <l4/vbus/vbus_i2c.h>

#include "lcd-omap3.h"

enum {
  /* Beagleboard mode assumes that U-Boot has set up everything and we
   * basically just need to set the framebuffer address */
  MODE_BEAGLEBOARD = 0,
  MODE_EVM = 1,
};

static int lcd_mode;

static inline int is_omap3evm(void)    { return lcd_mode == MODE_EVM; }
static inline int is_beagleboard(void) { return lcd_mode == MODE_BEAGLEBOARD; }

static inline int width(void)
{
  if (is_omap3evm())
    return 480;
  if (is_beagleboard())
    return 1280;
  return 0;
}

static inline int height(void)
{
  if (is_omap3evm())
    return 640;
  if (is_beagleboard())
    return 720;
  return 0;
}

static inline int bytes_per_pixel(void)
{ return 2; }

static unsigned int fbmem_size(void)
{ return height() * width() * bytes_per_pixel(); }


static l4_addr_t omap_dss_virt_base;
static void *fb_vaddr;
static l4_addr_t fb_paddr;
static l4_cap_idx_t vbus = L4_INVALID_CAP;
static l4vbus_device_handle_t i2c_handle;
static l4vbus_device_handle_t gpio_handle;

static l4_umword_t read_dss_reg(unsigned reg)
{
  return *((volatile l4_umword_t *)(omap_dss_virt_base + reg));
}

static void write_dss_reg(unsigned reg, l4_umword_t val)
{
  *((volatile l4_umword_t *)(omap_dss_virt_base + reg)) = val;
}

#if 0
static int disable_dss(void)
{
  l4_umword_t val = read_dss_reg(Reg_dispc_control);

  /* check if digital output or the lcd output are enabled */
  if (val & (Dispc_control_digitalenable | Dispc_control_lcdenable))
    {
      /*Disable the lcd output and digital output*/
      val &= ~(Dispc_control_digitalenable | Dispc_control_lcdenable);
      write_dss_reg(Reg_dispc_control, val);
      write_dss_reg(Reg_dispc_irqstatus, Dispc_irqstatus_framedone);

      l4_usleep(100);
      if (!(read_dss_reg(Reg_dispc_irqstatus) & Dispc_irqstatus_framedone))
	{
	  printf("OMAP LCD: Disable DSS timeout.\n");
	  return -1;
	}
    }
  return 0;
}

static void reset_display_controller(void)
{
  disable_dss();

  /* Reset the display controller. */
  write_dss_reg(Reg_dispc_sysconfig, Dispc_sysconfig_softreset);

  /* Wait until reset completes OR timeout occurs. */
  l4_usleep(100);
  if (!(read_dss_reg(Reg_dispc_sysstatus) & Dispc_sysstatus_resetdone))
    {
      printf("[LCD]: Warning: Reset DISPC timeout.\n");
    }

  l4_uint32_t reg_val = read_dss_reg(Reg_dispc_sysconfig);
  reg_val &= ~Dispc_sysconfig_softreset;
  write_dss_reg(Reg_dispc_sysconfig, reg_val);
}

static int enable_lcd_backlight(void)
{
  l4_uint8_t val;

  val = 0x32;
  if (l4vbus_i2c_write(vbus, i2c_handle, T2_I2C_LED_ADDR_GROUP, TRITON2_LED_LEDEN_REG, &val, 1))
    return -1;

  val = 0x7f;
  if (l4vbus_i2c_write(vbus, i2c_handle, T2_I2C_LED_ADDR_GROUP, TRITON2_LED_PWMAON_REG, &val, 1))
    return -1;

  val = 0x7f;
  if (l4vbus_i2c_write(vbus, i2c_handle, T2_I2C_LED_ADDR_GROUP, TRITON2_LED_PWMBON_REG, &val, 1))
    return -1;

  val = 0x7f;
  if (l4vbus_i2c_write(vbus, i2c_handle, T2_I2C_LED_ADDR_GROUP, TRITON2_LED_PWMAOFF_REG, &val, 1))
    return -1;

  val = 0x7f;
  if (l4vbus_i2c_write(vbus, i2c_handle, T2_I2C_LED_ADDR_GROUP, TRITON2_LED_PWMBOFF_REG, &val, 1))
    return -1;

  val = 0x0b;
  if (l4vbus_i2c_write(vbus, i2c_handle, 0x4b, TRITON2_VDAC_DEDICATED, &val, 1))
    return -1;

  val = 0xe0;
  if (l4vbus_i2c_write(vbus, i2c_handle, 0x4b, TRITON2_VDAC_DEV_GRP, &val, 1))
    return -1;

  return 0;
}

static int enable_lcd_power(void)
{
  if (l4vbus_gpio_write(vbus, gpio_handle, GPIO_NUM_VDD, 0))
    return -1;
  return 0;
}

#if 0
static int disable_lcd_power(void)
{
  if (l4vbus_gpio_write(vbus, GPIO_NUM_VDD, 1))
    return -1;
  return 0;
}
#endif

static int configure_vga_mode(void)
{
  if (l4vbus_gpio_write(vbus, gpio_handle, GPIO_NUM_QVGA_nVGA, 0))
    return -1;
  return 0;
}

static int configure_vert_scan_direction(int direction)
{
  if (l4vbus_gpio_write(vbus, gpio_handle, GPIO_NUM_UD, direction))
    return -1;
  return 0;
}

static int configure_horiz_scan_direction(int direction)
{
  if (l4vbus_gpio_write(vbus, gpio_handle, GPIO_NUM_LR, direction))
    return -1;
  return 0;
}

static int disable_lcd_reset(void)
{
  if (l4vbus_gpio_write(vbus, gpio_handle, GPIO_NUM_RESB, 0))
    return -1;
  l4_usleep(100);
  return 0;
}

static int enable_lcd_HVIF(void)
{
  l4_umword_t val = read_dss_reg(Reg_dispc_pol_freq);
  val |= ((0 << Dispc_pol_freq_rf_shift) |
          (1 << Dispc_pol_freq_onoff_shift));
  write_dss_reg(Reg_dispc_pol_freq, val);

  return 0;
}

static int enable_lcd_reset(void)
{
  if (l4vbus_gpio_write(vbus, gpio_handle, GPIO_NUM_RESB, 1))
    return -1;
  return 0;
}

static int enable_INI(void)
{
  if (l4vbus_gpio_write(vbus, gpio_handle, GPIO_NUM_INI, 1))
    return -1;
  return 0;
}

#if 0
static int disble_INI(void)
{
  if (l4vbus_gpio_write(vbus, GPIO_NUM_INI, 0))
    return -1;
  return 0;
}
#endif
#endif

static void issue_go_lcd(void)
{
  l4_umword_t val = read_dss_reg(Reg_dispc_control);
  val |= Dispc_control_golcd;
  write_dss_reg(Reg_dispc_control, val);

  l4_usleep(1000);
  if (read_dss_reg(Reg_dispc_control) & Dispc_control_golcd)
    {
      printf("[LCD] Info: Update DISPC timeout.\n");
    }
}

static void configure_dss_omap3evm(l4_addr_t frame_buffer)
{
  l4_uint32_t val;

  val = read_dss_reg(Reg_dss_control);
  val &= 0xfffffffe;
  write_dss_reg(Reg_dss_control, val);

  /* No standby, No idle,mormal mode, ocp clock free running */
  //val = Dispc_sysconfig_midlemode_nstandby | Dispc_sysconfig_sidlemode_nidle;
  //val &= ~Dispc_sysconfig_softreset ;
  //write_dss_reg(Reg_dispc_sysconfig, val);
  write_dss_reg(Reg_dispc_sysconfig, 0x2015);

  /* Not enabling any interrupts */
  write_dss_reg(Reg_dispc_irqenable, 0x00);

  /*  2:1 - Frame Data only loaded every frame (10) */
  write_dss_reg(Reg_dispc_config, Dispc_config_loadmode_frdatlefr);

  /* Default Color is white */
  write_dss_reg(Reg_dispc_default_colour0, 0xffffff & Default_colour_mask);

  /* Default Transparency Color is black */
  write_dss_reg(Reg_dispc_trans_colour0, 0xffffff & Transparency_colour_mask);

  /*timing logic for HSYNC signal */
  val = (38 << Dispc_timing_h_hbp_shift) |
        (44 << Dispc_timing_h_hfp_shift) |
	(2 << Dispc_timing_h_hsw_shift);
  write_dss_reg(Reg_dispc_timing_h, val);

  /*timing logic for VSYNC signal */
  val = (1 << Dispc_timing_v_vbp_shift) |
        (2 << Dispc_timing_v_vfp_shift) |
	(1 << Dispc_timing_v_vsw_shift) ;
  write_dss_reg(Reg_dispc_timing_v, val);

  /*signal configuration*/
  val = read_dss_reg(Reg_dispc_pol_freq);
  val |= (0 << Dispc_pol_freq_rf_shift) |
         (1 << Dispc_pol_freq_onoff_shift) |
	 (1 << Dispc_pol_freq_ipc_shift) |
	 (1 << Dispc_pol_freq_ihs_shift) |
	 (1 << Dispc_pol_freq_ivs_shift); 
  write_dss_reg(Reg_dispc_pol_freq, val);

  /*configure the divisor*/ 
  //val = (1 << Dispc_divisor_lcd_shift) | (3 << Dispc_divisor_pcd_shift);
  //write_dss_reg(Dispc_divisor, val);
  write_dss_reg(Reg_dispc_divisor, 0x10012);

  /* Set panel size */
  val = (((width() - 1) << Dispc_size_lcd_ppl_shift) & Dispc_size_lcd_ppl) |
        (((height() - 1) << Dispc_size_lcd_lpp_shift) & Dispc_size_lcd_lpp);
  write_dss_reg(Reg_dispc_size_lcd, val);

  /* Set tft interface width */
  val = read_dss_reg(Reg_dispc_control);
  val &= ~Dispc_control_tftdatalines_oalsb16b;
  val |= Dispc_control_tftdatalines_oalsb18b;
  write_dss_reg(Reg_dispc_control, val);

  /* Configure Graphics Window. */
  write_dss_reg(Reg_dispc_gfx_ba0, frame_buffer);
  write_dss_reg(Reg_dispc_gfx_ba1, frame_buffer);
  write_dss_reg(Reg_dispc_gfx_position, 0);
  val = (((width() - 1) << Dispc_gfx_size_ppl_shift) & Dispc_gfx_size_ppl) |
        (((height() - 1) << Dispc_gfx_size_lpp_shift) & Dispc_gfx_size_lpp);
  write_dss_reg(Reg_dispc_gfx_size, val);

  val = read_dss_reg(Reg_dispc_gfx_attributes);
  val |= (RGB16 << 1);
  write_dss_reg(Reg_dispc_gfx_attributes, val);

  val = (252 << Dispc_gfx_fifo_threshold_high_shift) |
        (192 << Dispc_gfx_fifo_threshold_low_shift);
  write_dss_reg(Reg_dispc_gfx_fifo_threshold, val);

  /* Default row inc = 1. */
  write_dss_reg(Reg_dispc_gfx_row_inc, 1);
  /* Default pixel inc = 1. */
  write_dss_reg(Reg_dispc_gfx_pixel_inc, 1);

  /* Enable GFX pipeline */
  val = read_dss_reg(Reg_dispc_gfx_attributes);
  val |= Attributes_enable;
  write_dss_reg(Reg_dispc_gfx_attributes, val);
}


static void configure_dss_beagleboard(l4_addr_t frame_buffer)
{
  // for beagleboard just set the framebuffer address and let it run,
  // everything else is already configured by U-Boot
  write_dss_reg(Reg_dispc_gfx_ba0, frame_buffer);
  write_dss_reg(Reg_dispc_gfx_ba1, frame_buffer);
}

static void configure_dss(l4_addr_t frame_buffer)
{
  if (is_omap3evm())
    configure_dss_omap3evm(frame_buffer);

  if (is_beagleboard())
    configure_dss_beagleboard(frame_buffer);

  printf("[LCD] Info: Configured display controller.\n");
}

static void display_lcd_image(void)
{

  if (is_omap3evm())
    {
      l4_umword_t val;
      /* Lcd output enabled, active display, 16-bit output */
      val = Dispc_control_gpout1 |
            Dispc_control_gpout0 |
            Dispc_control_tftdatalines_oalsb18b |
            Dispc_control_stntft |
            Dispc_control_lcdenable;
      val &= ~Dispc_control_rfbimode;
      write_dss_reg(Reg_dispc_control, val);
    }

  issue_go_lcd();
}

static int configure_lcd(l4_addr_t frame_buffer)
{
#if 0
  if (is_omap3evm())
    {
      reset_display_controller();
      if (enable_lcd_backlight())
        return -1;
      if (enable_lcd_power())
        return -1;
      if (configure_vga_mode())
        return -1;
      disable_lcd_reset();
      enable_lcd_HVIF();
      enable_lcd_reset();
      enable_INI();
      configure_vert_scan_direction(CONV_SCAN_DIRECTION);
      configure_horiz_scan_direction(CONV_SCAN_DIRECTION);
    }
#endif
  configure_dss(frame_buffer);

  //enable_replication_logic(GFX_PIPELINE);
  display_lcd_image();

  return 0;
}

static
int clcd_init(void)
{
  if (is_omap3evm())
    {
      vbus = l4re_env_get_cap("vbus");

      if (l4_is_invalid_cap(vbus))
        {
          printf("[LCD] Error: Could not query <vbus> capability\n");
          return -1;
        }

      if (l4vbus_get_device_by_hid(vbus, 0, &i2c_handle, "i2c", 0, 0))
        {
          printf("[LCD] Error: Could not find <i2c> vbus device\n");
          return -1;
        }

      if (l4vbus_get_device_by_hid(vbus, 0, &gpio_handle, "gpio", 0, 0))
        {
          printf("[LCD] Error: Could not find <gpio> vbus device\n");
          return -L4_ENODEV;
        }
    }

  return configure_lcd(fb_paddr);
}

static void setup_memory(void)
{
  int ret;

  l4_size_t phys_size;

  if (fb_vaddr)
    return;

  ret = l4io_request_iomem(0x48050000, 0x1000, 0, &omap_dss_virt_base);
  if (ret)
    {
      printf("[LCD] Error: Could not map device memory\n");
      return;
    }
  
  // get some frame buffer
  l4re_ds_t mem = l4re_util_cap_alloc();
  if (l4_is_invalid_cap(mem))
    return;

  if (l4re_ma_alloc(fbmem_size(), mem, L4RE_MA_CONTINUOUS | L4RE_MA_PINNED))
    {
      printf("[LCD] Error: Could not allocate memory\n");
      return;
    }

  fb_vaddr = 0;
  if (l4re_rm_attach(&fb_vaddr, fbmem_size(),
                     L4RE_RM_SEARCH_ADDR | L4RE_RM_EAGER_MAP,
                     mem, 0, L4_PAGESHIFT))
    {
      printf("[LCD] Error: Could not attach memory\n");
      return;
    }

  printf("[LCD] Info: Video memory is at virtual %p (size: 0x%x Bytes)\n",
         fb_vaddr, fbmem_size());

  // get physical address
  if (l4re_ds_phys(mem, 0, &fb_paddr, &phys_size)
      || phys_size != fbmem_size())
    {
      printf("[LCD] Error: Could not get physical address\n");
      return;
    }
  printf("[LCD] Info: Physical video memory is at %p\n", (void *)fb_paddr);
}


static int lcd_probe(const char *configstr)
{
  lcd_mode = MODE_BEAGLEBOARD;

  if (configstr &&
      (strstr(configstr, "evm")
       || strstr(configstr, "init")))
    lcd_mode = MODE_EVM;

  return !l4io_lookup_device("OMAP_LCD", NULL, 0, 0);
}

static void *lcd_get_fb(void)
{
  if (!fb_vaddr)
    setup_memory();

  return fb_vaddr;
}

static unsigned int lcd_fbmem_size(void) { return fbmem_size(); }

static const char *lcd_get_info(void)
{
  if (is_beagleboard())
    return "ARM OMAP3 Beagleboard LCD";

  if (is_omap3evm())
    return "ARM OMAP3EVM LCD";

  return "ARM OMAP3EVM unknown";
}

static int get_fbinfo(l4re_video_view_info_t *vinfo)
{
  vinfo->width               = width();
  vinfo->height              = height();
  vinfo->bytes_per_line      = bytes_per_pixel() * vinfo->width;

  vinfo->pixel_info.bytes_per_pixel = bytes_per_pixel();
  vinfo->pixel_info.r.shift         = 11;
  vinfo->pixel_info.r.size          = 5;
  vinfo->pixel_info.g.shift         = 5;
  vinfo->pixel_info.g.size          = 6;
  vinfo->pixel_info.b.shift         = 0;
  vinfo->pixel_info.b.size          = 5;
  vinfo->pixel_info.a.shift         = 0;
  vinfo->pixel_info.a.size          = 0;
  return 0;
}

static void lcd_enable(void)
{
  setup_memory();

  if (clcd_init())
    {
      printf("CLCD init failed!\n");
      return;
    }
}

static void lcd_disable(void)
{
  printf("%s unimplemented.\n", __func__);
}

static struct arm_lcd_ops arm_lcd_ops_omap3 = {
  .probe              = lcd_probe,
  .get_fb             = lcd_get_fb,
  .get_fbinfo         = get_fbinfo,
  .get_video_mem_size = lcd_fbmem_size,
  .get_info           = lcd_get_info,
  .enable             = lcd_enable,
  .disable            = lcd_disable,
};

arm_lcd_register(&arm_lcd_ops_omap3);
