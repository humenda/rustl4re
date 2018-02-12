/**
 * \file    rtc/server/src/x86.c
 * \brief   Get current data and time from CMOS
 *
 * \date    09/23/2003
 * \author  Frank Mehnert <fm3@os.inf.tu-dresden.de> */

/*
 * (c) 2003-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

/* most stuff taken from Linux 2.4.22 */

#include <stdio.h>

#include <l4/util/rdtsc.h>
#include <l4/util/port_io.h>
#include <l4/re/env>
#include <l4/util/util.h>
#include <l4/io/io.h>

#include "rtc.h"

#define RTC_SECONDS		0
#define RTC_SECONDS_ALARM	1
#define RTC_MINUTES		2
#define RTC_MINUTES_ALARM	3
#define RTC_HOURS		4
#define RTC_HOURS_ALARM		5
#define RTC_DAY_OF_WEEK		6
#define RTC_DAY_OF_MONTH	7
#define RTC_MONTH		8
#define RTC_YEAR		9

#define RTC_REG_A		10
#define RTC_REG_B		11
#define RTC_REG_C		12
#define RTC_REG_D		13

#define RTC_FREQ_SELECT		RTC_REG_A
# define RTC_UIP		0x80
# define RTC_DIV_CTL		0x70
#  define RTC_REF_CLCK_4MHZ	0x00
#  define RTC_REF_CLCK_1MHZ	0x10
#  define RTC_REF_CLCK_32KHZ	0x20
#  define RTC_DIV_RESET1	0x60
#  define RTC_DIV_RESET2	0x70
# define RTC_RATE_SELECT	0x0F

#define RTC_CONTROL		RTC_REG_B
# define RTC_SET		0x80
# define RTC_PIE		0x40
# define RTC_AIE		0x20
# define RTC_UIE		0x10
# define RTC_SQWE		0x08
# define RTC_DM_BINARY		0x04
# define RTC_24H		0x02
# define RTC_DST_EN		0x01

#define RTC_PORT(x)		(0x70 + (x))
#define RTC_ALWAYS_BCD	1	/* RTC operates in binary mode */

#define sleep_as_iodelay_port80() l4_sleep(0)

#define CMOS_READ(addr) ({		\
	l4_uint8_t val;			\
	l4util_out8(addr, RTC_PORT(0)); \
	sleep_as_iodelay_port80();	\
	val = l4util_in8(RTC_PORT(1));	\
	sleep_as_iodelay_port80();	\
	val;				\
	})

#define CMOS_WRITE(val, addr) ({	\
	l4util_out8(addr, RTC_PORT(0)); \
	sleep_as_iodelay_port80();	\
	l4util_out8(val,  RTC_PORT(1));	\
	sleep_as_iodelay_port80();	\
	})

#define BCD_TO_BIN(val)		((val)=((val)&15) + ((val)>>4)*10)
#define BIN_TO_BCD(val)		((((val)/10)<<4) + (val)%10)

/* Converts Gregorian date to seconds since 1970-01-01 00:00:00.
 * Assumes input in normal date format, i.e. 1980-12-31 23:59:59
 * => year=1980, mon=12, day=31, hour=23, min=59, sec=59.
 *
 * [For the Julian calendar (which was used in Russia before 1917,
 * Britain & colonies before 1752, anywhere else before 1582,
 * and is still in use by some communities) leave out the
 * -year/100+year/400 terms, and add 10.]
 *
 * This algorithm was first published by Gauss (I think).
 *
 * WARNING: this function will overflow on 2106-02-07 06:28:16 on
 * machines were long is 32-bit! (However, as time_t is signed, we
 * will already get problems at other places on 2038-01-19 03:14:08) */
static inline l4_uint32_t
mktime (l4_uint32_t year, l4_uint32_t mon, l4_uint32_t day,
	l4_uint32_t hour, l4_uint32_t min, l4_uint32_t sec)
{
  if (0 >= (int) (mon -= 2))
    {
      /* 1..12 -> 11,12,1..10 */
      mon += 12; /* puts Feb last since it has leap day */
      year -= 1;
    }

  return ((( (year/4 - year/100 + year/400 + 367*mon/12 + day)
	    + year*365 - 719499
	   )*24 + hour /* now have hours */
	  )*60 + min /* now have minutes */
         )*60 + sec; /* finally seconds */
}

