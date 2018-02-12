/* Also look into example.c of libpng */

#include <l4/libpng/l4png_wrap.h>
#include <l4/sys/l4int.h>
#include <stdlib.h>
#include <string.h>
#include <png.h>
#include <pngstruct.h>

#ifndef png_jmpbuf
#  define png_jmpbuf(png_ptr) ((png_ptr)->jmpbuf)
#endif

#ifdef DEBUG
enum { _DEBUG = 1 };
#else
enum { _DEBUG = 0 };
#endif

#define u16 unsigned short

enum
{
  PNG_BYTES_TO_CHECK = 8,
};

static int check_if_png(char *png_data)
{
  int i;
  char buf[PNG_BYTES_TO_CHECK];

  for (i = 0; i < PNG_BYTES_TO_CHECK; i++)
    buf[i] = png_data[i];

  /* Compare the first PNG_BYTES_TO_CHECK bytes of the signature.
     Return nonzero (true) if they match */

  return png_sig_cmp((unsigned char *)buf, (png_size_t)0,
                     PNG_BYTES_TO_CHECK);
}

struct l4png_wrap_read_func_struct
{
  char *mem;
  l4_size_t offset;
  l4_size_t size;
};

static void
l4png_wrap_read_func_from_memory(png_structp png_ptr, png_bytep data,
                                 png_size_t length)
{
  struct l4png_wrap_read_func_struct *a
    = (struct l4png_wrap_read_func_struct *)png_ptr->io_ptr;

  if (!png_ptr)
    return;

  if (a->offset + length > a->size)
    png_error(png_ptr, "read too much");

  memcpy(data, a->mem + a->offset, length);
  a->offset += length;
}

enum mode_t { M_MEMORY, M_FILE };

static int internal_init(enum mode_t mode,
                         void *png_data, int png_data_size,
                         FILE *fp,
                         struct l4png_wrap_read_func_struct *memio,
                         png_structp *png_ptr,
                         png_infop *info_ptr)
{
  /* check if png_data really contains a png image */
  if (mode == M_MEMORY && check_if_png(png_data))
    return LIBPNG_ENOPNG;

  /* create png read struct */
  *png_ptr = png_create_read_struct_2(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL,
                                      NULL, NULL, NULL);

  if (!*png_ptr)
    {
      printf("error during creation of png read struct");
      return -1;
    }

  if (mode == M_MEMORY)
    {
      memio->mem = png_data;
      memio->size = png_data_size;
      memio->offset = 0;
      png_set_read_fn(*png_ptr, memio, l4png_wrap_read_func_from_memory);
    }

  /* create png info struct */
  *info_ptr = png_create_info_struct(*png_ptr);

  if (!*info_ptr)
    {
      png_destroy_read_struct(png_ptr, NULL, NULL);
      printf("error during creation of png info struct");
      return -1;
    }

  if (setjmp(png_jmpbuf(*png_ptr)))
    {
      /* Free all of the memory associated with the png_ptr and info_ptr */
      png_destroy_read_struct(png_ptr, info_ptr, NULL);

      /* If we get here, we had a problem reading the file */
      return LIBPNG_EDAMAGEDPNG;
    }

  if (mode == M_MEMORY)
    png_init_io(*png_ptr,(png_FILE_p) memio);
  else
    png_init_io(*png_ptr,(png_FILE_p) fp);

  /* read info struct */
  png_read_info(*png_ptr, *info_ptr);

  return 0;
}

static int __internal_png_get_size(enum mode_t mode,
                                   void *png_data, int png_data_size,
                                   FILE *fp,
                                   int *width, int *height)
{
  png_structp png_ptr;
  png_infop info_ptr;
  struct l4png_wrap_read_func_struct memio;

  int r = internal_init(mode, png_data, png_data_size, fp,
                        &memio, &png_ptr, &info_ptr);
  if (r)
    return r;

  *width = png_get_image_width(png_ptr,info_ptr);
  *height = png_get_image_height(png_ptr,info_ptr);

  /* free mem of png structs */
  png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

  return 0;
}

int libpng_get_size_mem(void *png_data, int png_data_size,
                        int *width, int *height)
{
  return __internal_png_get_size(M_MEMORY, png_data, png_data_size, NULL,
                                 width, height);
}

int libpng_get_size_file(const char *filename, int *width, int *height)
{
  FILE *fp;
  int r;

  if (!(fp = fopen(filename, "rb")))
    return -1;

  r = __internal_png_get_size(M_FILE, NULL, 0, fp, width, height);

  fclose(fp);
  return r;
}

#define DITHER_SIZE 16

