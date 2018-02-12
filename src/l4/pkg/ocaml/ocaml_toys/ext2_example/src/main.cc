/*
 * (c) 2008-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <l4/ocaml/ext2.h>

int main(int argc, char** argv)
{
	if (argc != 3) {
		fprintf (stderr,"usage: ext2_example <disk_image_name> <path_to_file>\n");
		return -1;
	}

	if (!open_disk_image (argv[1])) {
		fprintf (stderr,"open_disk_image '%s' failed\n",argv[1]);
		return -1;
	}

	int fd = open_file (argv[2]);
	if (!fd) {
		fprintf (stderr,"open_file '%s' failed\n",argv[2]);
		return -1;
	}

	char* buffer = (char*)malloc(2<<12);
	if (!buffer) {
		fprintf (stderr,"malloc(4k) failed\n");
		return -1;
	}

	if (!read_block (fd, 0, buffer)) {
		fprintf (stderr,"read_block failed\n");
		return -1;
	}

	printf ("dumping file '%s'\n",argv[2]);

	// --- debug ---
	for (int i=0; i<64; i++) {
		if (i%16 == 0)
			printf ("0x%04x : ",i);

		printf ("%02x ",buffer[i]);

		if (i%16 == 15)
			printf ("\n");
	}

	printf ("%s",buffer);

	printf ("opening dir /\n");

	unsigned dir = opendir("/");

	if (!dir) {
		fprintf (stderr,"opendir / failed\n");
		return -1;
	}

	do {
		if (is_file (dir))
			printf ("File");
		else 
			printf ("Dir");

		printf (" '%s'\n",filename(dir));
	} while (dir = readdir (dir));

	return 0;
}
