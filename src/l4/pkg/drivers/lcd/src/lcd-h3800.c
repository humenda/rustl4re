/*
 * Some routines to enable the LCD in an IPAQ H3800 (and maybe others).
 *
 * Contains stuff taken from Linux and bootldr.
 *
 */

#include <l4/drivers/lcd.h>
#include <stdio.h>

/* Gnah... */
void putstr(const char *);

#define CONFIG_MACH_IPAQ 1
#include <arch-sa1100/sa1100.h>
#include <arch-sa1100/sa1100-lcd.h>
#define __SERIAL_H__ /* don't include serial.h */
#include <arch-sa1100/h3600.h>
#include <arch-sa1100/h3600_asic.h>

typedef unsigned short u16;

#define SET_ASIC1(x) \
   do {if ( setp ) { H3800_ASIC1_GPIO_OUT |= (x); } else { H3800_ASIC1_GPIO_OUT &= ~(x); }} while(0)

#define CTL_REG_READ(addr)          (*(volatile unsigned long *)(addr))
#define CTL_REG_WRITE(addr, val)    (*(volatile unsigned long *)(addr) = (val))

#define CTL_REG_READ_BYTE(addr)     (*(volatile unsigned char *)(addr))
#define CTL_REG_WRITE_BYTE(addr, val)    (*(volatile unsigned char *)(addr) = (val))

#define TIMEOUT        350000

static void sa_lcd_light(int on, int level)
{
  (void)on; (void)level;
}

static void sa_control_egpio(enum ipaq_egpio_type x, int setp)
{
  switch (x) {
    case IPAQ_EGPIO_LCD_ON:
      SET_ASIC1( GPIO1_LCD_5V_ON
	         | GPIO1_LCD_ON
		 | GPIO1_LCD_PCI
		 | GPIO1_VGH_ON
		 | GPIO1_VGL_ON);
    case IPAQ_EGPIO_CODEC_NRESET:
    case IPAQ_EGPIO_AUDIO_ON:
    case IPAQ_EGPIO_QMUTE:
    case IPAQ_EGPIO_OPT_NVRAM_ON:
    case IPAQ_EGPIO_OPT_ON:
    case IPAQ_EGPIO_CARD_RESET:
    case IPAQ_EGPIO_OPT_RESET:
    case IPAQ_EGPIO_IR_ON:
    case IPAQ_EGPIO_IR_FSEL:
    case IPAQ_EGPIO_RS232_ON:
    case IPAQ_EGPIO_VPP_ON:
    case IPAQ_EGPIO_COM_DSR:
      break;
  }
}

static void arm_lcd_h3800_disable(void)
{
  sa_lcd_light(0, 0);

  LCSR = 0;   /* Clear LCD Status Register */
  LCCR0 &= ~(LCCR0_LDM);      /* Enable LCD Disable Done Interrupt */
  LCCR0 &= ~(LCCR0_LEN);      /* Disable LCD Controller */
  CTL_REG_WRITE(GPIO_BASE+GPIO_GPCR_OFF, 0 /*params->gpio*/);
  sa_control_egpio(IPAQ_EGPIO_LCD_ON, 0);
}

#define GPIO_WRITE(off, v)  \
    ((*((volatile unsigned long *)(((char*)GPIO_BASE)+(off)))) = (v))

#define GPIO_GAFR_WRITE(v)   GPIO_WRITE(GPIO_GAFR_OFF,v)
#define GPIO_GPDR_WRITE(v)   GPIO_WRITE(GPIO_GPDR_OFF,v)

static unsigned long *sa_vidmem;

static void arm_lcd_h3800_enable(void)
{
  //arm_lcd_h3800_disable();

  //printf("Enabling LCD controller.\n");

#if 0
  GPIO_GAFR_WRITE(0xff << 2);
  GPIO_GPDR_WRITE(0xff << 2);

  sa_vidmem[0] = 0x2077;

  LCCR3 = 0x10 | LCCR3_VrtSnchL | LCCR3_HorSnchL;
  LCCR2 = LCCR2_DisHght(VESA_YRES + 1) + LCCR2_VrtSnchWdth(3)
          + LCCR2_BegFrmDel(10) + LCCR2_EndFrmDel(1);
  LCCR1 = LCCR1_DisWdth(VESA_XRES) + LCCR1_HorSnchWdth(4) +
	  LCCR1_BegLnDel(0xC) + LCCR1_EndLnDel(0x11);
  LCCR0 = (LCCR0_LEN + LCCR0_Color + LCCR0_Sngl + LCCR0_Act +
           LCCR0_LtlEnd + LCCR0_LDM + LCCR0_BAM + LCCR0_ERM +
           LCCR0_DMADel(0))
          & ~LCCR0_LEN;

#endif
  {
    l4_size_t phys_size;

    if (!l4dm_mem_phys_addr(sa_vidmem, 4, &phys, 1, &pnum)
	|| !pnum)
      {
	printf("Cannot get physical address of vidmem.\n");
	return;
      }
    printf("Physical address of vidmem is %08lx\n", phys.addr);

    DBAR1 = phys.addr;
  }

#if 0
  LCCR0 |= LCCR0_LEN;

  sa_control_egpio(IPAQ_EGPIO_LCD_ON, 1);

  CTL_REG_WRITE(GPIO_BASE+GPIO_GPDR_OFF, 0xff << 2);
  CTL_REG_WRITE(GPIO_BASE+GPIO_GPSR_OFF, 0);

  sa_control_egpio(IPAQ_EGPIO_LCD_ON, 1);

#endif
  sa_lcd_light(1, 5);
}

