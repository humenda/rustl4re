// SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom
/*
 * Copyright (C) 2019 Kernkonzept GmbH.
 * Author(s): Frank Mehnert <frank.mehnert@kernkonzept.com>
 *
 * This file is distributed under the terms of the GNU General Public
 * License, version 2. Please see the COPYING-GPL-2 file for details.
 */

#include "cpg_rcar3.h"

// Static constexpr data member declarations aren't definitions in C++11/14.
constexpr unsigned Rcar3_cpg::rmstpcr[Nr_modules];
constexpr unsigned Rcar3_cpg::smstpcr[Nr_modules];
constexpr unsigned Rcar3_cpg::mstpsr[Nr_modules];
