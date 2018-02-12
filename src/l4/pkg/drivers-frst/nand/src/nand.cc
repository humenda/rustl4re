#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "common.h"
#include "nand.h"

Nand_chip::Nand_chip(Nand_ctrl *ctrl, Dev_desc *dev, Mfr_desc *mfr, int ext_id)
  : _state(Ready), _ongoing_op(0), _ctrl(ctrl), _dev(dev), _mfr(mfr)
{
  /* New devices have all the information in additional id bytes */
  if (!dev->sz_page)
    {
      /* The 3rd id byte holds MLC / multichip data */
      _sz_write = 1024 << (ext_id & 0x3);
      ext_id >>= 2;
      _sz_spare = (8 << (ext_id & 0x01)) * (_sz_write >> 9);
      ext_id >>= 2;
      _sz_erase = (64 * 1024) << (ext_id & 0x03);
      ext_id >>= 2;
      _bus_width = (ext_id & 0x01) ? Opt_buswidth_16 : 0;
    }
  /* Old devices have chip data hardcoded in the device id table */
  else
    {
      _sz_write = dev->sz_page;
      _sz_spare = dev->sz_spare ? dev->sz_spare : _sz_write / 32;
      _sz_erase = dev->sz_erase;
      _bus_width = dev->options & Opt_buswidth_16;
    }
  _sz_chip = (long long unsigned)dev->sz_chip << 20;

  if (_bus_width != (_options & Opt_buswidth_16))
    {
      printf("NAND bus width %d instead %d bit\n",
	     (_options & Opt_buswidth_16) ? 16 : 8, _bus_width ? 16 : 8);
    }

  _options |= dev->options;
  _options |= Opt_no_autoincr;

  printf("NAND chip: Mfr ID: 0x%02x(%s), Device ID: 0x%02x(%s)\n",
         mfr->id, mfr->name, dev->id, dev->name);
}

int Nand_chip::handle_irq()
{
  switch (_state)
    {
    case Reading:
      return _ctrl->done_read(static_cast<Read_op *>(_ongoing_op));
      break;

    case Writing:
      return _ctrl->done_write(static_cast<Write_op *>(_ongoing_op));
      break;

    case Erasing:
      return _ctrl->done_erase(static_cast<Erase_op *>(_ongoing_op));
      break;

    default:
      _state = Ready;
      break;
    }
  return 0;
}

bool Nand_ctrl::is_wp()
{
  wr_cmd(Cmd_status);
  return (rd_dat() & Status_wp) ? false : true;
}

int Nand_ctrl::get_status()
{
  wr_cmd(Cmd_status);
  return rd_dat();
}

int Nand_ctrl::read(Read_op *op)
{
  Nand_chip *chip = select(op->addr);

  if (!op->transfer->len)
    return -EINVAL;

  u16 col = op->addr & (chip->sz_write() - 1);
  u32 row = (op->addr >> chip->page_shift()) & chip->page_mask();

  assert(!col);
  assert(op->transfer->len + col <= (chip->sz_write() + chip->sz_spare()));

  wr_cmd(Cmd_read0);
  wr_adr(col);
  wr_adr(col >> 8);
  wr_adr(row);
  wr_adr(row >> 8);
  wr_adr(row >> 16);
  wr_cmd(Cmd_readstart);

  chip->set_state(Nand_chip::Reading, op);

  return 0;
}

int Nand_ctrl::done_read(Read_op *op)
{
  Nand_chip *chip = select(op->addr);
  assert(chip->state() == Nand_chip::Reading);

  for (unsigned i = 0; i < op->transfer->num; ++i)
    rd_dat((u8 *)(*op->transfer)[i].addr, (*op->transfer)[i].size);

  chip->set_state(Nand_chip::Ready);
  return 0;
}

int Nand_ctrl::write(Write_op *op)
{
  Nand_chip *chip = select(op->addr);

  if (!op->transfer->len)
    return -EINVAL;

  // check end of device
  if ((op->addr + op->transfer->len) > size)
    return -EINVAL;
  
  // check page aligning
  if (!aligned(op->addr))
    {
      printf("NAND: Attempt to write not page aligned data\n");
      return -EINVAL;
    }

  if (is_wp())
    {
      printf("NAND: Device is write protected\n");
      return -EIO;
    }

  u32 page = (op->addr >> chip->page_shift()) & chip->page_mask();

  wr_cmd(Cmd_seqin);
  wr_adr(0x00);
  wr_adr(0x00);
  wr_adr(page);
  wr_adr(page >> 8);
  wr_adr(page >> 16);

  for (unsigned i = 0; i < op->transfer->num; ++i)
    wr_dat((u8 *)(*op->transfer)[i].addr, (*op->transfer)[i].size);

  wr_cmd(Cmd_pageprog);
  chip->set_state(Nand_chip::Writing, op);
  
  return 0;
}

