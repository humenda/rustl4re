
#include <l4/drivers/io_regblock.h>
#include <l4/drivers/amba.h>


void amba_read_id(l4_addr_t address, uint32_t *periphid, uint32_t *cellid)
{
  L4::Io_register_block_mmio b(address);

  *periphid =   ((b.read32( 0) & 0xff) << 0)
              | ((b.read32( 4) & 0xff) << 8)
              | ((b.read32( 8) & 0xff) << 16)
              | ((b.read32(12) & 0xff) << 24);

  *cellid   =   ((b.read32(16) & 0xff) << 0)
              | ((b.read32(20) & 0xff) << 8)
              | ((b.read32(24) & 0xff) << 16)
              | ((b.read32(28) & 0xff) << 24);
}
