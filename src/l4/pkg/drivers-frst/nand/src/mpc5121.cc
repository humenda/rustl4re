#include <stdio.h>
#include <stdlib.h>

#include "l4/drivers-frst/lib_nand.h"

#include "common.h"
#include "nand.h"
#include "mpc5121.h"

Mpc5121::Mpc5121(addr base_addr)
  : _reg((Reg *)(base_addr + 0x1e00)),
    _buffer_main((u32 *)base_addr), _buffer_spare((u32 *)(base_addr + 0x1000))
{
  // reset controller
  _reg->nand_flash_config1 |= 0x40;
  unsigned num_us_reset = 0;
  while (_reg->nand_flash_config1 & 0x40)
    {
      if (num_us_reset++ >= Max_us_reset)
	{
	  printf("[Mpc5121] Timeout while resetting controller!\n");
	  return;
	}

      udelay(1);
    }

  // unlock buffer memory
  _reg->nfc_configuration = 0x2;

  // disable write protection
  _reg->unlock_start_blk_add0 = 0x0000;
  _reg->unlock_end_blk_add0 = 0xffff;
  _reg->nf_wr_prot = 0x4;

  // mask and configure interrupts only after full page transfer
  _reg->nand_flash_config1 = 0x810;

  // detect NAND chips
  if (scan(1))
    {
      printf(": NAND Flash not found !\n");
      return;
    }

  // set spare area size
  _reg->spas = sz_spare >> 1;

  // set erase block size
  switch (sz_erase / sz_write)
    {
    case 32:
      _reg->nand_flash_config1 |= 0x000;
      break;

    case 64:
      _reg->nand_flash_config1 |= 0x200;
      break;

    case 128:
      _reg->nand_flash_config1 |= 0x400;
      break;

    case 256:
      _reg->nand_flash_config1 |= 0x600;
      break;

    default:
      printf("[Mpc5121] Unsupported NAND flash chip!\n");
    }

  return;
}

void Mpc5121::add(Nand_chip *chip)
{
  chip->add_options(Opt_no_autoincr);
  _chips[0] = chip;
}

Nand_chip *Mpc5121::select(loff_t /*addr*/)
{
  return _chips[0];
}

void Mpc5121::wr_cmd(u8 c)
{
  switch (c)
    {
    case Cmd_pageprog:
      _start_write();
      _enable_irq();
      break;

    case Cmd_erase2:
      _enable_irq();
      break;

    default:
      break;
    }

  _reg->nand_flash_cmd = c;
  _reg->nand_flash_config2 = 0x1;

  // wait for completion
  if ((c != Cmd_pageprog) && (c != Cmd_erase2))
    // XXX udelay
    while (_reg->nand_flash_config2 & 0x1) ;

  switch (c)
    {
    case Cmd_status:
      _get_status();
      break;

    case Cmd_readstart:
      _start_read();
      break;

    case Cmd_seqin:
      _buffer_ptr = 0;
      break;

    default:
      break;
    }
}

void Mpc5121::wr_adr(u8 a)
{
  _reg->nand_flash_addr = a;
  _reg->nand_flash_config2 = 0x2;
  
  // XXX udelay
  while (_reg->nand_flash_config2 & 0x2) ;
}

void Mpc5121::wr_dat(u8 d)
{ _buffer_main[0] = d; }

u8 Mpc5121::rd_dat()
{ return _buffer_main[0]; }

void Mpc5121::rd_dat(const u8 *buf, unsigned len)
{
  while (len && (_buffer_ptr < (sz_write + sz_spare)))
    {
      if (_buffer_ptr < sz_write)
	{
	  int l = len < sz_write ? len : sz_write;
	  _copy_from_data((u8 *)buf, l);
	  _buffer_ptr += l;
	  len -= l;
	}
      else if (_buffer_ptr < (sz_write + sz_spare))
	{
	  int l = len < sz_spare ? len : sz_spare;
	  _copy_from_spare((u8 *)buf, l);
	  _buffer_ptr += l;
	  len -= l;
	}
    }
 
}

void Mpc5121::wr_dat(const u8 *buf, unsigned len)
{
  while (len && (_buffer_ptr < (sz_write + sz_spare)))
    {
      if (_buffer_ptr < sz_write)
	{
	  int l = len < sz_write ? len : sz_write;
	  _copy_to_data(buf, l);
	  _buffer_ptr += l;
	  len -= l;
	}
      else if (_buffer_ptr < (sz_write + sz_spare))
	{
	  int l = len < sz_spare ? len : sz_spare;
	  _copy_to_spare(buf, l);
	  _buffer_ptr += l;
	  len -= l;
	}
    }
  
  unsigned sum = 0;
  for (int i = 0; i < 4096/4; i++)
    sum += _buffer_main[i];
  printf("buffer sum:%d\n", sum);
}

