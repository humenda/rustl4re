#ifndef __NETBOOT_PXE_TFTP_H__
#define __NETBOOT_PXE_TFTP_H__

#include "grub.h"

#ifdef INCLUDE_PXE_TFTP

int pxe_tftp_driver_active(void);
int pxe_tftp_open(const char *filename);
int pxe_tftp_read(char *buf, int size);
int pxe_tftp_close(void);
int pxe_tftp_get_file_size(const char *filename);

#else

static inline int pxe_tftp_driver_active(void)
{ return 0; }
static inline int pxe_tftp_open(const char *filename)
{ return 0; }
static inline int pxe_tftp_read(char *buf, int size)
{ return -1; }
static inline int pxe_tftp_close(void)
{ return 0; }
static inline int pxe_tftp_get_file_size(const char *filename)
{ return -1; }

#endif

#endif /* ! __NETBOOT_PXE_TFTP_H__ */