static const int dither_matrix[DITHER_SIZE][DITHER_SIZE] = {
  {   0,192, 48,240, 12,204, 60,252,  3,195, 51,243, 15,207, 63,255 },
  { 128, 64,176,112,140, 76,188,124,131, 67,179,115,143, 79,191,127 },
  {  32,224, 16,208, 44,236, 28,220, 35,227, 19,211, 47,239, 31,223 },
  { 160, 96,144, 80,172,108,156, 92,163, 99,147, 83,175,111,159, 95 },
  {   8,200, 56,248,  4,196, 52,244, 11,203, 59,251,  7,199, 55,247 },
  { 136, 72,184,120,132, 68,180,116,139, 75,187,123,135, 71,183,119 },
  {  40,232, 24,216, 36,228, 20,212, 43,235, 27,219, 39,231, 23,215 },
  { 168,104,152, 88,164,100,148, 84,171,107,155, 91,167,103,151, 87 },
  {   2,194, 50,242, 14,206, 62,254,  1,193, 49,241, 13,205, 61,253 },
  { 130, 66,178,114,142, 78,190,126,129, 65,177,113,141, 77,189,125 },
  {  34,226, 18,210, 46,238, 30,222, 33,225, 17,209, 45,237, 29,221 },
  { 162, 98,146, 82,174,110,158, 94,161, 97,145, 81,173,109,157, 93 },
  {  10,202, 58,250,  6,198, 54,246,  9,201, 57,249,  5,197, 53,245 },
  { 138, 74,186,122,134, 70,182,118,137, 73,185,121,133, 69,181,117 },
  {  42,234, 26,218, 38,230, 22,214, 41,233, 25,217, 37,229, 21,213 },
  { 170,106,154, 90,166,102,150, 86,169,105,153, 89,165,101,149, 85 }
};


static inline void
convert_line_24to16(unsigned char *src, short *dst, int width, int line)
{
  int j, r, g, b, v;
  int const *dm = dither_matrix[line & 0xf];

  /* calculate 16bit RGB value and copy it to destination buffer */
  for (j = 0; j < width; j++)
    {
      v = dm[j & 0xf];
      r = (int)(*(src++)) + (v >> 5);
      g = (int)(*(src++)) + (v >> 6);
      b = (int)(*(src++)) + (v >> 5);
      if (r > 255)
        r = 255;
      if (g > 255)
        g = 255;
      if (b > 255)
        b = 255;
      *(dst + j) = ((r & 0xf8) << 8) + ((g & 0xfc) << 3) + ((b & 0xf8) >> 3);
    }
}





static void
convert_pixel_from_rgb888(unsigned srcval, void *dst, l4re_video_view_info_t *fbi)
{
  unsigned v;

  v  = ((srcval >> (8  - fbi->pixel_info.r.size)) & ((1 << fbi->pixel_info.r.size) - 1)) << fbi->pixel_info.r.shift;
  v |= ((srcval >> (16 - fbi->pixel_info.g.size)) & ((1 << fbi->pixel_info.g.size) - 1)) << fbi->pixel_info.g.shift;
  v |= ((srcval >> (24 - fbi->pixel_info.b.size)) & ((1 << fbi->pixel_info.b.size) - 1)) << fbi->pixel_info.b.shift;

  //printf("srcval=%08x v=%08x dst=%p bpl=%d\n", srcval, v, dst, fbi->bytes_per_line);
  switch (fbi->pixel_info.bytes_per_pixel)
    {
    case 1:
      *(unsigned char  *)dst = v;
      break;
    case 2:
      *(unsigned short *)dst = v;
      break;
#if defined(ARCH_x86) || defined(ARCH_amd64)
    case 3:
      *(unsigned short *)(dst + 0) = v & 0xffff;
      *(unsigned char  *)(dst + 2) = v >> 16;
      break;
#endif
    case 4:
      *(unsigned int   *)dst = v;
      break;
    default:
      printf("unhandled bitsperpixel %d\n",
             l4re_video_bits_per_pixel(&fbi->pixel_info));
    };
}


