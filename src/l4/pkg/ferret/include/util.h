/**
 * \file   ferret/include/util.h
 * \brief  Some helper functions
 *
 * \date   29/11/2005
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2005-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef __FERRET_INCLUDE_UTIL_H_
#define __FERRET_INCLUDE_UTIL_H_

#if 0
#include <l4/sys/ktrace_events.h>
#endif
#include <l4/ferret/sensors/list.h>

EXTERN_C_BEGIN

/**
 * @brief Pack a number of parameters according to format string into
 *        memory buffer
 *
 * Currently the following tags are valid in format strings: "xbBhHlLqQsp"
 *
 * x       padding byte, no parameter required
 *
 * b,B     Byte and Boolean, one byte is packed in to the memory area
 *
 * h,H     Short word, one 16 bit integer is packed
 *
 * l,L     Long word, one 32 bit integer is packed
 *
 * q,Q     Quad word, one 64 bit integer is packed
 *
 * s       c-String, one c string is packed, terminated by a zero byte
 *
 * p       Pascal-String, one c string is packed as Pascal string,
 *         preceded with one byte of length information (-> max length 255)
 *
 * <count> number before regular tokens are interpreted as
 *         multipliers, before strings they denote the length to use for
 *         storing the string
 *
 * Example: "hhxb10s2xq" will pack to two shorts, one padding byte,
 * one regular byte, a string of length 10 (+ 1 terminating zero
 * byte), two more padding bytes, and a 64 bit integer.  This
 * corresponds to the following struct defintion in c:
 *
 * struct  __attribute__((__packed__)) foo_s
 * {
 *     int16_t a;
 *     int16_t b;
 *     int8_t  _pad1;
 *     char    c[11];
 *     int8_t  _pad2[2];
 *     int64_t d;
 * };
 *
 * @param format format string desribing the memory layout (see
 *               Pythons struct.pack)
 * @param buffer pointer to target memory area
 *
 * @return - >= 0 number of bytes written
 *           < 0 error in format string
 */
L4_CV int ferret_util_pack(const char * format, void * buffer, ...);


/**
 * @brief Unpack a number of parameters according to format string from
 *        memory buffer
 *
 * @param format format string desribing the memory layout (see
 *               Pythons struct.pack)
 * @param buffer pointer to source memory area
 *
 * @return - >= 0 number of bytes parsed
 *           < 0 error in format string
 */

L4_CV int ferret_util_unpack(const char * format, const void * buffer, ...);


#if 0
/**
 * @brief Layout-converts kernel tracebuffer entries to common list
 *        entry subtypes.
 *
 * @param tb_e pointer to tracebuffer entry to get data from
 * @param lec  pointer to common list entry to put data to
 */
L4_CV void ferret_util_convert_k2c(const l4_tracebuffer_entry_t * tb_e,
                                   ferret_list_entry_kernel_t * lec);
#endif

EXTERN_C_END

#endif
