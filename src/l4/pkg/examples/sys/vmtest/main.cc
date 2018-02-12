/*
 * Copyright (C) 2015 Kernkonzept GmbH.
 * Author(s): Steffen Liebergeld <steffen.liebergeld@kernkonzept.com>
 *
 * This file is distributed under the terms of the GNU General Public
 * License, version 2.  Please see the COPYING-GPL-2 file for details.
 */
#include <l4/util/cpu.h>

#include "vmx.h"
#include "svm.h"

#include <cstdio>

enum Cpu_vendor
{
  Intel,
  Amd,
  Unknown
};

static Cpu_vendor get_cpu_vendor()
{
  if (!l4util_cpu_has_cpuid())
    return Unknown;

  l4_umword_t ax, bx, cx, dx;
  l4util_cpu_cpuid(0, &ax, &bx, &cx, &dx);

  if (bx == 0x756e6547 && cx == 0x6c65746e && dx == 0x49656e69)
    return Intel;

  if (bx == 0x68747541 && cx == 0x444d4163 && dx == 0x69746e65)
    return Amd;

  return Unknown;
}

int main()
{
  printf("Hello from vmtest\n");

  printf("TAP TEST START\n");
  switch (get_cpu_vendor())
    {
    case Intel:
      {
        Vmx vmxtest;
        vmxtest.run_tests();
      }
      break;
    case Amd:
      {
        Svm svmtest;
        svmtest.run_tests();
      }
      break;
    default:
      printf("# Unknown CPU. Bye.\n");
      return 1;
    }

  printf("TAP TEST FINISH\n");
  return 0;
}

