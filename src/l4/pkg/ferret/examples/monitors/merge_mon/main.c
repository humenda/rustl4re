/**
 * \file   ferret/examples/merge_mon/main.c
 * \brief  This monitor consumes events from several lists and merges
 *         them into a large buffer, which later can be transfered and
 *         evaluated offline.
 *
 * \date   11/12/2005
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2005-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <alloca.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <pthread.h>

#include <l4/util/l4_macros.h>
#include <l4/util/rand.h>
#include <l4/util/util.h>
#include <l4/sys/kdebug.h>

#include <l4/ferret/monitor.h>
#include <l4/ferret/types.h>
#include <l4/ferret/util.h>
#include <l4/ferret/comm.h>
#include <l4/ferret/sensors/list_consumer.h>
//#include <l4/ferret/sensors/tbuf_consumer.h>

#include "buffer.h"
#include "gui.h"
#include "main.h"
//#include "net.h"
#include "poll.h"

char LOG_tag[9] = "FerMeMo";

sensor_entry_t sensors[MAX_SENSORS];
int sensor_count = 0;
char local_ip[16] = "";    // local IP address in text form
//unsigned short local_port = 9000;   // local IP port

static int interval = 1000;
//static char * sensor_dir_basepath = NULL;
int verbose = 0;
static int delay = 0;
int use_gui = 1;
int dump_after = 0;

// buffer data structures
buf_t buf = {NULL, 10000 * 1024, NULL, NULL, PTHREAD_MUTEX_INITIALIZER,
             0ULL, 0ULL, 0, 0};

static void usage(void)
{
    printf(
        "usage: fer_merge_mon [-b val] [-s val]* [-m val] [-i val] [-v]"
        " [-d val]\n"
        "\n"
#if 0
        "  -b,--base ....... Base path name to the sensor directory.\n"
#endif
        "  -s,--sensor ..... Sensor to open:\n"
        "                    This can be either a major:minor:instance \n"
        "                    triple of numbers or a filename denoting string\n"
        "                    representing the sensor.\n"
        "                    This option can be given several times if your\n"
        "                    want to listen to several sensors..\n"
        "  -m,--memory ..... Amount of memory to use for sensor data buf.\n"
        "                    (in KBytes), defaults to 10 MBytes\n"
        "  -i,--interval ... Internal polling interval (in msec), defaults\n"
        "                    to 1000ms\n"
        "  -a,--dump-after...UU-Dump the event buffer after N iterations of the\n"
        "                    polling loop\n"
        "  -d,--delay ...... Waiting delay after program start until sensor\n"
        "                    directory is asked (in msec), defaults to 0 msec\n"
#if 0
        "  -l,--local_ip ... Local IP address to use (n.n.n.n where n are \n"
        "                    independent decimals 0...255)\n"
        "  -p,--local_port . Local IP port to use (default 9000)\n"
#endif
        "  -v,--verbose .... Be verbose\n"
        "  -g,--nogui ...... Don't use DOpE Gui, only work in background,\n"
        "                    controlling works via special control events\n"
        "  -o,--offline .... Start offline (not recording events)\n"
        "  -c,--cyclic ..... Autoclear buffer if full (pseudo ring buffer)\n"
        "                    and record from the start again.\n"
        "example: fer_merge_mon -b /servers/ferret/ -s 0:0:0 -s 10:2:1\n"
        " --sensor 10:2:3 -s verner/decoder/cpu -m 20000 -i 100\n"
        "This will open four sensors relative to '/servers/ferret/' and use 20\n"
        " MBytes of memory for storage.  The polling interval is 100 msec.");
}


int main(int argc, char* argv[])
{
    /* 1. parse command line for:
     *   - list of sensors to open,
     *   - polling interval,
     *   - memory buffer size
     * 2. have a GUI with: start, stop, destination ip:port, transfer
     *    button, visual fill level indicator, visual 'lost counter' for
     *    each sensor
     * 3. loop over all sensors and merge event into prefix-length
     *    encoded memory region (convert kernel event to common format)
     * 4. transfer data on request
     */

    int i;
    int ret;
    unsigned int max_size = 0;
