/**
 * \file   ferret/examples/l4lx_control/main.c
 * \brief  Remote control from L4Linux by inserting special control events
 *
 * \date   2006-06-14
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2006-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#define _GNU_SOURCE
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <l4/ferret/l4lx_client.h>
#include <l4/ferret/maj_min.h>
#include <l4/ferret/sensors/list_producer.h>
#include <l4/ferret/util.h>

static void usage(void)
{
    printf("usage: fer_l4lx_ctrl [exactly one option]\n"
           "  -b,--begin ......... Start recording now\n"
           "  -e,--end ........... Stop recording now\n"
           "  -c,--clear ......... Clear recording buffer now\n"
           "  -d,--dump .......... Dump some statistics in the monitor\n"
           "  -p,--ping .......... Tell the monitor to give a sign of life\n"
           "  -n,--net <ip:port> . Transfer recording buffer now to <ip:port>\n"
           "  -s,--serial <name> . Dump recording buffer now (uuencoded) over serial line\n"
           "example: fer_l4lx_ctrl -b\n"
           "example: fer_l4lx_ctrl -n 1.2.3.4:1234\n"
           "example: fer_l4lx_ctrl -s testing.dat\n"
        );
}

int main(int argc, char* argv[])
{
    int idx;
    ferret_list_entry_common_t * elc;
    ferret_list_l4lx_user_init();

    // parse parameters
    {
        int optionid;
        int option = 0;
        const struct option long_options[] =
            {
                { "begin",     0, NULL, 'b'},
                { "end",       0, NULL, 'e'},
                { "clear",     0, NULL, 'c'},
                { "net",       1, NULL, 'n'},
                { "serial",    1, NULL, 's'},
                { "dump",      0, NULL, 'd'},
                { "ping",      0, NULL, 'p'},
                { "help",      0, NULL, 'h'},
                { 0, 0, 0, 0}
            };

	if (argc < 2)
	{
            usage();
            exit(1);
	}

        do
        {
            option = getopt_long(argc, argv, "becn:s:hdp",
                                 long_options, &optionid);
            switch (option)
            {
            case 'h':
                usage();
                exit(0);
            case 'b':
                idx = ferret_list_dequeue(ferret_l4lx_user);
                elc = (ferret_list_entry_common_t *)
                    ferret_list_e4i(ferret_l4lx_user->glob, idx);
                elc->major     = FERRET_MONCON_MAJOR;
                elc->minor     = FERRET_MONCON_START;
                ferret_list_commit(ferret_l4lx_user, idx);
                break;
            case 'e':
                idx = ferret_list_dequeue(ferret_l4lx_user);
                elc = (ferret_list_entry_common_t *)
                    ferret_list_e4i(ferret_l4lx_user->glob, idx);
                elc->major     = FERRET_MONCON_MAJOR;
                elc->minor     = FERRET_MONCON_STOP;
                ferret_list_commit(ferret_l4lx_user, idx);
                break;
            case 'c':
                idx = ferret_list_dequeue(ferret_l4lx_user);
                elc = (ferret_list_entry_common_t *)
                    ferret_list_e4i(ferret_l4lx_user->glob, idx);
                elc->major     = FERRET_MONCON_MAJOR;
                elc->minor     = FERRET_MONCON_CLEAR;
                ferret_list_commit(ferret_l4lx_user, idx);
                break;
            case 's':
                idx = ferret_list_dequeue(ferret_l4lx_user);
                elc = (ferret_list_entry_common_t *)
                    ferret_list_e4i(ferret_l4lx_user->glob, idx);
                elc->major     = FERRET_MONCON_MAJOR;
                elc->minor     = FERRET_MONCON_SERSEND;
                strncpy(elc->data8, optarg, 48);
                printf("Sending name: %s\n", optarg);
                elc->data8[47] = '\0';
                ferret_list_commit(ferret_l4lx_user, idx);
                break;
            case 'n':
            {
                uint16_t port;
                struct in_addr ip;
                char * sep;
                char * aip;

                idx = ferret_list_dequeue(ferret_l4lx_user);
                elc = (ferret_list_entry_common_t *)
                    ferret_list_e4i(ferret_l4lx_user->glob, idx);
                elc->major     = FERRET_MONCON_MAJOR;
                elc->minor     = FERRET_MONCON_NETSEND;
                sep = index(optarg, ':');
                if (sep == NULL)
                {
                    printf("Error parsing <ip:port>: '%s'\n", optarg);
                    exit(2);
                }
                port = atoi(sep + 1);
                aip = strndupa(optarg, sep - optarg);
                if (inet_aton(aip, &ip) == 0)
                {
                    printf("Error parsing <ip>: '%s'\n", aip);
                    exit(3);
                }
                printf("Parsed address: '%s:%hu'\n", aip, port);
                elc->data16[0] = port;
                memcpy(&(elc->data16[1]), &ip, sizeof(ip));
                ferret_list_commit(ferret_l4lx_user, idx);
                break;
            }
            case 'd':
                idx = ferret_list_dequeue(ferret_l4lx_user);
                elc = (ferret_list_entry_common_t *)
                    ferret_list_e4i(ferret_l4lx_user->glob, idx);
                elc->major     = FERRET_MONCON_MAJOR;
                elc->minor     = FERRET_MONCON_STATS;
                ferret_list_commit(ferret_l4lx_user, idx);
                break;
            case 'p':
                idx = ferret_list_dequeue(ferret_l4lx_user);
                elc = (ferret_list_entry_common_t *)
                    ferret_list_e4i(ferret_l4lx_user->glob, idx);
                elc->major     = FERRET_MONCON_MAJOR;
                elc->minor     = FERRET_MONCON_PING;
                ferret_list_commit(ferret_l4lx_user, idx);
                break;

            case -1:  // exit case
                break;
            default:
                printf("error  - unknown option %c\n", option);
                usage();
                exit(1);
            }
        } while (option != -1);
    }
    return 0;
}
