/*
 * (c) 2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <l4/util/port_io.h>
#include <l4/util/reboot.h>

//#include "version.h"

extern int hercules;	/* provided by startup.c */

static void direct_cons_putchar(unsigned char c, int to_hercules);
static int  direct_cons_try_getchar(void);
       int  have_hercules(void);
       void reboot(void);

int __libc_backend_outs( const char *s, size_t len);
int __getchar(void);

static void porte9(unsigned char c)
{
  l4util_out8(c, 0xe9);
  if (c == 10)
    l4util_out8(13, 0xe9);
}

int
__libc_backend_outs( const char *s, size_t len)
{
  size_t i;
  for (i = 0; i < len; i++)
    {
      direct_cons_putchar(s[i], 0);
      if (0)
        porte9(s[i]);
    }

  return 1;
}

void __attribute__((noreturn))
reboot(void)
{
  l4util_out8(0x80, 0x70);
  l4util_iodelay();
  l4util_in8(0x71);
  l4util_iodelay();

  while (l4util_in8(0x64) & 0x02)
    ;

  l4util_out8(0x8F, 0x70);
  l4util_iodelay();
  l4util_out8(0x00, 0x71);
  l4util_iodelay();

  l4util_out8(0xFE, 0x64);
  l4util_iodelay();

  for (;;)
    ;
}

int
__getchar(void)
{
  int c = 0;
  do
    {
      if (c == -1)
	c = direct_cons_try_getchar();
    } while (c == -1);
  return c;
}

void
_exit(int fd)
{
  (void)fd;
  printf("\n\033[1mReturn reboots...\033[m\n");
  __getchar();
  printf("Rebooting.\n");
  reboot();
}

static void
direct_cons_putchar(unsigned char c, int to_hercules)
{
  static int ofs  = -1;
  static int esc;
  static int esc_val;
  static int attr = 0x07;
  unsigned char *vidbase = (unsigned char*)(to_hercules ? 0xb0000 : 0xb8000);


  if (ofs < 0)
    {
      /* Called for the first time - initialize.  */
      ofs = 80*2*24;
      direct_cons_putchar('\n', to_hercules);
    }

  switch (esc)
    {
    case 1:
      if (c == '[')
	{
	  esc++;
	  goto done;
	}
      esc = 0;
      break;

    case 2:
      if (c >= '0' && c <= '9')
	{
	  esc_val = 10*esc_val + c - '0';
	  goto done;
	}
      if (c == 'm')
	{
	  attr = esc_val ? 0x0f : 0x07;
	  goto done;
	}
      esc = 0;
      break;
    }

  switch (c)
    {
    case '\n':
      memcpy(vidbase, vidbase+80*2, 80*2*24);
      memset(vidbase+80*2*24, 0, 80*2);
      /* fall through... */
    case '\r':
      ofs = 0;
      break;

    case '\t':
      ofs = (ofs + 8) & ~7;
      break;

    case '\033':
      esc = 1;
      esc_val = 0;
      break;

    default:
      /* Wrap if we reach the end of a line.  */
      if (ofs >= 80)
	direct_cons_putchar('\n', to_hercules);

      /* Stuff the character into the video buffer. */
	{
	  volatile unsigned char *p = vidbase + 80*2*24 + ofs*2;
	  p[0] = c;
  	  p[1] = attr;
	  ofs++;
	}
      break;
    }

done:
  return;
}

int
have_hercules(void)
{
  unsigned short *p, p_save;
  int count = 0;
  unsigned long delay;
  
  /* check for video memory */
  p = (unsigned short*)0xb0000;
  p_save = *p;
  *p = 0xaa55; if (*p == 0xaa55) count++;
  *p = 0x55aa; if (*p == 0x55aa) count++;
  *p = p_save;
  if (count != 2)
    return 0;

  /* check for I/O ports (HW cursor) */
  l4util_out8(0x0f, 0x3b4);
  l4util_iodelay();
  l4util_out8(0x66, 0x3b5);
  for (delay=0; delay<0x100000UL; delay++)
    asm volatile ("nop" : :);
  if (l4util_in8(0x3b5) != 0x66)
    return 0;
  
  l4util_iodelay();
  l4util_out8(0x0f, 0x3b4);
  l4util_iodelay();
  l4util_out8(0x99, 0x3b5);

  for (delay=0; delay<0x10000; delay++)
    l4util_iodelay();
  if (l4util_in8(0x3b5) != 0x99)
    return 0;

  /* reset position */
  l4util_out8(0xf, 0x3b4);
  l4util_iodelay();
  l4util_out8(0x0, 0x3b5);
  l4util_iodelay();
  return 1;
}

#define SHIFT -1

