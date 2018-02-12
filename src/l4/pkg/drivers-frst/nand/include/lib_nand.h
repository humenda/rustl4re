#pragma once

#include <l4/sys/types.h>
#include "transfer.h"

class Nand_ctrl;
class Nand
{
public:
  Nand(Nand_ctrl *nand_ctrl);

  int read_page(l4_addr_t page, Transfer &transfer);
  int write_page(l4_addr_t page, Transfer &transfer);
  int erase(l4_addr_t block);
  int handle_irq();

  long unsigned page_size;
  long unsigned spare_size;
  long unsigned block_size;
  long unsigned num_blocks;

private:
  Nand_ctrl *_nand_ctrl;
};

class Nand_drv
{
public:
  virtual int probe(const char *configstr) = 0;
  virtual Nand_ctrl *create(l4_umword_t base) = 0;
};

struct Nand *arm_nand_probe(const char *configstr, l4_addr_t base);

void arm_nand_register_driver(Nand_drv *nand_drv);

