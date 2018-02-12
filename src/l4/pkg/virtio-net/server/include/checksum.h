/*
 * Copyright (C) 2018 Kernkonzept GmbH.
 * Author(s): Andreas Wiese <andreas.wiese@kernkonzept.com>
 *
 * This file is distributed under the terms of the GNU General Public
 * License, version 2.  Please see the COPYING-GPL-2 file for details.
 */

#pragma once

#include <l4/sys/l4int.h>

/**
 * A network checksum.
 *
 * This class calculates a 1's complement checksum over the data
 * incrementally provided to it.
 */
class Net_checksum
{
private:
  l4_uint32_t _sum;

public:
  /**
   * Initialise a network checksum with start value 0.
   */
  Net_checksum() : _sum(0) {}

  /**
   * Reset the checksum to its initial start value.
   */
  void reset()
  { _sum = 0; }

  /**
   * Add a given data buffer to the checksum.
   *
   * \param buf  A data buffer to be added to the checksum.
   * \param len  The size of the data buffer.
   *
   * This adds the given data buffer to the current checksum.
   */
  void add(l4_uint8_t const *buf, size_t len)
  {
    for (; len > 1; len -= 2, buf += 2)
      _sum += ((l4_uint16_t)buf[0] << 8) | buf[1];

    if (len > 0)
      _sum += *buf << 8;
  }

  /**
   * Finalize the checksum value.
   *
   * \retval The final checksum value.
   *
   * This adds the carry of the current checksum value to the checksum
   * and returns the final (negated) checksum value.
   */
  l4_uint16_t finalize()
  {
    while (_sum >> 16)
      _sum = (_sum & 0xffffU) + (_sum >> 16);

    return ~_sum & 0xffffU;
  }
};
