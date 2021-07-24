/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2020 Kernkonzept GmbH.
 * Author(s): Sarah Hoffmann <sarah.hoffmann@kernkonzept.com>
 *
 */
/*
 * \brief   Just reboot
 * \date    2006-03
 * \author  Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 */

/*
 * (c) 2006-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/re/env.h>
#include <l4/sys/platform_control.h>

int main(void)
{
  l4_cap_idx_t pfc = l4re_env_get_cap("pfc");

  if (l4_is_valid_cap(pfc))
    l4_platform_ctl_system_shutdown(pfc, 1);

  return 0;
}
