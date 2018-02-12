/*
 * Some generic functions for input drivers.
 */

#include <l4/input/drv_reg.h>

enum { nr_drivers = 10 };
static struct arm_input_ops *ops[nr_drivers];
static int ops_alloced;

struct arm_input_ops *arm_input_probe(const char *name)
{
  int i;

  for (i = 0; i < ops_alloced; i++)
    if (ops[i]->probe && ops[i]->probe(name))
      return ops[i];

  return NULL;
}

void arm_input_register_driver(struct arm_input_ops *_ops)
{
  if (ops_alloced < nr_drivers)
    ops[ops_alloced++] = _ops;
}
