/*
 * (c) 2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <l4/util/util.h>

#include <l4/ferret/gcc_instrument.h>

static void test1(void)
{
    printf("test1\n");
}

static void test2(void)
{
    printf("test2\n");
    test1();
}

int main(void)
{
    ferret_gcc_instrument_init(&malloc, NULL);

    l4_sleep(6000);

    test1();
    test2();

    return 0;
}