static void spare_copy(u32 *dst, const u32 *src, int len)
{
  for (int i = 0; i < (len/4); ++i)
    dst[i] = src[i];

  switch (len % 4)
    {
    case 1:
      dst[len/4] = (dst[len/4] & 0xffffff00) | (src[len/4] & 0xff);
      break;

    case 2:
      dst[len/4] = (dst[len/4] & 0xffff0000) | (src[len/4] & 0xffff);
      break;

    case 3:
      dst[len/4] = (dst[len/4] & 0xff000000) | (src[len/4] & 0xffffff);
      break;

    default:
      break;
    }
}

void Mpc5121::_copy_from_data(u8 *buf, int len)
{
  u32 *p = (u32 *)buf;
  len >>= 2;
  
  for (int i = 0; i < len; i++)
    p[i] = _buffer_main[i];
}

void Mpc5121::_copy_to_data(const u8 *buf, int len)
{
  u32 *p = (u32 *)buf;
  len >>= 2;
  
  for (int i = 0; i < len; i++)
    _buffer_main[i] = p[i];
}

void Mpc5121::_copy_from_spare(u8 *buf, int len)
{
  int section_len = (sz_spare / (sz_write >> 9) >> 1) << 1;

  unsigned i = 0;
  for (i = 0; i < (sz_write >> 9) - 1; ++i)
    spare_copy((u32 *)&buf[i * Spare_section_len], &_buffer_spare[(i * Spare_section_len)/4], section_len);
  spare_copy((u32 *)&buf[i * Spare_section_len], &_buffer_spare[(i * Spare_section_len)/4], len - i * section_len);
}

void Mpc5121::_copy_to_spare(const u8 *buf, int len)
{
  int section_len = (sz_spare / (sz_write >> 9) >> 1) << 1;

  unsigned i = 0;
  for (i = 0; i < (sz_write >> 9) - 1; ++i)
    spare_copy(&_buffer_spare[i * Spare_section_len/4], (u32 *)&buf[i * Spare_section_len], section_len);
  spare_copy(&_buffer_spare[i * Spare_section_len/4], (u32 *)&buf[i * Spare_section_len], len - i * section_len);
}

void Mpc5121::_get_status()
{
  _reg->nand_flash_config2 = 0x20;
  while (_reg->nand_flash_config2 & 0x20) ;
}

int Mpc5121::get_id(char id[4])
{
  wr_cmd(Cmd_readid);
  wr_adr(0x00);
  
  // transfer data
  _reg->nand_flash_config2 = 0x10;
  while (_reg->nand_flash_config2 & 0x10) ;

  id[0] = _buffer_main[0];
  id[1] = _buffer_main[0] >> 8;
  id[2] = _buffer_main[1];
  id[3] = _buffer_main[1] >> 8;

  return 0;
}

void Mpc5121::_start_read()
{
  // clear interrupt status
  _reg->nand_flash_config2 = 0x0;
  // unmask interrupt
  _reg->nand_flash_config1 &= ~0x10;
  // start data transfer
  _reg->nand_flash_config2 = 0x8;
  // reset buffer ptr
  _buffer_ptr = 0;
}

void Mpc5121::_start_write()
{
  // transfer data to chip
  _reg->nand_flash_config2 = 0x4;
  while (_reg->nand_flash_config2 & 0x4) ;
}

void Mpc5121::_enable_irq()
{
  // clear interrupt status
  _reg->nand_flash_config2 = 0x0;
  // unmask interrupt
  _reg->nand_flash_config1 &= ~0x10;
}

int Mpc5121::handle_irq()
{
  // mask interrupt
  _reg->nand_flash_config1 |= 0x10;
  _chips[0]->handle_irq();
  return 0;
}

class Mpc5121_drv : public Nand_drv
{
public:
  Mpc5121_drv()
    { arm_nand_register_driver(this); }
  
  int probe(const char *configstr)
    { return (strcmp(configstr, "MPC5121")) ? 0 : 1; }
  
  Nand_ctrl *create(addr base)
    { return new Mpc5121(base); }
};

static Mpc5121_drv mpc5121_drv;
