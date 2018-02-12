#ifndef _L4_PPC32_OF_DEV_H__
#define _L4_PPC32_OF_DEV_H__

enum
{
  MAX_OF_DEVICES = 32
};

typedef struct of_device_t
{
  char type[32];               //type name
  char name[32];               //device name
  unsigned long reg;           //address
  union {
    struct {
      unsigned long cpu_freq;
      unsigned long bus_freq;
      unsigned long time_freq;
    } freq;
  unsigned long interrupts[3]; //pin, int nr, sense
  };
} of_device_t;

#endif
