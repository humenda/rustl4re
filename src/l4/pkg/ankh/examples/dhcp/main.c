#include <l4/ankh/client-c.h>
#include <l4/ankh/netboot.h>
#include <l4/ankh/session.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread-l4.h>
#include <arpa/inet.h>

#define GETOPT_LIST_END { 0, 0, 0, 0 }

enum options
{
	BUF_SIZE,
	SHM_NAME,
};

static int bufsize = 1024;
static char *shm_name = "";


int main(int argc, char **argv)
{
	if (l4ankh_init())
	  return 1;
	l4_cap_idx_t c = pthread_l4_cap(pthread_self());

	static struct option long_opts[] = {
		{"bufsize", 1, 0, BUF_SIZE },
		{"shm", 1, 0, SHM_NAME },
		GETOPT_LIST_END
	};

	while (1) {
		int optind = 0;
		int opt = getopt_long(argc, argv, "b:s:", long_opts, &optind);
		printf("getopt: %d\n", opt);

		if (opt == -1)
			break;

		switch(opt) {
			case BUF_SIZE:
				printf("buf size: %d\n", atoi(optarg));
				bufsize = atoi(optarg);
				break;
			case SHM_NAME:
				printf("shm name: %s\n", optarg);
				shm_name = strdup(optarg);
				break;
			default:
				break;
		}
	}

    netboot_init(shm_name, bufsize, c, c);
	dhcp();
	print_network_configuration();

	free(shm_name);

	return 0;
}
