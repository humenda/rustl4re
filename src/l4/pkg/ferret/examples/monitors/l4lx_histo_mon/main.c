/*
 * (c) 2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>

#include <l4/ferret/sensors/histogram_consumer.h>

#define FIFO_NAME "histo_ctrl.fifo"

struct stat sbuf;

int length;
ferret_histo_t * histo[2];
void * addr;

FILE *f1, *f2;

char temp[3];

int main(void)
{
    unsigned u;
    int fd, fifo, ret;

    // setup
    ret = mkfifo(FIFO_NAME, 00666);
    perror("mkfifo: ");

    fd = open("/proc/ferret/syscall_histo", O_RDONLY);
    perror("open: ");
    if (fd < 0)
        exit(1);
    ret = fstat(fd, &sbuf);
    perror("fstat: ");
    if (ret < 0)
        exit(1);
    length = sbuf.st_size;
    fprintf(stderr, "size: %d\n", length);
    addr = mmap(0, length, PROT_READ, MAP_SHARED, fd, 0);
    perror("mmap: ");
    if (addr == MAP_FAILED)
        exit(1);
    fprintf(stderr, "fd: %d, addr %p, errno %d\n", fd, addr, errno);
    histo[0] = malloc(length);
    histo[1] = malloc(length);
    if (histo[0] == NULL || histo[1] == NULL)
    {
        fprintf(stderr, "Malloc failed.\n");
        exit(1);
    }

    // getting the data
    do
    {
        fifo = open(FIFO_NAME, O_RDONLY);
        if (fifo < 0)
            exit(1);
        while ((ret = read(fifo, temp, 1)) == -1)
            ;
        if (ret == 0)
        {
            close(fifo);
            continue;
        }
    } while (ret != 1);
    fprintf(stderr, "Copying for the first time ...\n");
    memcpy(histo[0], addr, length);
    fprintf(stderr, "Waiting for next signal ...\n");

    do
    {
        while ((ret = read(fifo, temp, 1)) == -1)
            ;
        if (ret == 0)
        {
            close(fifo);
            fifo = open(FIFO_NAME, O_RDONLY);
            if (fifo < 0)
                exit(1);
            continue;
        }
    } while (ret != 1);
    memcpy(histo[1], addr, length);
    fprintf(stderr, "... done.  Postprocessing data.\n");

#if 0
    histo[0] = malloc(length);
    histo[1] = malloc(length);

    f1 = fopen("ferret_syscall_hist1", "r");
    fread(histo[0], length, 1, f1);
    f2 = fopen("ferret_syscall_hist2", "r");
    fread(histo[1], length, 1, f2);

#endif

    for (u = 0; u < histo[0]->head.bins; u++)
    {
        if (histo[1]->data[u] - histo[0]->data[u] != 0)
            printf("%u:\t%u\n", u, histo[1]->data[u] - histo[0]->data[u]);
    }

    //ferret_histo_dump(histo[0]);
    //ferret_histo_dump(histo[1]);

    return 0;
}
