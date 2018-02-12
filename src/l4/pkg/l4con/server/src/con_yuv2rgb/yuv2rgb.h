#ifndef _YUV2RGB_H
#define _YUV2RGB_H

typedef void (* yuv2rgb_fun) (unsigned char *image,
			      const unsigned char *py,
			      const unsigned char *pu,
			      const unsigned char *pv,
			      int h_size, int v_size,
			      int rgb_stride,
			      int y_stride, int uv_stride);

/* exports */
extern yuv2rgb_fun yuv2rgb_render;
extern int yuv2rgb_init_done;

extern unsigned char *yuv2rgb_fb;
extern int yuv2rgb_bytes_per_line;
extern int yuv2rgb_bytes_per_pixel;
extern int yuv2rgb_bits_per_pixel;

/* extern functions */
extern void yuv2rgb_init (int bpp, int mode);

#define MODE_RGB  0x1
#define MODE_BGR  0x2

#endif /* _YUV2RGB_H */

