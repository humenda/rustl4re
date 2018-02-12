#include <l4/drivers-frst/lib_nand.h>
#include "nand.h"

enum { Max_drivers = 10 };
static Nand_drv *nand_drv[Max_drivers];
static int nr_drivers;

Nand *arm_nand_probe(const char *configstr, l4_umword_t base)
{
  for (int i = 0; i < nr_drivers; i++)
    if (nand_drv[i] && nand_drv[i]->probe(configstr))
      return new Nand(nand_drv[i]->create(base));

  return 0;
}

void arm_nand_register_driver(Nand_drv *drv)
{
  if (nr_drivers < Max_drivers)
    nand_drv[nr_drivers++] = drv;
}

Nand::Nand(Nand_ctrl *nand_ctrl)
  : _nand_ctrl(nand_ctrl)
{
  page_size = nand_ctrl->sz_write;
  spare_size = nand_ctrl->sz_spare;
  block_size = nand_ctrl->sz_erase;
  num_blocks = nand_ctrl->size / nand_ctrl->sz_erase;
}

int Nand::read_page(l4_addr_t page, Transfer &transfer)
{
  if (!_nand_ctrl)
    return -1;

  static Read_op op;
  op.addr = page * _nand_ctrl->sz_write;
  op.transfer = &transfer;

  return _nand_ctrl->read(&op);
}

int Nand::write_page(l4_addr_t page, Transfer &transfer)
{
  if (!_nand_ctrl)
    return -1;

  static Write_op op;
  op.addr = page * _nand_ctrl->sz_write;
  op.transfer = &transfer;

  return _nand_ctrl->write(&op);
}

int Nand::erase(l4_addr_t block)
{
  if (!_nand_ctrl)
    return -1;
  
  static Erase_op op;
  op.addr = block * _nand_ctrl->sz_erase;
  op.len = _nand_ctrl->sz_erase;

  _nand_ctrl->erase(&op);
  return 0;
}

int Nand::handle_irq()
{
  if (!_nand_ctrl)
    return -1;

  return _nand_ctrl->handle_irq();
}
