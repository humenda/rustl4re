#pragma once

#include "nand.h"

class Mpc5121 : public Nand_ctrl 
{
  enum
    { Max_cs = 4, };
  enum
    { Max_us_reset = 100, };
  enum
    { Spare_section_len = 64, };

  struct Reg
    {
      u16 _res1[2];
      u16 ram_buffer_addr;
      u16 nand_flash_addr;
      u16 nand_flash_cmd;
      u16 nfc_configuration;
      u16 ecc_status_result1;
      u16 ecc_status_result2;
      u16 spas;
      u16 nf_wr_prot;
      u16 _res2[2];
      u16 nand_flash_wr_pr_st;
      u16 nand_flash_config1;
      u16 nand_flash_config2;
      u16 _res3;
      u16 unlock_start_blk_add0;
      u16 unlock_end_blk_add0;
      u16 unlock_start_blk_add1;
      u16 unlock_end_blk_add1;
      u16 unlock_start_blk_add2;
      u16 unlock_end_blk_add2;
      u16 unlock_start_blk_add3;
      u16 unlock_end_blk_add3;
    };

public:
  Mpc5121(addr base_addr);
  
protected:
  void add(Nand_chip *chip);
  Nand_chip *select(loff_t addr);
  
  void wr_cmd(u8 c);
  void wr_adr(u8 a);
  void wr_dat(u8 d);
  u8 rd_dat();
  void rd_dat(const u8 *buf, unsigned len);
  void wr_dat(const u8 *buf, unsigned len);
  
  int handle_irq();

  int get_id(char id[4]);
 
private:
  void _get_status();
  void _start_read();
  void _start_write();

  void _enable_irq();

  void _copy_from_data(u8 *buf, int len);
  void _copy_to_data(const u8 *buf, int len);
  void _copy_from_spare(u8 *buf, int len);
  void _copy_to_spare(const u8 *buf, int len);

  volatile Reg *_reg;
  Nand_chip *_chips[Max_cs];
  
  volatile u32 *_buffer_main;
  u32 *_buffer_spare;
  u32 _buffer_ptr;
};
