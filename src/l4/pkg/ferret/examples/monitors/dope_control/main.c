/**
 * \file   ferret/examples/dope_control/main.c
 * \brief  Remote control from DOpE by inserting special control events
 *
 * \date   2006-07-24
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2006-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <l4/dope/dopelib.h>
#include <l4/ferret/client.h>
#include <l4/ferret/maj_min.h>
#include <l4/ferret/sensors/list_producer.h>
#include <l4/ferret/util.h>
#include <l4/util/util.h>


#define BUTTON_START  1
#define BUTTON_STOP   2
#define BUTTON_CLEAR  3
#define BUTTON_SERIAL 4
#define BUTTON_DUMP   5
#define BUTTON_PING   6

long app_id;
ferret_list_local_t * list = NULL;


static void press_callback(dope_event *e, void *arg)
{
    int id = (int)arg;
    int idx;
    ferret_list_entry_common_t * elc;

    printf("Callback %d\n", id);

    switch (id)
    {
    case BUTTON_START:
        idx = ferret_list_dequeue(list);
        elc = (ferret_list_entry_common_t *)ferret_list_e4i(list->glob, idx);
        elc->major = FERRET_MONCON_MAJOR;
        elc->minor = FERRET_MONCON_START;
        ferret_list_commit(list, idx);
        break;
    case BUTTON_STOP:
        idx = ferret_list_dequeue(list);
        elc = (ferret_list_entry_common_t *)ferret_list_e4i(list->glob, idx);
        elc->major = FERRET_MONCON_MAJOR;
        elc->minor = FERRET_MONCON_STOP;
        ferret_list_commit(list, idx);
        break;
    case BUTTON_CLEAR:
        idx = ferret_list_dequeue(list);
        elc = (ferret_list_entry_common_t *)ferret_list_e4i(list->glob, idx);
        elc->major = FERRET_MONCON_MAJOR;
        elc->minor = FERRET_MONCON_CLEAR;
        ferret_list_commit(list, idx);
        break;
    case BUTTON_SERIAL:
        idx = ferret_list_dequeue(list);
        elc = (ferret_list_entry_common_t *)ferret_list_e4i(list->glob, idx);
        elc->major = FERRET_MONCON_MAJOR;
        elc->minor = FERRET_MONCON_SERSEND;
        strcpy(elc->data8, "Test.dat");
        ferret_list_commit(list, idx);
        break;
    case BUTTON_DUMP:
        idx = ferret_list_dequeue(list);
        elc = (ferret_list_entry_common_t *)ferret_list_e4i(list->glob, idx);
        elc->major = FERRET_MONCON_MAJOR;
        elc->minor = FERRET_MONCON_STATS;
        ferret_list_commit(list, idx);
        break;
    case BUTTON_PING:
        idx = ferret_list_dequeue(list);
        elc = (ferret_list_entry_common_t *)ferret_list_e4i(list->glob, idx);
        elc->major = FERRET_MONCON_MAJOR;
        elc->minor = FERRET_MONCON_PING;
        ferret_list_commit(list, idx);
        break;
    }
}


int main(int argc, char* argv[])
{
    int ret;

    ret = ferret_create(FERRET_MONCON_MAJOR, FERRET_MONCON_MINOR, 0,
                        FERRET_LIST, 0, "64:100", list, &malloc);
    if (ret)
    {
        printf("Error creating sensor: ret = %d, giving up.", ret);
        exit(1);
    }

    while ((ret = dope_init()))
    {
        printf("Waiting for DOpE ...\n");
        l4_sleep(200);
    }
    app_id = dope_init_app("Ferret Merge Monitor");

    #include "main_window.i"

    dope_bind(app_id, "b_start", "clack", press_callback,
              (void *)BUTTON_START);
    dope_bind(app_id, "b_stop", "clack", press_callback,
              (void *)BUTTON_STOP);
    dope_bind(app_id, "b_clear", "clack", press_callback,
              (void *)BUTTON_CLEAR);
    dope_bind(app_id, "b_serial", "clack", press_callback,
              (void *)BUTTON_SERIAL);
    dope_bind(app_id, "b_dump", "clack", press_callback,
              (void *)BUTTON_DUMP);
    dope_bind(app_id, "b_ping", "clack", press_callback,
              (void *)BUTTON_PING);

    dope_eventloop(app_id);

    return 0;
}
