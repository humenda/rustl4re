/**
 * \file
 * \brief Bitmap font renderer header file.
 */
/*
 * (c) 2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU Lesser General Public License 2.1.
 * Please see the COPYING-LGPL-2.1 file for details.
 */
#pragma once

#include <l4/sys/compiler.h>
#include <l4/re/c/video/view.h>
#include <l4/libgfxbitmap/bitmap.h>

/**
 * \defgroup api_gfxbitmap_font Functions for rendering bitmap fonts to frame buffers
 * \ingroup api_gfxbitmap
 */

/**
 * \addtogroup api_gfxbitmap_font
 */
/*@{*/

/**
 * \brief Constant to use for the default font.
 */
#define GFXBITMAP_DEFAULT_FONT (void *)0

/**
 * \brief Constant for length field.
 *
 * Use this if the function should call strlen on the text argument itself.
 */
enum { GFXBITMAP_USE_STRLEN = ~0U };

EXTERN_C_BEGIN

/** \brief Font */
typedef void *gfxbitmap_font_t;

/**
 * \brief Initialize the library.
 *
 * This function must be called before any other font function of this
 * library.
 *
 * \return 0 on success, other on error
 */
L4_CV int gfxbitmap_font_init(void);

/**
 * \brief Get a font descriptor.
 *
 * \param name Name of the font.
 *
 * \return A (opaque) font descriptor, or NULL if font could not be found.
 */
L4_CV gfxbitmap_font_t gfxbitmap_font_get(const char *name);

/**
 * \brief Get the font width.
 *
 * \param font   Font.
 * \return Font width, 0 if font width could not be retrieved.
 */
L4_CV unsigned
gfxbitmap_font_width(gfxbitmap_font_t font);

/**
 * \brief Get the font height.
 *
 * \param font   Font.
 * \return Font height, 0 if font height could not be retrieved.
 */
L4_CV unsigned
gfxbitmap_font_height(gfxbitmap_font_t font);

/**
 * \brief Get bitmap font data for a specific character.
 *
 * \param font   Font.
 * \param c      Character.
 * \return Pointer to bmap data, NULL on error.
 */
L4_CV void *
gfxbitmap_font_data(gfxbitmap_font_t font, unsigned c);

/**
 * \brief Render a string to a framebuffer.
 *
 * \param fb      Pointer to frame buffer.
 * \param vi      Frame buffer info structure.
 * \param font    Font.
 * \param text    Text string.
 * \param len     Length of the text string.
 * \param x       Horizontal position in the frame buffer.
 * \param y       Vertical position in the frame buffer.
 * \param fg      Foreground color.
 * \param bg      Background color.
 */
L4_CV void
gfxbitmap_font_text(void *fb, l4re_video_view_info_t *vi,
                    gfxbitmap_font_t font, const char *text, unsigned len,
                    unsigned x, unsigned y,
                    gfxbitmap_color_pix_t fg, gfxbitmap_color_pix_t bg);

/**
 * \brief Render a string to a framebuffer, including scaling.
 *
 * \param fb      Pointer to frame buffer.
 * \param vi      Frame buffer info structure.
 * \param font    Font.
 * \param text    Text string.
 * \param len     Length of the text string.
 * \param x       Horizontal position in the frame buffer.
 * \param y       Vertical position in the frame buffer.
 * \param fg      Foreground color.
 * \param bg      Background color.
 * \param scale_x Horizonal scale factor.
 * \param scale_y Vertical scale factor.
 */
L4_CV void
gfxbitmap_font_text_scale(void *fb, l4re_video_view_info_t *vi,
                          gfxbitmap_font_t font, const char *text, unsigned len,
                          unsigned x, unsigned y,
                          gfxbitmap_color_pix_t fg, gfxbitmap_color_pix_t bg,
                          int scale_x, int scale_y);
EXTERN_C_END
/*@}*/
