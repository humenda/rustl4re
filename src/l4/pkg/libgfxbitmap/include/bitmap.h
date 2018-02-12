/**
 * \file
 * \brief Bitmap renderer header file.
 */
/*
 * (c) 2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU Lesser General Public License 2.1.
 * Please see the COPYING-LGPL-2.1 file for details.
 */
#pragma once

#include <l4/sys/types.h>
#include <l4/re/c/video/view.h>

/**
 * \defgroup api_gfxbitmap Bitmap graphics and fonts
 * \ingroup l4util_api
 *
 * This library provides some functions for bitmap handling in frame
 * buffers. Includes simple functions like filling or copying an area of the
 * frame buffer going up to rendering text into the frame buffer using
 * bitmap fonts.
 */

/**
 * \defgroup api_gfxbitmap_bitmap Functions for rendering bitmap data in frame buffers
 * \ingroup api_gfxbitmap
 */

EXTERN_C_BEGIN
/**
 * \name Param macros for bmap_*
 *
 * Bitmap type - start least or start most significant bit */
/*@{*/
#define pSLIM_BMAP_START_MSB    0x02    /*!<\brief `pbm'-style: "The bits are 
                                         * stored eight per byte, high bit first
                                         * low bit last." */
#define pSLIM_BMAP_START_LSB    0x01    /*!< the other way round*/
/*@}*/

/**
 * \addtogroup api_gfxbitmap_bitmap
 */
/*@{*/

/**
 * \brief Standard color type
 *
 * It's a RGB type with 8bits for each channel, regardless of the
 * framebuffer used.
 */
typedef unsigned int gfxbitmap_color_t;

/**
 * \brief Specific color type
 *
 * This color type is specific for a particular framebuffer, it can be use
 * to write pixel on a framebuffer. Use gfxbitmap_convert_color to convert
 * from  gfxbitmap_color_t to gfxbitmap_color_pix_t.
 */
typedef unsigned int gfxbitmap_color_pix_t;

/** offsets in pmap[] and bmap[] */
struct gfxbitmap_offset
{
  l4_uint32_t preskip_x; /**< skip pixels at beginning of line */
  l4_uint32_t preskip_y; /**< skip lines */
  l4_uint32_t endskip_x; /**< skip pixels at end of line */
};

/**
 * \brief Convert a color.
 *
 * Converts a given color in standard format to the format used in the
 * framebuffer.
 */
gfxbitmap_color_pix_t
gfxbitmap_convert_color(l4re_video_view_info_t *vi, gfxbitmap_color_t rgb);

/**
 * \brief Fill a rectangular area with a color.
 *
 * \param vfb    Frame buffer.
 * \param vi     Frame buffer information structure.
 * \param x      X position of area.
 * \param y      Y position of area.
 * \param w      Width of area.
 * \param h      Height of area.
 * \param color  Color of area.
 */
void
gfxbitmap_fill(l4_uint8_t *vfb, l4re_video_view_info_t *vi,
                int x, int y, int w, int h, gfxbitmap_color_pix_t color);

/**
 * \brief Fill a rectangular area with a bicolor bitmap pattern.
 *
 * \param vfb    Frame buffer.
 * \param vi     Frame buffer information structure.
 * \param x      X position of area.
 * \param y      Y position of area.
 * \param w      Width of area.
 * \param h      Height of area.
 * \param bmap   Bitmap pattern.
 * \param fgc    Foreground color.
 * \param bgc    Background color.
 * \param offset Offsets.
 * \param mode   Mode
 *
 * \see #pSLIM_BMAP_START_MSB and #pSLIM_BMAP_START_LSB.
 */
void
gfxbitmap_bmap(l4_uint8_t *vfb, l4re_video_view_info_t *vi,
                l4_int16_t x, l4_int16_t y, l4_uint32_t w,
                l4_uint32_t h, l4_uint8_t *bmap,
                gfxbitmap_color_pix_t fgc, gfxbitmap_color_pix_t bgc,
                struct gfxbitmap_offset *offset, l4_uint8_t mode);

/**
 * \brief Set area from source area.
 *
 * \param vfb    Frame buffer.
 * \param vi     Frame buffer information structure.
 * \param x      X position of area.
 * \param y      Y position of area.
 * \param w      Width of area.
 * \param h      Height of area.
 * \param pmap   Source.
 * \param xoffs  X offset.
 * \param yoffs  Y offset.
 * \param offset Offsets.
 * \param pwidth Width of source in bytes.
 */
void
gfxbitmap_set(l4_uint8_t *vfb, l4re_video_view_info_t *vi,
               l4_int16_t x, l4_int16_t y, l4_uint32_t w,
               l4_uint32_t h, l4_uint32_t xoffs, l4_uint32_t yoffs,
               l4_uint8_t *pmap, struct gfxbitmap_offset *offset,
               l4_uint32_t pwidth);

/**
 * \brief Copy a rectangular area.
 *
 * \param dest   Destination frame buffer.
 * \param src    Source frame buffer.
 * \param vi     Frame buffer information structure.
 * \param x      Source X position of area.
 * \param y      Source Y position of area.
 * \param w      Width of area.
 * \param h      Height of area.
 * \param dx     Source X position of area.
 * \param dy     Source Y position of area.
 */
void
gfxbitmap_copy(l4_uint8_t *dest, l4_uint8_t *src, l4re_video_view_info_t *vi,
                int x, int y, int w, int h, int dx, int dy);
/*@}*/

EXTERN_C_END
