#pragma once

#include <l4/sys/compiler.h>

EXTERN_C_BEGIN

/**
 * DDE support to access block device specific functionality
 */

struct ddekit_bd_geometry {
      unsigned char heads;
      unsigned char sectors;
      unsigned short cylinders;
      unsigned long start;
};

struct gendisk;

struct ddekit_bd_disk {
    struct gendisk *disk;
    int partno;
    unsigned long start_sector;
    unsigned long sectors;
};

struct ddekit_bd_disk *ddekit_bd_create(const char *name, int partno);
void ddekit_bd_destroy(struct ddekit_bd_disk *d);

struct bio;

//int ddekit_bd_get_geom(struct ddekit_bd_disk *disk, struct ddekit_bd_geometry *geom);
//struct bio *ddekit_bd_create_bio(struct ddekit_bd_disk *disk, unsigned long start_sector, unsigned sg_count);
struct bio *ddekit_bd_create_bio(struct gendisk *disk, int partno, unsigned long start_sector,
                                 unsigned sg_count);

void ddekit_bd_page_cache_add(void *data_address, unsigned long data_size);
void ddekit_bd_page_cache_remove(void *data_address, unsigned long data_size);

/** callback function after BIO has completed */
typedef void (*bio_end_callback_t)(void *priv_data, unsigned int bytes_done);

bio_end_callback_t ddekit_bd_register_callback(bio_end_callback_t callback);

int ddekit_bd_add_bio_io(struct bio *b, unsigned i, unsigned addr, unsigned size);
void ddekit_bd_submit_bio(struct bio *b, void *priv_data, int is_read);

void ddekit_add_disk(char * name, void *private_data);
void *ddekit_find_disk(const char *name);

EXTERN_C_END