#if 0
    int outstanding_sensors = 0;
#endif
    l4_cap_idx_t srv = lookup_sensordir();

    if (l4_is_invalid_cap(srv)) {
        printf("Could not find sensor directory. Exiting.\n");
        exit(1);
    }

    // parse parameters
    {
        int optionid;
        int option = 0;
        const struct option long_options[] =
            {
                { "sensor",     1, NULL, 's'},
                { "memory",     1, NULL, 'm'},
                { "base",       1, NULL, 'b'},
                { "interval",   1, NULL, 'i'},
                { "dump-after", 1, NULL, 'a'},
                { "delay",      1, NULL, 'd'},
                { "verbose",    0, NULL, 'v'},
                { "local_ip",   1, NULL, 'l'},
                { "local_port", 1, NULL, 'p'},
                { "nogui",      0, NULL, 'g'},
                { "offline",    0, NULL, 'o'},
                { "cyclic",     0, NULL, 'c'},
                { 0, 0, 0, 0}
            };

        do
        {
            option = getopt_long(argc, argv, "s:m:b:i:vd:l:goc",
                                 long_options, &optionid);
            switch (option)
            {
            case 's':
                if (optarg[0] >= '0' && optarg[0] <= '9')  // mmi tripple
                {
                    ret = sscanf(optarg, "%hu:%hu:%hu",
                                 &sensors[sensor_count].major,
                                 &sensors[sensor_count].minor,
                                 &sensors[sensor_count].instance);
                    if (ret != 3)
                    {
                        printf("Error parsing string '%s' for tripple\n",
                               optarg);
                        exit(3);
                    }
                }
                else                                       // filename
                {
                    sensors[sensor_count].name = strdup(optarg);
                }
                // debug
                sensor_count++;
                break;
            case 'm':
                buf.size = atoi(optarg) * 1024;
                if (buf.size < 1024)
                {
                    printf("Wrong memory size specified: '%s', should be an"
                           " integer greater than 0!", optarg);
                    exit(1);
                }
                break;
#if 0
            case 'b':
                sensor_dir_basepath = strdup(optarg);
                if (strlen(sensor_dir_basepath) == 0 ||
                    sensor_dir_basepath[0] != L4VFS_PATH_SEPARATOR)
                {
                    printf("Wrong base path name specified: '%s', should be an"
                           " absolute path (starting with %c)!",
                           optarg, L4VFS_PATH_SEPARATOR);
                    exit(1);
                }
                break;
#endif
            case 'i':
                interval = atoi(optarg);
                if (interval < 1)
                {
                    printf("Wrong interval specified: '%s', should be an"
                           " integer greater than 0!", optarg);
                    exit(1);
                }
                break;
            case 'a':
                dump_after = atoi(optarg);
                printf("dumping buffer after %d iterations.\n", dump_after);
                break;
            case 'd':
                delay = atoi(optarg);
                if (delay < 1)
                {
                    printf("Wrong delay specified: '%s', should be an"
                           " integer greater than 0!", optarg);
                    exit(1);
                }
                break;
#if 0
            case 'p':
                local_port = atoi(optarg);
                break;
            case 'l':
            {
                unsigned char a, b, c, d;
                ret = sscanf(optarg, "%hhu.%hhu.%hhu.%hhu", &a, &b, &c, &d);
                if (ret != 4)
                {
                    printf("Error parsing IP address: '%s', should be like"
                           " '1.2.3.4'!", optarg);
                    exit(1);
                }
                strcpy(local_ip, optarg);
                break;
            }
#endif
            case 'v':
                verbose = 1;
                break;
            case 'g':
                use_gui = 0;
                break;
            case 'o':
                buf_set_offline(&buf);
                break;
            case 'c':
                buf_set_cyclic(&buf);
                break;
            case -1:  // exit case
                break;
            default:
                printf("error  - unknown option %c\n", option);
                usage();
                return 2;
            }
        } while (option != -1);
    }

    // dump what we got
    if (verbose)
    {
        printf("Sensors requested (%d):\n", sensor_count);
        for (i = 0; i < sensor_count; ++i)
        {
            printf("  %s: %hu, %hu, %hu\n", sensors[i].name,
                   sensors[i].major, sensors[i].minor, sensors[i].instance);
        }
    }

    // delay
    if (verbose)
    {
        printf("Sleeping for %d msec\n", delay);
    }
    l4_sleep(delay);

    // allocate required memory
