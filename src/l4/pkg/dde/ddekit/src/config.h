/*
 * This file is part of DDEKit.
 *
 * (c) 2006-2012 Bjoern Doebel <doebel@os.inf.tu-dresden.de>
 *               Christian Helmuth <ch12@os.inf.tu-dresden.de>
 *               Thomas Friebel <tf13@os.inf.tu-dresden.de>
 *     economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

/**
 * \file    ddekit/include/config.h
 * \brief   Configuration file for ddekit.
 */

#pragma once

#define DEBUG_MSG(msg, ...) ddekit_printf("%s: \033[32;1m" msg "\033[0m\n", __FUNCTION__, ##__VA_ARGS__)
