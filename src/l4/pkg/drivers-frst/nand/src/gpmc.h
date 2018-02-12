#pragma once

#include "nand.h"


class Gpmc : public Nand_ctrl 
{
  enum
    { Num_cs = 8, };

  struct Reg_cs
    {
      u32 config1;
      u32 config2;
      u32 config3;
      u32 config4;
      u32 config5;
      u32 config6;
      u32 config7;
      volatile u32 nand_cmd;
      volatile u32 nand_adr;
      volatile u32 nand_dat;
    };

  struct Reg
    {
      u8  res1[0x10];
      u32 sysconfig;              
      u8  res2[0x04];
      u32 irqstatus;              
      u32 irqenable;      
      u8  res3[0x20];
      u8  timeout_control;        
      u8  res4[0x0c];
      u32 config;         
      u32 status;         
      u8 res5[0x08];
      Reg_cs cs[Num_cs];
    };

public:
  Gpmc(addr base_addr);

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

private:
  volatile Reg *_reg;
  Nand_chip *_chips[Num_cs];
};
