/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2009-2020 Kernkonzept GmbH.
 * Authors: Alexander Warg <warg@os.inf.tu-dresden.de>
 *          Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *          Marcus Haehnel <marcus.haehnel@kernkonzept.com>
 */

#pragma once

#include <l4/sys/compiler.h>

/**
 * Output a string and go into endless loop
 *
 * \param str  The string to print
 */
void L4_NORETURN panic(char const *str);

enum { Max_reservations = 8 };

/**
 * Adds a range of memory to a reservation map.
 *
 * A static reservation map is kept by the program. This function adds memory
 * ranges to that reservation map. The overlaps_reservation function can be used
 * to check whether a memory range intersects with any range in the map.
 *
 * To reduce bookkeeping effort and fragmentation, ranges are extended when
 * added to the whole pages occupied by any part of the range. This means that
 * the start address will be rounded down to the nearest multiple of page size
 * while the size will be rounded up to page size. This helps with adding
 * possibly fragmented data such as MBI information.
 *
 * The map contains space for as many entries as specified in Max_reservations.
 * When no more entries can be allocated, the last entry is alwyas expanded to
 * contain any additionally added reservation and a warning is printed.
 *
 * \param start  start address of the range to be reserved
 * \param size   size of the memory range to reserve.
 */
void
reservation_add(unsigned long start, unsigned long size);

/**
 * Check if a range of memory intersects with any range in the reservation map.
 *
 * If the specified range intersects with any range of memory kept in the
 * internal reservation map, the address of the end of the range (i.e. the first
 * address no longer belonging to the reserved region) is returned.
 *
 * \note The function does not guarantee that the so returned address does not
 *       intersect with any other reservation. It is the job of the caller to
 *       call this function again if necessary.
 *
 * \param start  Pointer to the start of the memory area to check.
 * \param size   Size of the memory area to check.
 *
 * \retval 0     The tested range did not intersect with any reservation.
 * \returns      The first address after the memory range the checked range
 *               intersected with.
 */
unsigned long
overlaps_reservation(void* start, unsigned long size);
