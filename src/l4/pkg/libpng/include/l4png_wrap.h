#ifndef __L4PNG_WRAP_H__
#define __L4PNG_WRAP_H__

#include <sys/cdefs.h>

__BEGIN_DECLS

enum {
  LIBPNG_ARGB_BUF_TO_SMALL = -2,
  LIBPNG_ENOPNG            = -3,
  LIBPNG_EDAMAGEDPNG       = -4,
};

#include <l4/re/c/video/goos.h>

/**
 * \brief Get the dimension of an PNG picture.
 * \return 0 on success, negative on error
 */
int libpng_get_size_mem(void *png_data, int png_data_size, int *width, int *height);
int libpng_get_size_file(const char *fp, int *width, int *height);

int libpng_render_mem(void *png_data, void *dst_buf,
                      unsigned png_data_size, unsigned dst_size,
                      l4re_video_view_info_t *dst_descr);

int libpng_render_mem2(void *png_data, void *dst_buf,
                       unsigned png_data_size, unsigned dst_size,
                       int offset_x, int offset_y,
                       l4re_video_view_info_t *dst_descr);

int libpng_render_file(const char *filename, void *dst_buf,
                       unsigned dst_size, l4re_video_view_info_t *dst_descr);

__END_DECLS

#endif /* ! __L4PNG_WRAP_H__ */