/*
 * Returns the address to the framebuffer, and does some initialisation
 * stuff.
 */
static void *arm_lcd_h3800_fb(void)
{
  if (arm_driver_reserve_region(H3800_ASIC_BASE, 0x100000))
    return NULL;
  if (arm_driver_reserve_region(_LCCR0, 0x100000))
    return NULL;
  if (arm_driver_reserve_region(GPIO_BASE, 0x100000))
    return NULL;

  // get some frame buffer memory
  if (!(sa_vidmem = l4dm_mem_allocate(0x100000,
	                              L4DM_PINNED | L4DM_CONTIGUOUS |
                                      L4RM_MAP | L4RM_LOG2_ALLOC)))
    {
      printf("Could not get video memory.\n");
      return NULL;
    }
  printf("Video memory is at virtual %p\n", sa_vidmem);

  // --------------------------------------------------------------------

  //H3800_ASIC2_GPIODIR = GPIO2_PEN_IRQ | GPIO2_SD_DETECT | GPIO2_EAR_IN_N | GPIO2_USB_DETECT_N | GPIO2_SD_CON_SLT;

  // This is all for the H3800 display
  //*((unsigned short *) H3800_ASIC1_GPIO_MASK_ADDR) = H3800_ASIC1_GPIO_MASK_INIT;
  //*((unsigned short *) H3800_ASIC1_GPIO_OUT_ADDR) = H3800_ASIC1_GPIO_OUT_INIT;
  //*((unsigned short *) H3800_ASIC1_GPIO_DIR_ADDR) = H3800_ASIC1_GPIO_DIR_INIT;
  //*((unsigned short *) H3800_ASIC1_GPIO_OUT_ADDR) = H3800_ASIC1_GPIO_OUT_INIT;

  //sa_disable_controller();

  {
    int num_pixels;
    unsigned short* bufp = (unsigned short*)LCD_FB_IMAGE(sa_vidmem, 16);

    for (num_pixels = LCD_NUM_PIXELS(); num_pixels; num_pixels--)
      {
	*bufp++ = (unsigned short)num_pixels;
      }
  }


  arm_lcd_h3800_enable();

  return LCD_FB_IMAGE(sa_vidmem, 16);
}

static unsigned int arm_lcd_h3800_video_mem_size(void)
{ return LCD_XRES * LCD_YRES * ((LCD_BPP + 7) >> 3); }

static unsigned int arm_lcd_h3800_get_screen_width(void)
{ return LCD_XRES; }

static unsigned int arm_lcd_h3800_get_screen_height(void)
{ return LCD_YRES; }

static unsigned int arm_lcd_h3800_get_bpp(void)
{ return LCD_BPP; }

static unsigned int arm_lcd_h3800_get_bytes_per_line(void)
{ return LCD_XRES * ((LCD_BPP + 7) >> 3); }

static int get_fbinfo(l4re_fb_info_t *fbinfo)
{

  fbinfo->x_res               = LCD_XRES;
  fbinfo->y_res               = LCD_YRES;
  fbinfo->bits_per_pixel      = LCD_BPP;
  fbinfo->bytes_per_pixel     = (LCD_BPP + 7) >> 3;
  fbinfo->bytes_per_scan_line = ((LCD_BPP + 7) >> 3) * fbinfo->x_res;

  fbinfo->r.shift = 0;
  fbinfo->r.size  = 5;
  fbinfo->g.shift = 5;
  fbinfo->g.size  = 6;
  fbinfo->b.shift = 11;
  fbinfo->b.size  = 5;

  return 0;
}


static const char *arm_lcd_h3800_get_info(void)
{ return "ARM LCD driver for IPAQ H3800 series"; }

static int arm_lcd_h3800_probe(const char *configstr)
{
  (void)configstr;
  return !l4io_lookup_device("H3800 LCD", NULL, 0, 0);
}

struct arm_lcd_ops arm_lcd_ops_h3800 = {
  .probe              = arm_lcd_h3800_probe,
  .get_fb             = arm_lcd_h3800_fb,
  .get_fbinfo         = get_fbinfo,
  .get_video_mem_size = arm_lcd_h3800_video_mem_size,
  .get_info           = arm_lcd_h3800_get_info,
  .enable             = arm_lcd_h3800_enable,
  .disable            = arm_lcd_h3800_disable,
};

arm_lcd_register(&arm_lcd_ops_h3800);
