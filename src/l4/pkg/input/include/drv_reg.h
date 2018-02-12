#pragma once

#include <l4/sys/types.h>

EXTERN_C_BEGIN

typedef struct {
    short unsigned type;
    short unsigned code;
    int value;
} Input_event;

typedef void (*Input_handler)(Input_event event, void *priv);

struct arm_input_ops {
  const char * (*get_info)(void);
  int  (*probe)(const char *name);
  void (*attach)(Input_handler handler, void *priv);
  void (*enable)(void);
  void (*disable)(void);
};

struct arm_input_ops *arm_input_probe(const char *name);

void arm_input_register_driver(struct arm_input_ops *);

/* Callable once per file (should be enough?) */
#define arm_input_register(ops)                                        \
    static void __attribute__((constructor)) __register_ops(void)    \
    { arm_input_register_driver(ops); }

EXTERN_C_END
