#pragma once

#define ANKH 1
#define CONFIG_TSC_CURRTICKS 1

#include <stdlib.h>

void *memset(void *s, int c, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);

// grub provides its own libc stuff, we just use
// the original functions
#define grub_memset memset
#define grub_printf printf
#define grub_putchar putchar

#define etherboot_printf	printf
#define etherboot_sprintf	sprintf
#define etherboot_vsprintf	vsprintf
