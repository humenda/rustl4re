#pragma once

#include <linux/compiler.h>

long l4x_strncpy_from_user(char *dst, const char *src, long count);

__must_check long l4x_strlen_user(const char __user *str);
__must_check long l4x_strnlen_user(const char __user *str, long n);

unsigned long __must_check
l4x_copy_from_user(void *to, const void __user *from, unsigned long n);

unsigned long __must_check
l4x_copy_to_user(void __user *to, const void *from, unsigned long n);

unsigned long __must_check
l4x_clear_user(void __user *address, unsigned long n);
