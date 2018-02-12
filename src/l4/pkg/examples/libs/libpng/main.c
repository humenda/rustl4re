#include <l4/re/c/util/cap_alloc.h>
#include <l4/re/c/util/video/goos_fb.h>
#include <l4/libpng/l4png_wrap.h>
#include <l4/util/util.h>

#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, char **argv)
{
  void *bildmem;
  void *vidmem;
  l4re_util_video_goos_fb_t gfb;
  l4re_video_view_info_t fbi;

  if (argc < 2)
    {
      printf("Need to give PNG picture to display\n");
      return 1;
    }

  if (l4re_util_video_goos_fb_setup_name(&gfb, "fb"))
    return 45;

  if (!(vidmem = l4re_util_video_goos_fb_attach_buffer(&gfb)))
    return 46;

  printf("size: %ld\n", l4re_ds_size(l4re_util_video_goos_fb_buffer(&gfb)));
  printf("Vidmem attached to %p\n", vidmem);

  if (l4re_util_video_goos_fb_view_info(&gfb, &fbi))
    {
      printf("l4re_fb_open failed\n");
      return 1;
    }

  int bild = open(argv[1], O_RDONLY);
  if (bild == -1)
    {
      printf("Could not open '%s'.\n", argv[1]);
      perror("open");
      return 8;
    }

  struct stat st;
  if (fstat(bild, &st) == -1)
    return 9;

  bildmem = mmap(0, st.st_size, PROT_READ, MAP_SHARED, bild, 0);
  if (bildmem == MAP_FAILED)
    return 10;

  int png_w, png_h;
  libpng_get_size_mem(bildmem, st.st_size, &png_w, &png_h);

  printf("PNG: %dx%d\n", png_w, png_h);

  if (png_w < 0 || png_h < 0)
    {
      printf("Error with picture. Is it one?\n");
      return 1;
    }

  libpng_render_mem2(bildmem, (void *)vidmem,
                     st.st_size,
                     l4re_ds_size(l4re_util_video_goos_fb_buffer(&gfb)),
                     ((int)fbi.width - png_w) / 2,
                     ((int)fbi.height - png_h) / 2,
                     &fbi);

  l4re_util_video_goos_fb_refresh(&gfb, 0, 0, png_w, png_h);

  l4_sleep_forever();

  return 0;
}
