#pragma once

enum
{
  Reg_dss_control	= 0x40,
};

enum
{
  Reg_dispc_revision                 = 0x400,
  Reg_dispc_sysconfig                = 0x410,
  Reg_dispc_sysstatus                = 0x414,
  Reg_dispc_irqstatus                = 0x418,
  Reg_dispc_irqenable                = 0x41C,
  Reg_dispc_control                  = 0x440,
  Reg_dispc_config                   = 0x444,
  Reg_dispc_capable                  = 0x448,
  Reg_dispc_default_colour0          = 0x44C,
  Reg_dispc_default_colour1          = 0x450,
  Reg_dispc_trans_colour0            = 0x454,
  Reg_dispc_trans_colour1            = 0x458,
  Reg_dispc_line_status              = 0x45C,
  Reg_dispc_line_number              = 0x460,
  Reg_dispc_timing_h                 = 0x464,
  Reg_dispc_timing_v                 = 0x468,
  Reg_dispc_pol_freq                 = 0x46C,
  Reg_dispc_divisor                  = 0x470,
  Reg_dispc_size_dig                 = 0x478,
  Reg_dispc_size_lcd                 = 0x47C,
  Reg_dispc_gfx_ba0                  = 0x480,
  Reg_dispc_gfx_ba1                  = 0x484,
  Reg_dispc_gfx_position             = 0x488,
  Reg_dispc_gfx_size                 = 0x48C,
  Reg_dispc_gfx_attributes           = 0x4A0,
  Reg_dispc_gfx_fifo_threshold       = 0x4A4,
  Reg_dispc_gfx_fifo_size_status     = 0x4A8,
  Reg_dispc_gfx_row_inc              = 0x4AC,
  Reg_dispc_gfx_pixel_inc            = 0x4B0,
  Reg_dispc_gfx_window_skip          = 0x4B4,
  Reg_dispc_gfx_table_ba             = 0x4B8,
};

enum
{
  Default_colour_mask           = 0x0FFFFFF,
  Transparency_colour_mask      = 0x0FFFFFF,
  X_pos_mask                    = 0x00007FF,
  Y_pos_mask                    = 0x00007FF,
  Pixel_inc_mask                = 0x000FFFF,
  Attributes_enable             = (1 << 0),
};

/* DISPC_CONTROL register fields. */
#define Dispc_control_gpout1                    (1 << 16)
#define Dispc_control_gpout0                    (1 << 15)
#define Dispc_control_rfbimode                  (1 << 11)
#define Dispc_control_tftdatalines_oalsb24b     (3 <<  8)
#define Dispc_control_tftdatalines_oalsb18b     (2 <<  8)
#define Dispc_control_tftdatalines_oalsb16b     (1 <<  8)
#define Dispc_control_godigital                 (1 <<  6)
#define Dispc_control_golcd                     (1 <<  5)
#define Dispc_control_stntft                    (1 <<  3)
#define Dispc_control_digitalenable             (1 <<  1)
#define Dispc_control_lcdenable                 (1 <<  0)

/* DISPC_SYSCONFIG register fields. */
#define Dispc_sysconfig_midlemode_nstandby      (1 << 12)
#define Dispc_sysconfig_sidlemode_nidle         (1 <<  3)
#define Dispc_sysconfig_softreset               (1 <<  1)

/* DISPC_SYSSTATUS register field. */
#define Dispc_sysstatus_resetdone               (1 << 0)

/* DISPC_CONFIG register fields. */
#define Dispc_config_palettegammatable          (1 <<  3)
#define Dispc_config_loadmode_frdatlefr         (1 <<  2)
#define Dispc_config_loadmode_pgtabusetb        (1 <<  1)

/* DISPC_TIMING_H register fields. */
#define Dispc_timing_h_hbp_shift                (20)
#define Dispc_timing_h_hfp_shift                (8)
#define Dispc_timing_h_hsw_shift                (0)

/* DISPC_TIMING_V register fields. */
#define Dispc_timing_v_vbp_shift                (20)
#define Dispc_timing_v_vfp_shift                (8)
#define Dispc_timing_v_vsw_shift                (0)

/* DISPC_POL_FREQ register fields. */
#define Dispc_pol_freq_ihs                      (1 << 13)
#define Dispc_pol_freq_rf_shift                 (16)
#define Dispc_pol_freq_onoff_shift              (17)
#define Dispc_pol_freq_ipc_shift                (14)
#define Dispc_pol_freq_ihs_shift                (13)
#define Dispc_pol_freq_ivs_shift                (12)

/* DISPC_DIVISOR. */
#define Dispc_divisor_lcd_shift                 (16)
#define Dispc_divisor_pcd_shift                 (0)

/* DISPC_SIZE_LCD register fields. */
#define Dispc_size_lcd_lpp                      (0x7FF << 16)
#define Dispc_size_lcd_lpp_shift                (16)
#define Dispc_size_lcd_ppl                      (0x7FF)
#define Dispc_size_lcd_ppl_shift                (0)

/* DISPC_GFX_SIZE register fields. */
#define Dispc_gfx_size_lpp                      (0x7FF << 16)
#define Dispc_gfx_size_lpp_shift                (16)
#define Dispc_gfx_size_ppl                      (0x7FF)
#define Dispc_gfx_size_ppl_shift                (0)

/* DISPC_ATTRIBUTES register fields. */
#define Dispc_attributes_gfxenable              (1 << 0)
#define Dispc_gfx_attributes_replication_en     (1 << 5)
#define Dispc_vid_attributes_replication_en     (1 << 10)

/* DISPC_GFX_FIFO_THRESHOLD register fields. */
#define Dispc_gfx_fifo_threshold_high_shift     (16)
#define Dispc_gfx_fifo_threshold_low_shift      (0)
#define Dispc_fifo_threshold_high               (225)
#define Dispc_fifo_threshold_low                (194)

/* DISPC_IRQSTATUS registerr fields. */
#define Dispc_irqstatus_framedone               (1 <<  0)

/* Image Formats. */
#define BITMAP1     (0x0)
#define BITMAP2     (0x1)
#define BITMAP4     (0x2)
#define BITMAP8     (0x3)
#define RGB12       (0x4)
#define RGB16       (0x6)
#define RGB24       (0x8)
#define YUV422      (0xA)
#define UYVY422     (0xB)


#define GPIO_NUM_RESB       155
#define GPIO_NUM_QVGA_nVGA  154
#define GPIO_NUM_VDD        153
#define GPIO_NUM_INI	    152
#define GPIO_NUM_LR	      2
#define GPIO_NUM_UD	      3	


#define CONV_SCAN_DIRECTION     1
#define INVR_SCAN_DIRECTION     0

#define T2_I2C_LED_ADDR_GROUP   0x4a
#define T2_I2C_XXX_ADDR_GROUP   0x4b

/* Triton2 power module  registers */
#define TRITON2_LED_LEDEN_REG       0xee
#define TRITON2_LED_PWMAON_REG      0xef
#define TRITON2_LED_PWMAOFF_REG     0xf0
#define TRITON2_LED_PWMBON_REG      0xf1
#define TRITON2_LED_PWMBOFF_REG     0xf2
#define TRITON2_VDAC_DEDICATED      0x91
#define TRITON2_VDAC_DEV_GRP        0x8e
