/**
 * \file
 */
/*
 * (c) 2014 Matthias Lange <matthias.lange@kernkonzept.com>
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU Lesser General Public License 2.1.
 * Please see the COPYING-LGPL-2.1 file for details.
 */
#pragma once

#include <l4/sys/compiler.h>

/**
 * \defgroup libedid EDID parsing functionality
 * @{
 */

/**
 * \brief EDID constants
 */
enum Libedid_consts
{
  Libedid_block_size = 128, ///< Size of one EDID block in bytes
};

__BEGIN_DECLS

/**
 * \brief Check for valid EDID header
 *
 * @param edid Pointer to a 128byte EDID block
 * @return 0 if the header is correct, -EINVAL otherwise
 */
int libedid_check_header(const unsigned char *edid);

/**
 * \brief Calculates the EDID checksum
 *
 * @param edid Pointer to a 128byte EDID block
 * @return 0 if checksum is correct, -EINVAL otherwise
 */
int libedid_checksum(const unsigned char *edid);

/**
 * \brief Returns the EDID version number
 *
 * @param edid Pointer to a 128byte EDID block
 * @return Version number
 */
unsigned libedid_version(const unsigned char *edid);

/**
 * \brief Returns the EDID revision number
 *
 * @param edid Pointer to a 128 EDID block
 * @return Revision number
 */
unsigned libedid_revision(const unsigned char *edid);

/**
 * \brief Extracts the display's PnP ID
 *
 * @param edid Pointer to a 128byte EDID block
 * @retval id Return the PnP id. Must point to 4 bytes.
 */
void libedid_pnp_id(const unsigned char *edid, unsigned char *id);

/**
 * \brief Extract the display's prefered mode
 *
 * @param edid Pointer to a 128byte EDID block
 * @retval w X resolution of prefered video mode in pixels.
 * @retval h Y resolution of prefered video mode in pixels.
 */
void libedid_prefered_resolution(const unsigned char *edid,
                                 unsigned *w, unsigned *h);

/**
 * \brief Get the number of EDID extension blocks
 *
 * @param edid Pointer to a 128byte EDID block
 * @return Number of EDID extension blocks
 */
unsigned libedid_num_ext_blocks(const unsigned char *edid);

/**
 * \brief Dump the standard timings to stdout
 *
 * @param edid Pointer to a 128byte EDID block
 * @return Number of standard timings stored in EDID
 */
unsigned libedid_dump_standard_timings(const unsigned char *edid);

/**
 * \brief Dump raw EDID data to stdout
 *
 * @param edid Pointer to a 128byte EDID block
 */
void libedid_dump(const unsigned char *edid);

/**@}*/

__END_DECLS
