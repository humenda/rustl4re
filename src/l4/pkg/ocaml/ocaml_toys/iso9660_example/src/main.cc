/*
 * (c) 2008-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <l4/ocaml/iso9660.h>

int main(int argc, char** argv)
{
	if (argc != 3) {
		fprintf (stderr,"usage: ext2_example <disk_image_name> <path_to_dir>\n");
		return -1;
	}

	if (!open_disk_image (argv[1])) {
		fprintf (stderr,"open_disk_image '%s' failed\n",argv[1]);
		return -1;
	}

	printf ("opening dir %s\n",argv[2]);

	unsigned dir = opendir(argv[2]);

	if (!dir) {
		fprintf (stderr,"opendir '%s' failed\n",argv[2]);
		return -1;
	}

	do {
		printf ("%s",filename(dir));
		if (is_file (dir)) {
			unsigned size;
			unsigned sector = lookup_file (dir, &size);
			printf (" - FILE sector=%d size=%d\n",sector,size);
		} else printf(" - DIR\n");

	} while (readdir (dir));

	return 0;
}
