/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2010-2020 Kernkonzept GmbH.
 * Author(s): Alexander Warg <alexander.warg@kernkonzept.com>
 */
#include <l4/re/env>
#include <l4/re/namespace>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/object_registry>
#include <l4/re/util/br_manager>
#include <l4/re/error_helper>

#include <l4/sys/cxx/ipc_server_loop>
#include <l4/cxx/ipc_timeout_queue>

#include "debug.h"
#include "server.h"

class Loop_hooks :
  public L4::Ipc_svr::Timeout_queue_hooks<Loop_hooks, L4Re::Util::Br_manager>,
  public L4::Ipc_svr::Ignore_errors
{
public:
  static l4_kernel_clock_t now()
  { return l4_kip_clock(l4re_kip()); }
};

typedef L4Re::Util::Registry_server<Loop_hooks> Registry_svr;
static Registry_svr *svr()
{
  static Registry_svr server;
  return &server;
}

L4Re::Util::Object_registry *registry;

Internal::Io_svr_init::Io_svr_init()
{ registry = svr()->registry(); }

int server_loop()
{
  svr()->loop();
  return 0;
}

