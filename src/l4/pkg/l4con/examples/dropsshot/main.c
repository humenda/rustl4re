/*
 * Linux program to take screenshot from the DROPS console
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#define _GNU_SOURCE
#include <getopt.h>

#include <l4/l4con/l4con.h>
#include <l4/l4con/l4con-client.h>
#include <l4/names/libnames.h>
#include <l4/dm_phys/dm_phys.h>
#include <l4/sys/syscalls.h>
#include <l4/dm_mem/dm_mem.h>
#include <l4/util/l4_macros.h>

#define MY_SBUF_SIZE 65536

/* global vars */
l4_threadid_t con_l4id;		/* con at names */
l4_threadid_t vc_l4id;		/* partner VC */

char opt_verbose                = 0;
char opt_quiet                  = 0;
char opt_write_fb               = 0;
char opt_convert                = 1;
char opt_delete_raw             = 1;
char opt_process_raw_input_only = 0;

char get_vc = 0; /* 0 ... current foreground VC */

#define INFO(format, args...) { if (opt_verbose) printf(format, ## args); \
  			      } while (0);

#define PRINT(format, args...) { if (!opt_quiet) printf(format, ## args); \
  			       } while (0);

#define FILENAME_LEN 196
#define FILEEXT_LEN 10
#define RAW_EXT ".raw"
#define FB_EXT ".fb"
#define STD_EXT ".png"
#define PROGNAME "dropsshot"

char filename[FILENAME_LEN];
char raw_filename[FILENAME_LEN];
char fb_filename[FILENAME_LEN];
char file_ext[FILEEXT_LEN] = "";
l4_uint32_t xres, yres, bpp;
int raw_pic_size;
l4_size_t fb_size;
char *fb_mem, *raw_pic_mem;

typedef char l4_page_t[L4_PAGESIZE];
static l4_page_t map_page __attribute__ ((aligned(L4_PAGESIZE)));

typedef struct bpp_15 {
  unsigned short blue:5;
  unsigned short green:5;
  unsigned short red:5;
  unsigned short reserved:1;
} __attribute__((packed)) bpp_15_t;

typedef struct bpp_16 {
  unsigned short blue:5;
  unsigned short green:6;
  unsigned short red:5;
} __attribute__((packed)) bpp_16_t;

typedef struct bpp_24 {
  unsigned char blue, green, red;
} bpp_24_t;

typedef struct bpp_32 {
  unsigned char blue, green, red;
  unsigned char pad; /* so that this struct is 4 bytes... */
} bpp_32_t;

typedef struct raw {
  unsigned char red, green, blue;
} raw_t;

/* pseudo short options for long options */
enum {
  OPTION_GETRAWFB = 256,
};
static struct option long_options[] = {
  {"verbose",     0, 0, 'v'},
  {"quiet",       0, 0, 'q'},
  {"ext",         1, 0, 'e'},
  {"keep-raw",    0, 0, 'r'},
  {"noconvert",   0, 0, 'n'},
  {"help",        0, 0, 'h'},
  {"processraw",  0, 0, 'p'},
  {"size",        1, 0, 's'},
  {"vc",          1, 0, 'i'},
  {"get-fb",      0, 0, OPTION_GETRAWFB},
  {0, 0, 0, 0}
};

void static usage(void)
{
  printf("Usage: %s [options] [picname.ext]\n"
         "\n"
	 "Options:\n"
	 "   -v, --verbose               Tell what's going on.\n"
	 "   -q, --quiet                 Be quiet and say nothing.\n"
	 "   -e, --ext=GRAPHICTYPE       Graphics type (png, jpg, bmp, ...).\n"
	 "                                 Default: png\n"
	 "   -r, --keep-raw              Keep raw data.\n"
	 "   -n, --noconvert             Don't call \"convert\", includes \"-r\".\n"
	 "   --get-fb                    Get plain framebuffer data.\n"
	 "   --processraw                Don't take a screenshot, instead\n"
	 "                                 only convert raw data to pictures.\n"
	 "                                 Give picture name as argument.\n"
	 "                                 Includes \"-r\".\n"
	 "   -s, --size                  Size for \"-p\" option.\n"
	 "   -i, --vc                    Choose VC, 0 for current foreground VC.\n"
	 "   -h, --help                  This help screen.\n"
	 "\n"
	 , PROGNAME);
}

static void decode_size(char *size_str)
{
  int i = 0;
  char *s = size_str;
  char xx[10], yy[10];
  char *t;
  
  t = xx;
  while (*s) {
    if (*s == 'x') {
      i = 0;
      s++;
      *t = 0;
      t = yy;
    }
    *t = *s;
    s++;
    i++;
    t++;
    if (i == 10) {
      fprintf(stderr, "size string is wrong.\n");
      exit(1);
    }
  }
  *t = 0;

  xres = strtol(xx, &t, 10);
  if (t != NULL && *t) {
    fprintf(stderr, "x part of size is wrong.\n");
    exit(1);
  }
  yres = strtol(yy, &t, 10);
  if (t != NULL && *t) {
    fprintf(stderr, "y part of size is wrong.\n");
    exit(1);
  }

}

