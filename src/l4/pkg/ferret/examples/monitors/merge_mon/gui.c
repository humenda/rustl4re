/**
 * \file   ferret/examples/merge_mon/gui.c
 * \brief  Some GUI related functions
 *
 * \date   13/12/2005
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2005-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <stdio.h>
#include <stdlib.h>

#include <l4/dope/dopelib.h>
#include <l4/util/util.h>

#include <l4/ferret/sensors/list_consumer.h>

#include "buffer.h"
#include "gui.h"
#include "main.h"
//#include "net.h"
#include "uu.h"

#define MERGE_MON_B_START 0
#define MERGE_MON_B_STOP  1
#define MERGE_MON_B_CLEAR 2
#define MERGE_MON_B_EXIT  3
#define MERGE_MON_B_IP    4
#define MERGE_MON_B_LOG   5
int global_exit = 0;

pthread_t gui_thread_id;
extern int verbose;

static void *gui_thread(void * arg)
{
	(void)arg;
    dope_eventloop();
	return NULL;
}

static void press_callback(dope_event  *e, void *arg)
{
    int id = (int)arg;
	(void)e;

    printf("press_callback: %d\n", id);

    switch (id)
    {
    case MERGE_MON_B_START:
        pthread_mutex_lock(&buf.lock);
        buf_set_start_ts(&buf);
        pthread_mutex_unlock(&buf.lock);
        break;
    case MERGE_MON_B_STOP:
        pthread_mutex_lock(&buf.lock);
        buf_set_stop_ts(&buf);
        pthread_mutex_unlock(&buf.lock);
        break;
    case MERGE_MON_B_CLEAR:
        pthread_mutex_lock(&buf.lock);
        buf_clear(&buf);
        pthread_mutex_unlock(&buf.lock);
        break;
    case MERGE_MON_B_EXIT:
        global_exit = 1;
        exit(0);
#if 0
    case MERGE_MON_B_IP:
        pthread_mutex_lock(&buf.lock);
        if (local_ip[0] != '\0')
        {
            char c_ip[16];
            char c_port[6];
            int ret;
            struct in_addr ip;
            unsigned short port;

            ret = l4semaphore_try_down(&net_sem);
            if (ret == 0)
            {
                printf("Network transfer already in progress!\n");
                pthread_mutex_unlock(&buf.lock);
                return;
            }

            dope_req(c_ip, sizeof(c_ip), "e_ip.text");
            dope_req(c_port, sizeof(c_port), "e_port.text");

            printf("Got: '%s':'%s'\n", c_ip, c_port);
            ret = inet_aton(c_ip, &ip);
            if (ret == 0)
            {
                printf("Error parsing IP address: '%s'!\n", c_ip);
                l4semaphore_up(&net_sem);
                pthread_mutex_unlock(&buf.lock);
                return;
            }
            port = atoi(c_port);
            ret = uip_ore_connect(ip, port);
            if (ret)
            {
                printf("Error connecting: %d!\n", ret);
                l4semaphore_up(&net_sem);
                pthread_mutex_unlock(&buf.lock);
                return;
            }
        }
        else
        {
            printf("No local IP address given, not doing anything!\n");
        }
        pthread_mutex_unlock(&buf.lock);
        break;
#endif
    case MERGE_MON_B_LOG:
    {
        static char filename[100];
        pthread_mutex_lock(&buf.lock);
        dope_req(filename, sizeof(filename), "e_log.text");
        printf("Dumping from %p: %hhx %hhx %hhx %hhx...\n", buf.start,
               buf.start[0], buf.start[1], buf.start[2], buf.start[3]);
        uu_dumpz(filename, buf.start, buf.write_pos - buf.start);
        pthread_mutex_unlock(&buf.lock);
        break;
    }
    }
}

int gui_init()
{
    int ret, cnt = 0;

    while ((ret = dope_init()) && cnt++ < 5)
    {
		if (verbose)
			printf("Waiting for DOpE ...\n");
        l4_sleep(200);
    }

    if (ret)
        return -1;

#if 0
    app_id = dope_init_app("Ferret Merge Monitor");
    // fixme: check ret
#endif

    #include "main_window.dpi"

    dope_bind("b_start", "press", press_callback,
              (void *)MERGE_MON_B_START);
    dope_bind("b_stop", "press", press_callback,
              (void *)MERGE_MON_B_STOP);
    dope_bind("b_clear", "press", press_callback,
              (void *)MERGE_MON_B_CLEAR);
    dope_bind("b_exit", "press", press_callback,
              (void *)MERGE_MON_B_EXIT);

    dope_bind("b_tran_ip", "press", press_callback,
              (void *)MERGE_MON_B_IP);
    dope_bind("b_tran_log", "press", press_callback,
              (void *)MERGE_MON_B_LOG);

#if 0
    l4thread_create_named(gui_thread, ".dope", 0,
                          L4THREAD_CREATE_ASYNC);
#else
	ret = pthread_create(&gui_thread_id, NULL, gui_thread, NULL);
	if (ret < 0) {
		printf("Error creating DOpE GUI thread: %d\n", ret);
		return -1;
	}
#endif

    return 0;
}

void gui_update(const buf_t * buf)
{
    int i;
    int min         = 0;
    int max         = buf->size;
    int fill        = buf->write_pos - buf->start;
    int transferred = buf->dump_pos - buf->start;

    if (! use_gui)
        return;

    dope_cmdf("l_fill.set(-from %d -to %d)", min, max);
    dope_cmdf("l_fill.barconfig(transfer, -value %d)", transferred);
    dope_cmdf("l_fill.barconfig(fill, -value %d)", fill - transferred);

    for (i = 0; i < sensor_count; i++)
    {
        if (sensors[i].open)
        {
            dope_cmd("temp = new Label()");
            if (sensors[i].name)
            {
                dope_cmdf(
                    "temp.set(-text \"%s [%hu:%hu:%hu] %llu %llu %u\")",
                    sensors[i].name, sensors[i].major, sensors[i].minor,
                    sensors[i].instance,
                    ((ferret_list_moni_t *)(sensors[i].sensor))->lost,
                    ((ferret_list_moni_t *)(sensors[i].sensor))->next_read,
                    sensors[i].copied);
            }
            else
            {
                dope_cmdf(
                    "temp.set(-text \"[%hu:%hu:%hu] %llu %llu %u\")",
                    sensors[i].major, sensors[i].minor,
                    sensors[i].instance,
                    ((ferret_list_moni_t *)(sensors[i].sensor))->lost,
                    ((ferret_list_moni_t *)(sensors[i].sensor))->next_read,
                    sensors[i].copied);
            }
            dope_cmdf("g_sen.place(temp, -column 0 -row %d)", i);
        }
    }
}
