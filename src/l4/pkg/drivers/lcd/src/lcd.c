/*
 * Some generic functions for LCD drivers.
 */

#include <l4/drivers/lcd.h>

enum { nr_drivers = 10 };
static struct arm_lcd_ops *ops[nr_drivers];
static int ops_alloced;

struct arm_lcd_ops *arm_lcd_probe(const char *configstr)
{
  int i;

  for (i = 0; i < ops_alloced; i++)
    if (ops[i]->probe && ops[i]->probe(configstr))
      return ops[i];

  return NULL;
}

void arm_lcd_register_driver(struct arm_lcd_ops *_ops)
{
  if (ops_alloced < nr_drivers)
    ops[ops_alloced++] = _ops;
}