static int process_args(int argc, char **argv)
{
  int c;
  int option_index = 0;

  while ((c = getopt_long(argc, argv, "vqe:nrhps:i:",
      		  long_options, &option_index)) != -1) {

    switch (c) {
      case 'v':
	opt_verbose = 1;
	break;
      case 'q':
	opt_quiet = 1;
	break;
      case 'e':
	strcpy(file_ext, ".");
	strncat(file_ext, optarg, FILEEXT_LEN-1);
	break;
      case OPTION_GETRAWFB:
	opt_write_fb = 1;
	break;
      case 'n':
	opt_convert = 0;
	opt_delete_raw = 0;
	break;
      case 'r':
	opt_delete_raw = 0;
	break;
      case 'p':
	opt_process_raw_input_only = 1;
	opt_delete_raw = 0;
	break;
      case 's':
	decode_size(optarg);
	break;
      case 'i':
	get_vc = atol(optarg);
	break;
      case 'h':
	usage();
	exit(0);
      default:
	printf("Unknown option.\n");
	usage();
	exit(1);
    }
  }

  return optind;
}

void static inline col15_to_raw(bpp_15_t *c, raw_t *r)
{
  r->red   = c->red   << 3;
  r->green = c->green << 3;
  r->blue  = c->blue  << 3;
}
void static inline col16_to_raw(bpp_16_t *c, raw_t *r)
{
  r->red   = (unsigned char)c->red   << 3;
  r->green = (unsigned char)c->green << 2;
  r->blue  = (unsigned char)c->blue  << 3;
}

void static inline col24_to_raw(bpp_24_t *c, raw_t *r)
{
  r->red   = c->red;
  r->green = c->green;
  r->blue  = c->blue;
}

void static inline col32_to_raw(bpp_32_t *c, raw_t *r)
{
  r->red   = c->red;
  r->green = c->green;
  r->blue  = c->blue;
}

static void convert_raw_to_pic(char *filename, char *raw_filename,
    			       int xres, int yres)
{
  char exec_str[260];
  int len;

  if (xres == 0 || yres == 0) {
    fprintf(stderr, "Width or height seems to be wrong.\n");
    exit(1);
  }

  len = snprintf(exec_str, sizeof(exec_str),
                 "convert -depth 8 -size %dx%d rgb:%s %s\n",
                 xres, yres, raw_filename, filename);
  if (len > sizeof(exec_str)) {
    fprintf(stderr, "exec string is too big!");
    exit(1);
  }
  INFO("Exec string: %s\n", exec_str);

  PRINT("Converting raw data to picture... ");
  if (system(exec_str)) {
    fprintf(stderr, "Error while converting.\n");
  } else {
    PRINT("done.\n");
    INFO("Pictures converted, result in %s\n", filename);
  }
}

static void write_fb(char *fb_mem, int size)
{
  int fb_fd;

  if ((fb_fd = open(fb_filename, O_RDWR|O_CREAT, 0666)) == -1) {
    perror(fb_filename);
    exit(1);
  }

  if (write(fb_fd, fb_mem, size) != size) {
    fprintf(stderr, "Error writing fb file (%s)\n", fb_filename);
  }

  if (close(fb_fd) == -1) {
    perror("close");
    exit(1);
  }
}

static void write_raw(char *raw_pic_mem, int raw_pic_size)
{
  int raw_fd;

  if ((raw_fd = open(raw_filename, O_RDWR|O_CREAT, 0666)) == -1) {
    perror(raw_filename);
    exit(1);
  }

  if (write(raw_fd, raw_pic_mem, raw_pic_size) != raw_pic_size) {
    fprintf(stderr, "Error writing raw file (%s)\n", raw_filename);
  }

  if (close(raw_fd) == -1) {
    perror("close");
    exit(1);
  }
}

