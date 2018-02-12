#ifndef __ARM_DRIVERS__LCD__INCLUDE__LCD_H__
#define __ARM_DRIVERS__LCD__INCLUDE__LCD_H__

#include <l4/sys/types.h>
#include <l4/re/c/video/view.h>

EXTERN_C_BEGIN

struct arm_lcd_ops {
  int          (*probe)(const char *configstr);
  void *       (*get_fb)(void);
  unsigned int (*get_video_mem_size)(void);
  const char * (*get_info)(void);

  int          (*get_fbinfo)(l4re_video_view_info_t *vinfo);

  void         (*enable)(void);
  void         (*disable)(void);
};

struct arm_lcd_ops *arm_lcd_probe(const char *configstr);

void arm_lcd_register_driver(struct arm_lcd_ops *);

/* Callable once per file (should be enough?) */
#define arm_lcd_register(ops)                                        \
    static void __attribute__((constructor)) __register_ops(void)    \
    { arm_lcd_register_driver(ops); }

EXTERN_C_END

#endif /* ! __ARM_DRIVERS__LCD__INCLUDE__LCD_H__ */