/*
 * Get current time from CMOS and initialize values.
 */
static int
get_base_time_x86(l4_uint64_t *offset)
{
  l4_uint32_t year, mon, day, hour, min, sec;
  l4_cpu_time_t current_tsc;
  long i;
  /* The Linux interpretation of the CMOS clock register contents:
   * When the Update-In-Progress (UIP) flag goes from 1 to 0, the
   * RTC registers show the second which has precisely just started.
   * Let's hope other operating systems interpret the RTC the same way. */

  /* read RTC exactly on falling edge of update flag */
  for (i=0 ; i<1000000 ; i++)	/* may take up to 1 second... */
    if (CMOS_READ(RTC_FREQ_SELECT) & RTC_UIP)
      break;

  for (i=0 ; i<1000000 ; i++)	/* must try at least 2.228 ms */
    if (!(CMOS_READ(RTC_FREQ_SELECT) & RTC_UIP))
      break;

  do
    {
      current_tsc = l4_rdtsc();

      sec  = CMOS_READ(RTC_SECONDS);
      min  = CMOS_READ(RTC_MINUTES);
      hour = CMOS_READ(RTC_HOURS);
      day  = CMOS_READ(RTC_DAY_OF_MONTH);
      mon  = CMOS_READ(RTC_MONTH);
      year = CMOS_READ(RTC_YEAR);

    } while (sec != CMOS_READ(RTC_SECONDS));

  if (!(CMOS_READ(RTC_CONTROL) & RTC_DM_BINARY) || RTC_ALWAYS_BCD)
    {
      BCD_TO_BIN(sec);
      BCD_TO_BIN(min);
      BCD_TO_BIN(hour);
      BCD_TO_BIN(day);
      BCD_TO_BIN(mon);
      BCD_TO_BIN(year);
    }

  if ((year += 1900) < 1970)
    year += 100;

  l4_uint64_t seconds_since_1970 = mktime(year, mon, day, hour, min, sec);
  printf("Date:%02d.%02d.%04d Time:%02d:%02d:%02d\n",
	  day, mon, year, hour, min, sec);

  *offset = seconds_since_1970 * 1000000000 - l4_tsc_to_ns(current_tsc);
  return 0;
}

struct rtc_time {
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
	int tm_wday;
	int tm_yday;
	int tm_isdst;
};

static inline void gettime(l4_uint32_t *s, l4_uint32_t *ns)
{
  if (l4_scaler_tsc_to_ns == 0)
    l4_calibrate_tsc(l4re_kip());
  l4_tsc_to_s_and_ns(l4_rdtsc(), s, ns);
}

/*
 * taken from Linux 3.17
 */
#define LEAPS_THRU_END_OF(y) ((y)/4 - (y)/100 + (y)/400)
static inline bool is_leap_year(unsigned int year)
{
	return (!(year % 4) && (year % 100)) || !(year % 400);
}
static const unsigned char rtc_days_in_month[] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

#define LEAPS_THRU_END_OF(y) ((y)/4 - (y)/100 + (y)/400)

/*
 * The number of days in the month.
 */
static int rtc_month_days(unsigned int month, unsigned int year)
{
	return rtc_days_in_month[month] + (is_leap_year(year) && month == 1);
}

/*
 * Convert seconds since 01-01-1970 00:00:00 to Gregorian date.
 */