#if 0
    buf.start = (unsigned char *)l4dm_mem_allocate(
        buf.size, L4RM_MAP + L4DM_PINNED + L4DM_CONTIGUOUS);
#else
    // shouldn't plain malloc() suffice?
    buf.start = malloc(buf.size);
    pthread_mutex_init(&buf.lock, NULL);
#endif
    if (buf.start == NULL)
    {
        printf("Error allocating memory: %zd!\n", buf.size);
        exit(2);
    }
    if (verbose)
    {
        printf("Allocated %zd Bytes for sensor data at %p.\n",
               buf.size, buf.start);
    }

    // open stuff
    for (i = 0; i < sensor_count; ++i)
    {
        if (sensors[i].name)
        {
            printf("Found name entry: '%s', not yet supported, ignored!\n",
                   sensors[i].name);
        }
        else
        {
            ret = ferret_att(srv, sensors[i].major, sensors[i].minor,
                             sensors[i].instance, sensors[i].sensor);
            if (ret)
            {
                printf("Could not attach to %hu:%hu:%hu, trying again later!\n",
                       sensors[i].major, sensors[i].minor,
                       sensors[i].instance);
#if 0
                outstanding_sensors = 1;
#endif
            }
            else
            {
                uint16_t type;

                sensors[i].open      = 1;
                sensors[i].copied    = 0;
                sensors[i].last_lost = 0;
                sensors[i].last_ts   = 0;

                type = ((ferret_common_t *)(sensors[i].sensor))->type;
                if (type == FERRET_LIST)
                {
                    max_size = MAX(max_size,
                                   ((ferret_list_moni_t *)
                                    (sensors[i].sensor))->glob->element_size);
                }
#if 0
                else if (type == FERRET_TBUF)
                {
                    max_size = MAX(max_size, sizeof(l4_tracebuffer_entry_t));
                }
#endif
                else
                {
                    printf("Found wrong sensor type (%d): %hu!\n", i, type);
                }

                if (verbose)
                {
                    printf("Attached to %hu:%hu:%hu.\n",
                           sensors[i].major, sensors[i].minor,
                           sensors[i].instance);
                    printf("New maximal element size is %d\n", max_size);
                }
            }
        }
    }
    buf_clear(&buf);

    if (verbose)
    {
        printf("Maximal element size is %d\n", max_size);
    }

#if 0
    // from here on we run multithreaded
    // startup uip helper thread
    if (local_ip[0] != '\0')
    {
        net_init(local_ip);
    }
#endif

    // startup gui stuff with helper thread
    if (use_gui) {
        if ((ret = gui_init()))
            printf("Could not initialize GUI: %d, continuing without it.\n", ret);
    }

#if 0
    // If we have sensors that could not be opened use a polling
    // thread to check for them periodically.
    if (outstanding_sensors)
    {
        poll_sensors();
        // so, we don't know how sensor size will be later on, so we
        // use a safe bet here
        max_size = MAX_EVENT_SIZE;
    }
#endif

    ret = buf_fill(&buf, max_size, interval, verbose);
    return 0;
}
