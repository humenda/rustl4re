
/*	con/examples/include/util.h
 *
 *	useful functions for con apps
 */

#ifndef _EXAMPLES_UTIL_H
#define _EXAMPLES_UTIL_H

/* L4 includes */
#include <l4/util/getopt.h>
#include <l4/l4con/l4con.h>

/* OSKit includes */
#include <stdlib.h>

/* prototypes */
void do_args(int, char**);
int set_rgb16(int, int, int);
int ret2ecodestr(l4_int32_t, char*);

/* global vars */
l4_uint8_t global_red, global_green, global_blue;

/******************************************************************************
 * do_args                                                                    *
 *                                                                            *
 * extract parameter (arguments) from command line                            *
 ******************************************************************************/
void do_args(int argc, char *argv[])
{
	int c;
	char* endp;
	l4_uint32_t a;

	static struct option long_options[] = {
		{"red", 1, 0, 'r'},
		{"green", 1, 0, 'g'},
		{"blue", 1, 0, 'b'},
		{0, 0, 0, 0}
	};

	/* read command line arguments */
	while (1) {
		c = getopt_long (argc, argv, "r:g:b:",
				 long_options, NULL);

		if (c == -1)
			break;

		switch(c) {
		case 'r':
			/* red channel value */
			if (optarg) {
				a = strtoul(optarg,&endp,0);
				if (*endp != 0)	{
					break;
				}
				global_red = (l4_uint8_t) a & 0xff;
			}
			break;
		case 'g':
			/* red channel value */
			if (optarg) {
				a = strtoul(optarg,&endp,0);
				if (*endp != 0)	{
					break;
				}
				global_green = (l4_uint8_t) a & 0xff;
			}
			break;
		case 'b':
			/* red channel value */
			if (optarg) {
				a = strtoul(optarg,&endp,0);
				if (*endp != 0)	{
					break;
				}
				global_blue = (l4_uint8_t) a & 0xff;
			}
			break;
		}
	}
}

/******************************************************************************
 * set_rgb16                                                                  *
 *                                                                            *
 * convert red, green, blue integers to 16 bit pixel                          *
 ******************************************************************************/
int set_rgb16(int r, int g, int b)
{
	return ((b >> 3) +
		((g >> 2) << 5) +
		((r >> 3) << 11));
}

/******************************************************************************
 * ret2ecodestr                                                               *
 *                                                                            *
 * convert return values to con error code strings                            *
 ******************************************************************************/
int ret2ecodestr(l4_int32_t ret, char *buffer)
{
	char *tmp;

	switch (0 - ret) {
	case 0:
		tmp = "OK";
		break;
	case CON_EUNKNOWN:
		tmp = "CON_EUNKNOWN";
		break;
	case CON_EFREE:
		tmp = "CON_EFREE";
		break;
	case CON_ETHREAD:
		tmp = "CON_ETHREAD";
		break;
	case CON_EPARAM:
		tmp = "CON_EPARAM";
		break;
	case CON_EXPARAM:
		tmp = "CON_EXPARAM";
		break;
	case CON_ENOTIMPL:
		tmp = "CON_ENOTIMPL";
		break;
	case CON_EPERM:
		tmp = "CON_EPERM";
		break;
	case CON_ENOMEM:
		tmp = "CON_ENOMEM";
		break;
	case CON_EPROTO:
		tmp = "CON_EPROTO";
		break;
	default:
		tmp = "unknown ecode";
	}
	sprintf(buffer, "%s", tmp);
	return 0;
}

#endif /* !_EXAMPLES_UTIL_H */
