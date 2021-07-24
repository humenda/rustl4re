/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2020 Kernkonzept GmbH.
 * Author(s): Alexander Warg <alexander.warg@kernkonzept.com>
 */
#pragma once

#include <l4/re/util/object_registry>

L4Re::Util::Object_registry *irq_queue();
