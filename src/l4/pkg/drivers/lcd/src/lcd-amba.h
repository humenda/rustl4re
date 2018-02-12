// System Controller Registers
enum
{
  Reg_sys_id   = 0x00,
  Reg_sys_sw   = 0x04,
  Reg_sys_led  = 0x08,
  Reg_sys_osc0 = 0x0C,
  Reg_sys_osc1 = 0x10,
  Reg_sys_osc2 = 0x14,
  Reg_sys_osc3 = 0x18,
  Reg_sys_osc4 = 0x1C,
  Reg_sys_lock = 0x20,
  Reg_sys_mci  = 0x48,
  Reg_sys_misc = 0x60,
  Reg_sys_clcd = 0x50,
};

enum
{
  Sys_clcd_idmask = 0x1F00,
  Sys_clcd_id84   = 0x0100,
  Sys_clcd_id38   = 0x0000,
  Sys_clcd_id25   = 0x0700,
};

enum
{
  Sys_lock_unlock = 0xA05F,
  Sys_lock_lock   = 0x0000,
};

enum
{
  Sys_osc4_xga    = 0x15c77,
  Sys_osc4_25mhz  = 0x2C77,
  Sys_osc4_10mhz  = 0x2C2A,
  Sys_osc4_5p4mhz = 0x2C13,
};

// CLCD Controller Registers
enum
{
  Reg_clcd_tim0 = 0x000,
  Reg_clcd_tim1 = 0x004,
  Reg_clcd_tim2 = 0x008,
  Reg_clcd_tim3 = 0x00C,
  Reg_clcd_ubas = 0x010,
  Reg_clcd_lbas = 0x014,
  Reg_clcd_cntl = 0x018,
  Reg_clcd_ienb = 0x01c,
  Reg_clcd_stat = 0x020,
  Reg_clcd_intr = 0x024,
  Reg_clcd_ucur = 0x028,
  Reg_clcd_lcur = 0x02C,
  Reg_clcd_pal  = 0x200,
};

enum
{
  Clcd_tim0_ppl84_xga = ((1024 / 16) - 1) << 2,
  Clcd_tim0_ppl84_vga = (( 640 / 16) - 1) << 2,
  Clcd_tim0_hsw84 = 63 << 8,  // hsync
  Clcd_tim0_hfp84 = 31 << 16, // hfront
  Clcd_tim0_hbp84 = 63 << 24, // hback
  Clcd_tim0_ppl38 = ((320/16)-1) << 2,
  Clcd_tim0_hsw38 = 5 << 8,
  Clcd_tim0_hfp38 = 5 << 16,
  Clcd_tim0_hbp38 = 5 << 24,
  Clcd_tim0_ppl25 = ((240/16)-1) << 2,
  Clcd_tim0_hsw25 = 10 << 8,
  Clcd_tim0_hfp25 = 30 << 16,
  Clcd_tim0_hbp25 = 20 << 24,
  Clcd_tim0_84_xga = Clcd_tim0_hbp84 | Clcd_tim0_hfp84| Clcd_tim0_hsw84 | Clcd_tim0_ppl84_xga,
  Clcd_tim0_84_vga = Clcd_tim0_hbp84 | Clcd_tim0_hfp84| Clcd_tim0_hsw84 | Clcd_tim0_ppl84_vga,
  Clcd_tim0_38     = Clcd_tim0_hbp38 | Clcd_tim0_hfp38| Clcd_tim0_hfp38 | Clcd_tim0_ppl38,
  Clcd_tim0_25     = Clcd_tim0_hbp25 | Clcd_tim0_hfp25| Clcd_tim0_hsw25 | Clcd_tim0_ppl25,
};

enum
{
  Clcd_tim1_lpp84_xga = (768-1),
  Clcd_tim1_lpp84_vga = (480-1),
  Clcd_tim1_vsw84 = 24 << 10, // vsync
  Clcd_tim1_vfp84 = 11 << 16, // vfront
  Clcd_tim1_vbp84 = 9 << 24,  // vback
  Clcd_tim1_lpp38 = (240-1),
  Clcd_tim1_vsw38 = 5 << 10,
  Clcd_tim1_vfp38 = 5 << 16,
  Clcd_tim1_vbp38 = 5 << 24,
  Clcd_tim1_lpp25 = (320-1),
  Clcd_tim1_vsw25 = 2 << 10,
  Clcd_tim1_vfp25 = 2 << 16,
  Clcd_tim1_vbp25 = 1 << 24,
  Clcd_tim1_84_xga = Clcd_tim1_vbp84 | Clcd_tim1_vfp84 | Clcd_tim1_vsw84 | Clcd_tim1_lpp84_xga,
  Clcd_tim1_84_vga = Clcd_tim1_vbp84 | Clcd_tim1_vfp84 | Clcd_tim1_vsw84 | Clcd_tim1_lpp84_vga,
  Clcd_tim1_38     = Clcd_tim1_vbp38 | Clcd_tim1_vfp38 | Clcd_tim1_vsw38 | Clcd_tim1_lpp38,
  Clcd_tim1_25     = Clcd_tim1_vbp25 | Clcd_tim1_vfp25 | Clcd_tim1_vsw25 | Clcd_tim1_lpp25,
};

enum
{
  Clcd_tim2_ivs = 1 << 11,
  Clcd_tim2_ihs = 1 << 12,
  Clcd_tim2_cpl84_xga = (1024-1) << 16,
  Clcd_tim2_cpl84_vga = (640-1) << 16,
  Clcd_tim2_cpl38     = (320-1) << 16,
  Clcd_tim2_cpl25     = (240-1) << 16,
  Clcd_tim2_bcd   = 1 << 26,
  Clcd_tim2_84_xga = Clcd_tim2_bcd | Clcd_tim2_cpl84_xga | Clcd_tim2_ihs | Clcd_tim2_ivs,
  Clcd_tim2_84_vga = Clcd_tim2_bcd | Clcd_tim2_cpl84_vga | Clcd_tim2_ihs | Clcd_tim2_ivs,
  Clcd_tim2_38     = Clcd_tim2_bcd | Clcd_tim2_cpl38     | Clcd_tim2_ihs | Clcd_tim2_ivs,
  Clcd_tim2_25     = Clcd_tim2_bcd | Clcd_tim2_cpl25     | Clcd_tim2_ihs | Clcd_tim2_ivs,
};

enum
{
  Clcd_tim3_84_xga = 0,
  Clcd_tim3_84_vga = 0,
  Clcd_tim3_38     = 0,
  Clcd_tim3_25     = 0,
};

enum
{
  Clcd_cntl_lcden              = 1,
  Clcd_cntl_lcdbpp4            = 2 << 1,
  Clcd_cntl_lcdbpp8            = 3 << 1,
  Clcd_cntl_lcdbpp16           = 4 << 1,
  Clcd_cntl_lcdbpp24           = 5 << 1,
  Clcd_cntl_lcdbpp16_pl111_565 = 6 << 1,
  Clcd_cntl_lcdbpp12_pl111_444 = 7 << 1,
  Clcd_cntl_lcdbw              = 0 << 4,
  Clcd_cntl_lcdtft             = 1 << 5,
  Clcd_cntl_lcdbgr             = 1 << 8,
  Clcd_cntl_lcdbebo            = 1 << 9,
  Clcd_cntl_lcdbepo            = 1 << 10,
  Clcd_cntl_lcdpwr             = 1 << 11,
  Clcd_cntl_lcdvcomp           = 1 << 12,
};