static const char keymap[128][2] = {
  {0},			/* 0 */
  {27,	27},		/* 1 - ESC */
  {'1',	'!'},		/* 2 */
  {'2',	'@'},
  {'3',	'#'},
  {'4',	'$'},
  {'5',	'%'},
  {'6',	'^'},
  {'7',	'&'},
  {'8',	'*'},
  {'9',	'('},
  {'0',	')'},
  {'-',	'_'},
  {'=',	'+'},
  {8,	8},		/* 14 - Backspace */
  {'\t','\t'},		/* 15 */
  {'q',	'Q'},
  {'w',	'W'},
  {'e',	'E'},
  {'r',	'R'},
  {'t',	'T'},
  {'y',	'Y'},
  {'u',	'U'},
  {'i',	'I'},
  {'o',	'O'},
  {'p',	'P'},
  {'[',	'{'},
//   {']','}'},		/* 27 */
  {'+',	'*'},		/* 27 */
  {'\r','\r'},		/* 28 - Enter */
  {0,	0},		/* 29 - Ctrl */
  {'a',	'A'},		/* 30 */
  {'s',	'S'},
  {'d',	'D'},
  {'f',	'F'},
  {'g',	'G'},
  {'h',	'H'},
  {'j',	'J'},
  {'k',	'K'},
  {'l',	'L'},
  {';',	':'},
  {'\'','"'},		/* 40 */
  {'`',	'~'},		/* 41 */
  {SHIFT, SHIFT},	/* 42 - Left Shift */
  {'\\', '|'},		/* 43 */
  {'z',	'Z'},		/* 44 */
  {'x',	'X'},
  {'c',	'C'},
  {'v',	'V'},
  {'b',	'B'},
  {'n',	'N'},
  {'m',	'M'},
  {',',	'<'},
  {'.',	'>'},
  //  {'/','?'},	/* 53 */
  {'-',	'_'},		/* 53 */
  {SHIFT, SHIFT},	/* 54 - Right Shift */
  {0,	0},		/* 55 - Print Screen */
  {0,	0},		/* 56 - Alt */
  {' ',	' '},		/* 57 - Space bar */
  {0,	0},		/* 58 - Caps Lock */
  {0,	0},		/* 59 - F1 */
  {0,	0},		/* 60 - F2 */
  {0,	0},		/* 61 - F3 */
  {0,	0},		/* 62 - F4 */
  {0,	0},		/* 63 - F5 */
  {0,	0},		/* 64 - F6 */
  {0,	0},		/* 65 - F7 */
  {0,	0},		/* 66 - F8 */
  {0,	0},		/* 67 - F9 */
  {0,	0},		/* 68 - F10 */
  {0,	0},		/* 69 - Num Lock */
  {0,	0},		/* 70 - Scroll Lock */
  {'7',	'7'},		/* 71 - Numeric keypad 7 */
  {'8',	'8'},		/* 72 - Numeric keypad 8 */
  {'9',	'9'},		/* 73 - Numeric keypad 9 */
  {'-',	'-'},		/* 74 - Numeric keypad '-' */
  {'4',	'4'},		/* 75 - Numeric keypad 4 */
  {'5',	'5'},		/* 76 - Numeric keypad 5 */
  {'6',	'6'},		/* 77 - Numeric keypad 6 */
  {'+',	'+'},		/* 78 - Numeric keypad '+' */
  {'1',	'1'},		/* 79 - Numeric keypad 1 */
  {'2',	'2'},		/* 80 - Numeric keypad 2 */
  {'3',	'3'},		/* 81 - Numeric keypad 3 */
  {'0',	'0'},		/* 82 - Numeric keypad 0 */
  {'.',	'.'},		/* 83 - Numeric keypad '.' */
};

int
direct_cons_try_getchar(void)
{
  static unsigned shift_state;
  unsigned status, scan_code, ch;

retry:
  __asm__ __volatile__ ("rep; nop");

  /* Wait until a scan code is ready and read it. */
  status = l4util_in8(0x64);
  if ((status & 0x01) == 0)
    {
      return -1;
    }
  scan_code = l4util_in8(0x60);

  /* Drop mouse events */
  if ((status & 0x20) != 0)
    {
      return -1;
    }

  /* Handle key releases - only release of SHIFT is important. */
  if (scan_code & 0x80)
    {
      scan_code &= 0x7f;
      if (keymap[scan_code][0] == SHIFT)
	shift_state = 0;
      goto retry;
    }

  /* Translate the character through the keymap. */
  ch = keymap[scan_code][shift_state];
  if (ch == (unsigned)SHIFT)
    {
      shift_state = 1;
      goto retry;
    } else if (ch == 0)
      goto retry;

  return ch;
}