int Nand_ctrl::done_write(Write_op *op)
{
  Nand_chip *chip = select(op->addr);
  assert(chip->state() == Nand_chip::Writing);
  chip->set_state(Nand_chip::Ready);
  return (get_status() & Status_fail) ? -EIO : 0;
}

int Nand_ctrl::erase(Erase_op *op)
{
  Nand_chip *chip = select(op->addr);

#if 0
  printf("nand: erase: start = 0x%08x, len = %u\n",
      (unsigned int) op->addr, op->len);
#endif

  /* address must align on block boundary */
  if (op->addr & chip->erase_mask())
    {
      printf("nand: erase: Unaligned address\n");
      return -EINVAL;
    }

  /* length must align on block boundary */
  if (op->len & chip->erase_mask())
    {
      printf("nand: erase: Length not block aligned\n");
      return -EINVAL;
    }

  /* Do not allow erase past end of device */
  if ((op->len + op->addr) > size)
    {
      printf("nand: erase: Erase past end of device\n");
      return -EINVAL;
  }

  /* Check, if it is write protected */
  if (is_wp())
    {
      printf("nand_erase: Device is write protected!!!\n");
      return -EIO;
    }

  int page = op->addr >> chip->page_shift();

  wr_cmd(Cmd_erase1);
  wr_adr(page);
  wr_adr(page >> 8);
  wr_adr(page >> 16);
  wr_cmd(Cmd_erase2);

  chip->set_state(Nand_chip::Erasing, op);
  
  return 0;
}

int Nand_ctrl::done_erase(Erase_op *op)
{
  Nand_chip *chip = select(op->addr);
  assert(chip->state() == Nand_chip::Erasing);
  chip->set_state(Nand_chip::Ready);
  return (get_status() & Status_fail) ? -EIO : 0;
}

int Nand_ctrl::get_id(char id[4])
{
  wr_cmd(Cmd_readid);
  wr_adr(0x00);
  wr_dat(0xff);
      
  id[0] = rd_dat();
  id[1] = rd_dat();
  id[2] = rd_dat();
  id[3] = rd_dat();

  return 0;
}

int Nand_ctrl::scan(int maxchips)
{
  for (int i = 0; i < maxchips; ++i)
    {
      wr_cmd(Cmd_reset);
      udelay(100);
      wr_cmd(Cmd_status);
      while (!(rd_dat() & Status_ready));
     
      char id1[4], id2[4];
      get_id(id1);
      get_id(id2);

      if (id1[0] != id2[0] || id1[1] != id2[1])
	{
	  printf("manufacturer or device id corrupt:\n");
	  printf("mfr-id1:%02x mfr-id2:%02x\n", id1[0], id2[0]);
	  printf("dev-id1:%02x dev-id2:%02x\n", id1[1], id2[1]);
	  return -1;
	}

      /* identify manufacturer */
      Mfr_desc *mfr = 0;
      for (int j = 0; _mfr_ids[j].id != 0; j++)
	{
	  if (id1[0] == _mfr_ids[j].id)
	    {
	      mfr = &_mfr_ids[j];
	      break;
	    }
	}

      /* identify device */
      Dev_desc *dev = 0;
      for (int j = 0; _dev_ids[j].name != 0; j++)
	{
	  if (id1[1] == _dev_ids[j].id)
	    {
	      dev = &_dev_ids[j];
	      break;
	    }
	}
      
      if (!dev)
	{
	  printf("no device device found\n");
	  continue;
	}
      if (!mfr)
	{
	  printf("no manufacturer found\n");
	  continue;
	}

      Nand_chip *chip = new Nand_chip(this, dev, mfr, id1[3]);
      add(chip);

      // XXX all chips should have the same features
      sz_write = chip->sz_write();
      sz_spare = chip->sz_spare();
      sz_erase = chip->sz_erase();

      size +=  chip->sz_chip();
      numchips++;
    }

  //printf("%d NAND chips detected\n", numchips);
  
  return 0;
}

Nand_ctrl::Nand_ctrl()
  : size(0)
{}

