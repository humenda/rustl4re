/**
 * \file   ferret/examples/l4linux_user/main.c
 * \brief  Example demonstrating the usage of lists inside of L4Linux.
 *
 * \date   2006-03-28
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2006-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <l4/ferret/l4lx_client.h>
#include <l4/ferret/sensors/list_producer.h>
#include <l4/ferret/util.h>

int main(int argc, char* argv[])
{
    int i, j, loops, ret;

    // -=# monitoring code start #=-
    ferret_list_entry_common_t * elc;
    ferret_list_entry_t * el;
    int index;

    ferret_list_l4lx_user_init();
    // -=# monitoring code end #=-

    /*
    printf("Sensor @: %p\n", ferret_l4lx_user);
    printf("Global sensor @: %p, mmi: %hd:%hd:%hd\n",
           ferret_l4lx_user->glob,
           ferret_l4lx_user->glob->header.major,
           ferret_l4lx_user->glob->header.minor,
           ferret_l4lx_user->glob->header.instance);
    */

    j = 0;
    while (1)
    {
        //fprintf(stderr, ".");

        // -=# monitoring code start #=-
        // this demonstrates to cast the received pointer to a certain
        // struct and fill it
        index = ferret_list_dequeue(ferret_l4lx_user);
        elc = (ferret_list_entry_common_t *)
            ferret_list_e4i(ferret_l4lx_user->glob, index);
        elc->major     = 100;
        elc->minor     = 10;
        elc->instance  = 0;
        elc->cpu       = 0;
        elc->data32[0] = 1;  // start
        elc->data32[1] = j;
        ferret_list_commit(ferret_l4lx_user, index);
        // -=# monitoring code end #=-

        // do some work
        loops = 10000 + ((double)rand() * 65536 / RAND_MAX);
        for (i = 0; i < loops; i++)
            asm volatile ("": : : "memory");

        // -=# monitoring code start #=-
        // here, dynamic marshaling is demonstrated
        index = ferret_list_dequeue(ferret_l4lx_user);
        el = ferret_list_e4i(ferret_l4lx_user->glob, index);
        ret = ferret_util_pack("hhhbxlll", el->data, 100, 10, 0, 0, 2, j, i);
        ferret_list_commit(ferret_l4lx_user, index);
        // -=# monitoring code end #=-

        usleep(10000);
        j++;
    }

    return 0;
}
