#include <stdio.h>

#include "l4/drivers-frst/lib_nand.h"

#include "nand.h"
#include "gpmc.h"

Gpmc::Gpmc(addr base_addr)
  : _reg((Reg *)base_addr)
{
  for (int i = 0; i < Num_cs; ++i)
    {
      /* Check if NAND type is set */
      if (((_reg->cs[i].config1) & 0xc00) == 0x800)
	{
	  //printf("found NAND chip\n");
	  /* Found it!! */
	  break;
	}
    }

  // XXX just scan for one chip
  scan(1);

  // disable write protection
  u32 config = _reg->config;
  config |= 0x10;
  _reg->config = config;

  // enable interrupts
  _reg->irqstatus = 256;
  _reg->irqenable = 256;
}

void Gpmc::add(Nand_chip *chip)
{
  chip->add_options(Opt_no_padding | Opt_cacheprg | Opt_no_autoincr);
  if ((_reg->cs[0].config1 & 0x3000) == 0x1000)
    chip->add_options(Opt_buswidth_16);
  //chip->delay = 100;

  _chips[0] = chip;
}

Nand_chip *Gpmc::select(loff_t /*addr*/)
{
  // XXX hack:we currently have only one chip
  return _chips[0];
}

void Gpmc::wr_cmd(u8 c)
{ *((volatile u8 *)&_reg->cs[0].nand_cmd) = c; }

void Gpmc::wr_adr(u8 a)
{ *((volatile u8 *)&_reg->cs[0].nand_adr) = a; }

void Gpmc::wr_dat(u8 d)
{ *((volatile u8 *)&_reg->cs[0].nand_dat) = d; }

u8 Gpmc::rd_dat()
{ return (u8)(*((volatile u16 *)&_reg->cs[0].nand_dat)); }

void Gpmc::rd_dat(const u8 *buf, unsigned len)
{
  u16 *p = (u16 *)buf;
  len >>= 1;

  for (unsigned i = 0; i < len; i++)
    p[i] = *((volatile u16 *)&_reg->cs[0].nand_dat);

#if 0
  for (int i = 0; i < len; i+=4)
    {
      p[i] = *((volatile u16 *)&_reg->cs[0].nand_dat);
      p[i+1] = *((volatile u16 *)&_reg->cs[0].nand_dat);
      p[i+2] = *((volatile u16 *)&_reg->cs[0].nand_dat);
      p[i+3] = *((volatile u16 *)&_reg->cs[0].nand_dat);
    }
#endif
}

void Gpmc::wr_dat(const u8 *buf, unsigned len)
{
  u16 *p = (u16 *)buf;
  len >>= 1;

  for (unsigned i = 0; i < len; i++)
    *((volatile u16 *)&_reg->cs[0].nand_dat) = p[i];
}

int Gpmc::handle_irq()
{
  int ret = 0;
  u32 v = _reg->irqstatus;
  u32 v2 = _reg->irqstatus;
  if (v & 256)
    {
      ret = _chips[0]->handle_irq();
      _reg->irqstatus = 256;
    }
  else
    {
      printf("spurious interrupt\n");
      printf("irqstatus:%x %x\n", v, v2);
      return -1;
    }
  
  return ret;
}

class Gpmc_drv : public Nand_drv
{
public:
  Gpmc_drv()
    { arm_nand_register_driver(this); }
  
  int probe(const char *configstr)
    { return (strcmp(configstr, "GPMC")) ? 0 : 1; }
  
  Nand_ctrl *create(addr base)
    { return new Gpmc(base); }
};

static Gpmc_drv gpmc_drv;