static int
internal_render(enum mode_t mode, void *png_data, char * const dst,
                int png_data_size, unsigned dst_buf_size,
                int offset_x, int offset_y,
                l4re_video_view_info_t *dst_descr, FILE *fp)
{
  png_structp png_ptr;
  png_infop info_ptr;
  struct l4png_wrap_read_func_struct memio;

  int r = internal_init(mode, png_data, png_data_size, fp,
                        &memio, &png_ptr, &info_ptr);
  if (r)
    return r;

  // ----------
  png_uint_32 width, height;
  png_bytep row_ptr;
  unsigned row;
  int bit_depth, color_type, interlace_type;

  /* get image data chunk */
  png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
               &interlace_type, NULL, NULL);


  if (_DEBUG)
    printf("bit_depth: %d, color_type: %d, width: %d, height: %d\n",
           bit_depth, color_type, width, height);

  // don't wrap around and don't write behind dst
  if (width + offset_x > dst_descr->width)
    width = dst_descr->width - offset_x;
  if (height + offset_y > dst_descr->height)
    height = dst_descr->height - offset_y;

  if (dst_buf_size < height * dst_descr->bytes_per_line)
    {
      png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
      return LIBPNG_ARGB_BUF_TO_SMALL;
    }

  if (color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_palette_to_rgb(png_ptr);

  if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
    png_set_expand_gray_1_2_4_to_8(png_ptr);

  if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    png_set_gray_to_rgb(png_ptr);

  if (bit_depth < 8)
    png_set_packing(png_ptr);

  /* set down to 8bit value */
  if (bit_depth == 16)
#ifdef PNG_READ_SCALE_16_TO_8_SUPPORTED
     png_set_scale_16(png_ptr);
#else
     png_set_strip_16(png_ptr);
#endif

  if (color_type == PNG_COLOR_TYPE_RGB_ALPHA)
    png_set_strip_alpha(png_ptr);

  if (dst_descr->pixel_info.bytes_per_pixel != 2)
    png_set_filler(png_ptr, 0xff, PNG_FILLER_BEFORE);

  png_read_update_info(png_ptr, info_ptr);

  row_ptr = malloc(png_get_rowbytes(png_ptr, info_ptr));

  if (!row_ptr)
    {
      printf("not enough memory for a png row");
      return -1;
    }

  char *_dst = dst + offset_y * dst_descr->bytes_per_line;

  if (dst_descr->pixel_info.bytes_per_pixel == 2)
    {
      // special case for 16bit, with dithering
      if (interlace_type == PNG_INTERLACE_NONE)
        {
          if (offset_x >= 0)
            for (row = 0; row < height; row++)
              {
                png_read_row(png_ptr, row_ptr, NULL);
                if (_dst >= dst)
                  convert_line_24to16(row_ptr, (short *)_dst + offset_x,
                                      width, row);
                _dst += dst_descr->bytes_per_line;
              }
          else
            for (row = 0; row < height; row++)
              {
                png_read_row(png_ptr, row_ptr, NULL);
                if (_dst >= dst)
                  convert_line_24to16((unsigned char *)row_ptr + 3 * (-offset_x),
                                      (short *)_dst, width + offset_x, row);
                _dst += dst_descr->bytes_per_line;
              }
        }
    }
  else
    {
      for (row = 0; row < height; row++)
        {
          png_read_row(png_ptr, (png_bytep)row_ptr, NULL);

          if (_dst >= dst)
            {
              unsigned *r = (unsigned *)row_ptr;
              unsigned o;

              if (offset_x >= 0)
                o = offset_x * dst_descr->pixel_info.bytes_per_pixel;
              else
                {
                  o = 0;
                  r += -offset_x;
                }

              for (unsigned j = 0; j < width;
                   ++j, ++r, o += dst_descr->pixel_info.bytes_per_pixel)
                convert_pixel_from_rgb888(*r >> 8, _dst + o, dst_descr);
            }

          _dst += dst_descr->bytes_per_line;
        }
    }

  free(row_ptr);

  if (0) /* If we do not read all rows this emit a warning */
    png_read_end(png_ptr, info_ptr);

  png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

  return 0;
}

int libpng_render_mem(void *png_data, void *dst_buf,
                      unsigned png_data_size, unsigned dst_size,
                      l4re_video_view_info_t *dst_descr)
{
  return internal_render(M_MEMORY, png_data, dst_buf,
                         png_data_size, dst_size,
                         0, 0,
                         dst_descr, NULL);
}

int libpng_render_mem2(void *png_data, void *dst_buf,
                       unsigned png_data_size, unsigned dst_size,
                       int offset_x, int offset_y,
                       l4re_video_view_info_t *dst_descr)
{
  return internal_render(M_MEMORY, png_data, dst_buf,
                         png_data_size, dst_size,
                         offset_x, offset_y,
                         dst_descr, NULL);
}

int libpng_render_file(const char *filename, void *dst_buf,
                       unsigned dst_size,
                       l4re_video_view_info_t *dst_descr)
{
  FILE *fp;
  int r;

  if (!(fp = fopen(filename, "rb")))
    return -1;

  r = internal_render(M_MEMORY, 0, dst_buf, 0, dst_size, 0, 0, dst_descr, fp);

  fclose(fp);
  return r;
}