static void get_fb(void)
{
  l4_addr_t fpage_addr;
  l4_size_t fpage_size;
  l4dm_dataspace_t ds;
  CORBA_Environment env = dice_default_environment;
  int i, pages, error;
  struct stat st;

  /* check if we're running under L4Linux */
  if (stat("/proc/l4", &st) == -1) {
    fprintf(stderr, "Error: /proc/l4 doesn't exist, not running on L4Linux?!\n");
    exit(1);
  }

  /* ask for 'con' (timeout = 5000 ms) */
  if (names_waitfor_name(CON_NAMES_STR, &con_l4id, 5000) == 0) {
    fprintf(stderr, "PANIC: %s not registered at names", CON_NAMES_STR);
    exit(1);
  }
  INFO("Found my console through names, it's at "l4util_idfmt".\n",
       l4util_idstr(con_l4id));

  PRINT("Screenshot'ing, please smile... ;-)\n");
  /* get screenshot */
  if (con_if_screenshot_call(&con_l4id, get_vc, &ds, 
			&xres, &yres, &bpp, &env)) {
    fprintf(stderr, "Could not get screenshot\n");
    exit(1);
  }
  INFO("Got screenshot: res: %dx%d, bpp: %d\n", xres, yres, bpp);

  if (l4dm_mem_size(&ds, &fb_size)) {
    fprintf(stderr, "Couldn't get size of data space\n");
    exit(1);
  }
  INFO("Size of data space: %d\n", fb_size);

  if ((fb_mem = malloc(fb_size)) == NULL) {
    fprintf(stderr, "Couldn't malloc %d Bytes of memory!\n", fb_size);
    exit(1);
  }
  raw_pic_size = xres*yres*3;
  if ((raw_pic_mem = malloc(raw_pic_size)) == NULL) {
    fprintf(stderr, "Couldn't malloc %d bytes of memory!\n", raw_pic_size);
    exit(1);
  }
  
  pages = fb_size / L4_PAGESIZE; // size is always a multiple of L4_PAGESIZE?
  for (i = 0; i < pages; i++) {
    /* unmap memory */
    l4_fpage_unmap(l4_fpage((l4_umword_t)map_page, L4_LOG2_PAGESIZE,
	  	   L4_FPAGE_RW, L4_MAP_ITEM_MAP),
                   L4_FP_FLUSH_PAGE|L4_FP_ALL_SPACES);

    /* page in L4 page */
    if ((error = l4dm_map_pages(&ds, i*L4_PAGESIZE, L4_PAGESIZE,
			        (l4_addr_t)map_page, L4_LOG2_PAGESIZE,
				0, L4DM_RO, &fpage_addr,&fpage_size)) < 0) {
      fprintf(stderr, "Error %d requesting ds %d at ds_manager "
	      l4util_idfmt"\n",
	  error, ds.id, l4util_idstr(ds.manager));
      l4dm_close(&ds);
      exit(error);
    }

    memcpy(fb_mem + i*L4_PAGESIZE, map_page, L4_PAGESIZE);
  }

  if (l4dm_close(&ds)) {
    fprintf(stderr, "Error on closing dataspace, expect memory leakage...\n");
  }
}

static void convert_fb_to_raw(void)
{
  int i;

  switch (bpp) {
    case 15: {
	       /* color distribution: r:5 g:5 b:5 + 1 empty bit */
	       bpp_15_t col;
	       raw_t    raw;
	       for (i=0; i < xres*yres; i++) {
		 memcpy(&col, fb_mem + i*2, 2);
		 col15_to_raw(&col, &raw);
		 memcpy(raw_pic_mem + i*3, &raw, 3);
	       }
	       break;
	     }
    case 16: {
	       /* color distribution: r:5 g:6 b:5 */
	       bpp_16_t col;
	       raw_t    raw;
	       for (i=0; i < xres*yres; i++) {
		 memcpy(&col, fb_mem + i*2, 2);
		 col16_to_raw(&col, &raw);
		 memcpy(raw_pic_mem + i*3, &raw, 3);
	       }
	       break;
	     }
    case 24: {
	       /* color distribution: r:8 g:8 b:8 */
	       bpp_24_t col;
	       raw_t	raw;
	       for (i=0; i < xres*yres; i++) {
		 memcpy(&col, fb_mem + i*3, 3);
		 col24_to_raw(&col, &raw);
		 memcpy(raw_pic_mem + i*3, &raw, 3);
	       }
	       break;
	     }
    case 32: {
	       /* color distribution: r:8 g:8 b:8 + an empty byte */
	       bpp_32_t col;
	       raw_t	raw;
	       for (i=0; i < xres*yres; i++) {
		 memcpy(&col, fb_mem + i*4, 3);
		 col32_to_raw(&col, &raw);
		 memcpy(raw_pic_mem + i*3, &raw, 3);
	       }
	       break;
	     }
    default:
      fprintf(stderr, "Unknown bits_per_pixel value %d, giving up.\n", bpp);
      exit(1);
  }
}

int main(int argc, char **argv)
{
  int nr_opts;


  nr_opts = process_args(argc, argv);

  if (argc - nr_opts == 0) {
    /* no filename given -> invent one */
    char my_hostname[100];
    gethostname(my_hostname, 100);
    if (strlen(file_ext) == 0)
      strcpy(file_ext, STD_EXT);
    sprintf(filename, "%s.%s.%d%s",
	    PROGNAME, my_hostname, (int)time(NULL), file_ext);
  } else {
    if (strlen(argv[nr_opts]) > FILENAME_LEN - strlen(RAW_EXT)
	                        - strlen(file_ext)) {
      fprintf(stderr, "Error: given filename too long\n");
      exit(1);
    }
    strcpy(filename, argv[nr_opts]);
    strcat(filename, file_ext);
  }
  strcpy(raw_filename, filename);
  strcat(raw_filename, RAW_EXT);
  strcpy(fb_filename, filename);
  strcat(fb_filename, FB_EXT);

  if (!opt_process_raw_input_only) {
    get_fb();

    if (opt_write_fb)
      write_fb(fb_mem, fb_size);

    convert_fb_to_raw();

    INFO("Writing raw picture data to %s\n", raw_filename);
    write_raw(raw_pic_mem, raw_pic_size);
  }

  if (opt_convert)
    convert_raw_to_pic(filename, raw_filename, xres, yres);

  if (opt_delete_raw)
    if (unlink(raw_filename) == -1)
      perror(raw_filename);
  
  return 0;
}