static void rtc_time_to_tm(unsigned long time, struct rtc_time *tm)
{
	unsigned int month, year;
	int days;

	days = time / 86400;
	time -= (unsigned int) days * 86400;

	/* day of the week, 1970-01-01 was a Thursday */
	tm->tm_wday = (days + 4) % 7;

	year = 1970 + days / 365;
	days -= (year - 1970) * 365
		+ LEAPS_THRU_END_OF(year - 1)
		- LEAPS_THRU_END_OF(1970 - 1);
	if (days < 0) {
		year -= 1;
		days += 365 + is_leap_year(year);
	}
	tm->tm_year = year - 1900;
	tm->tm_yday = days + 1;

	for (month = 0; month < 11; month++) {
		int newdays;

		newdays = days - rtc_month_days(month, year);
		if (newdays < 0)
			break;
		days = newdays;
	}
	tm->tm_mon = month;
	tm->tm_mday = days + 1;

	tm->tm_hour = time / 3600;
	time -= tm->tm_hour * 3600;
	tm->tm_min = time / 60;
	tm->tm_sec = time - tm->tm_min * 60;

	tm->tm_isdst = 0;
}

/* Set the current date and time in the real time clock. */
static inline int __set_rtc_time(struct rtc_time *time)
{
	unsigned char mon, day, hrs, min, sec;
	unsigned char save_control, save_freq_select;
	unsigned int yrs;

	yrs = time->tm_year;
	mon = time->tm_mon + 1;   /* tm_mon starts at zero */
	day = time->tm_mday;
	hrs = time->tm_hour;
	min = time->tm_min;
	sec = time->tm_sec;

	if (yrs > 255)	/* They are unsigned */
		return -L4_EINVAL;

	/* These limits and adjustments are independent of
	 * whether the chip is in binary mode or not.
	 */
	if (yrs > 169)
		return -L4_EINVAL;

	if (yrs >= 100)
		yrs -= 100;

	if (!(CMOS_READ(RTC_CONTROL) & RTC_DM_BINARY)
	    || RTC_ALWAYS_BCD) {
		sec = BIN_TO_BCD(sec);
		min = BIN_TO_BCD(min);
		hrs = BIN_TO_BCD(hrs);
		day = BIN_TO_BCD(day);
		mon = BIN_TO_BCD(mon);
		yrs = BIN_TO_BCD(yrs);
	}

	save_control = CMOS_READ(RTC_CONTROL);
	CMOS_WRITE((save_control|RTC_SET), RTC_CONTROL);
	save_freq_select = CMOS_READ(RTC_FREQ_SELECT);
	CMOS_WRITE((save_freq_select|RTC_DIV_RESET2), RTC_FREQ_SELECT);

	CMOS_WRITE(yrs, RTC_YEAR);
	CMOS_WRITE(mon, RTC_MONTH);
	CMOS_WRITE(day, RTC_DAY_OF_MONTH);
	CMOS_WRITE(hrs, RTC_HOURS);
	CMOS_WRITE(min, RTC_MINUTES);
	CMOS_WRITE(sec, RTC_SECONDS);

	CMOS_WRITE(save_control, RTC_CONTROL);
	CMOS_WRITE(save_freq_select, RTC_FREQ_SELECT);

	return 0;
}

struct X86_rtc : Rtc
{
  bool probe()
  {
    printf("probe x86 RTC\n");
    if (long i = l4io_request_ioport(RTC_PORT(0), 2))
      {
        printf("Could not get required port %x and %x, error: %lx\n",
               RTC_PORT(0), RTC_PORT(0) + 1, i);
        return false;
      }

    l4_calibrate_tsc(l4re_kip());
    return true;
  };

  /**
   * Take the given offset to 01-01-1970, calculate the current time using the
   * TSC, and write it into the rtc.
   */
  int set_time(l4_uint64_t offset)
  {
    l4_uint32_t s, ns, current_time;
    l4_uint32_t offset_ns = offset % 1000000000;
    l4_uint32_t offset_s = offset / 1000000000;
    do {
        gettime(&s, &ns);
    } while ((offset_ns  / 8) != (ns / 8));
    current_time = offset_s + s;
    struct rtc_time time;
    rtc_time_to_tm(current_time, &time);
    return __set_rtc_time(&time);
  }

  int get_time(l4_uint64_t *nsec_offset_1970)
  { return get_base_time_x86(nsec_offset_1970); }

};

static X86_rtc _x86_rtc;
