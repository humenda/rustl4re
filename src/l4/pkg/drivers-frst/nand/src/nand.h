#pragma once

#include <string.h>

#include "types.h"
#include <l4/drivers-frst/transfer.h>

struct Op
{};

struct Read_op : Op
{
  u32 addr;
  Transfer *transfer;
};

struct Write_op : Op
{
  u32 addr;
  Transfer *transfer;
};

struct Erase_op : Op
{
  u32 addr;
  u32 len;
  u_char state;
};


struct Dev_desc
{
  int id;
  const char *name;
  unsigned long sz_page; // bytes
  unsigned long sz_spare; // bytes
  unsigned long sz_chip; // MiB
  unsigned long sz_erase; // bytes
  unsigned long options;
};

struct Mfr_desc
{
  int id;
  const char *name;
};

extern Dev_desc _dev_ids[];
extern Mfr_desc _mfr_ids[];

enum
{
  Opt_no_autoincr    = 0x001,
  Opt_buswidth_16    = 0x002,
  Opt_no_padding     = 0x004,
  Opt_cacheprg       = 0x008,
  Opt_copyback       = 0x010,
  Opt_is_and         = 0x020,
  Opt_4page_array    = 0x040,
  /* Chip does not require ready check on read. True
   * for all large page devices, as they do not support autoincrement.*/
  Opt_no_readrdy       = 0x100,
  Opt_no_subpage_write = 0x200,
};


class Nand_ctrl;
class Nand_chip
{
public:
  enum State
    {
      Ready,
      Reading,
      Writing,
      Erasing,
      Cachedprg,
    };

  Nand_chip(Nand_ctrl *ctrl, Dev_desc *dev, Mfr_desc *mfr, int ext_id = 0);

  State state() {return _state; }
  void set_state(State state) { _state = state; }
  void set_state(State state, Op *op)
    {
      _state = state;
      _ongoing_op = op;
    }

  u32 sz_write() const
    { return _sz_write; }
  u32 sz_spare() const
    { return _sz_spare; }
  u32 sz_erase() const
    { return _sz_erase; }
  u64 sz_chip() const
    { return _sz_chip; }
  void add_options(int options)
    { _options |= options; }

  int page_shift() { return ffs(_sz_write) - 1; }
  int page_mask() { return (sz_chip() >> page_shift()) - 1; }
  int erase_shift() { return ffs(_sz_erase) - 1; }
  int erase_mask() { return (sz_chip() >> erase_shift()) - 1; }

  int handle_irq();

protected:
  u32 _sz_write;
  u32 _sz_spare;
  u32 _sz_erase;
  u64 _sz_chip;

  int _bus_width;
  int _delay;
  int _options = 0;

private:
  State _state;
  Op *_ongoing_op;
 
  Nand_ctrl *_ctrl;
  Dev_desc *_dev;
  Mfr_desc *_mfr;
};

class Nand_ctrl
{
protected:
  // commands
  enum
    {
      // standard device commands
      Cmd_read0    = 0x00,
      Cmd_read1    = 0x01,
      Cmd_rndout   = 0x05,
      Cmd_pageprog = 0x10,
      Cmd_readoob  = 0x50,
      Cmd_erase1   = 0x60,
      Cmd_status   = 0x70,
      Cmd_seqin    = 0x80,
      Cmd_rndin    = 0x85,
      Cmd_readid   = 0x90,
      Cmd_erase2   = 0xd0,
      Cmd_reset    = 0xff,

      // large page device commands
      Cmd_readstart   = 0x30,
      Cmd_rndoutstart = 0xe0,
      Cmd_cachedprog  = 0x15,
    };

  /* status bits */
  enum
    {
      Status_fail       = 0x01,
      Status_fail_n1    = 0x02,
      Status_true_ready = 0x20,
      Status_ready      = 0x40,
      Status_wp         = 0x80,
    };

public:
  Nand_ctrl();

  int read(Read_op *op);
  int write(Write_op *op);
  int erase(Erase_op *op);

  int done_read(Read_op *op);
  int done_write(Write_op *op);
  int done_erase(Erase_op *op);
 
  virtual int handle_irq() = 0;

protected:
  int scan(int maxchips);

  virtual void add(Nand_chip *chip) = 0;
  virtual Nand_chip *select(loff_t addr) = 0;
  
  virtual void wr_cmd(u8 c) = 0;
  virtual void wr_adr(u8 a) = 0;
  virtual void wr_dat(u8 d) = 0;

  bool aligned(u32 addr) const
    { return !(addr & (sz_write - 1)); }

  virtual u8 rd_dat() = 0;
  virtual void rd_dat(const u8 *buf, unsigned len) = 0;
  virtual void wr_dat(const u8 *buf, unsigned len) = 0;

  bool is_wp();
  int get_status();
  virtual int get_id(char id[4]);

public:
  const char *name;
  unsigned numchips;
  u64 size;
  u32 sz_erase;
  u32 sz_write;
  u32 sz_spare;
};

