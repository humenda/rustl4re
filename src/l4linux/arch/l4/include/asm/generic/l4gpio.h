#ifndef __ASM_L4__GENERIC__L4GPIO_H__
#define __ASM_L4__GENERIC__L4GPIO_H__

const char *l4gpio_dev_name(unsigned group_nr);
int l4gpio_config_pin(unsigned pin, unsigned func, unsigned value);
int l4gpio_multi_config_pin(unsigned gpiochip, unsigned pinmask,
                            unsigned func, unsigned value);

#endif /* ! __ASM_L4__GENERIC__L4GPIO_H__ */
