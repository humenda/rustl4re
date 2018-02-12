/*
 * (c) 2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/re/util/object_registry>
#include <l4/re/env>
#include <l4/re/console>
#include <l4/re/util/cap_alloc>
#include <l4/re/error_helper>
#include <l4/re/rm>
#include <l4/re/dataspace>
#include <l4/re/util/meta>

#include <cstdio>
#include <cstring>
#include <algorithm>

#include "client_fb.h"
#include <l4/mag-gfx/canvas>
#include <l4/mag/server/user_state>
#include <l4/mag/server/factory>

#include "service.h"
#include <l4/sys/kdebug.h>

namespace Mag_server {

void Service::start(Core_api *core)
{
  _core = core;
  if (!reg()->register_obj(cxx::Ref_ptr<Service>(this), "svc").is_valid())
    printf("Service registration failed.\n");
  else
    printf("Plugin: Frame-buffer service started\n");
}


namespace {
  Session::Property_handler const _client_fb_opts[] =
    { { "g",         true,  &Client_fb::set_geometry_prop },
      { "geometry",  true,  &Client_fb::set_geometry_prop },
      { "focus",     false, &Client_fb::set_flags_prop },
      { "shaded",    false, &Client_fb::set_flags_prop },
      { "fixed",     false, &Client_fb::set_flags_prop },
      { "barheight", true,  &Client_fb::set_bar_height_prop },
      { 0, 0, 0 }
    };
};

long
Service::op_create(L4::Factory::Rights, L4::Ipc::Cap<void> &obj,
                   l4_mword_t proto, L4::Ipc::Varg_list<> const &args)
{
  if (!L4::kobject_typeid<L4Re::Console>()->has_proto(proto))
    return -L4_ENODEV;

  cxx::Ref_ptr<Client_fb> x(new Client_fb(_core));
  _core->set_session_options(x.get(), args, _client_fb_opts);
  x->setup();

  _core->register_session(x.get());
  ust()->vstack()->push_top(x.get());
  x->view_setup();

  reg()->register_obj(x);
  x->obj_cap()->dec_refcnt(1);
  obj = L4::Ipc::make_cap(x->obj_cap(), L4_CAP_FPAGE_RWSD);
  return 0;
}


void
Service::destroy()
{
  printf("MAG: destroying the fb_client_service, holla\n");
}

Service::~Service()
{
  printf("MAG: destroy FB svc\n");
}

static Service _fb_service("frame-buffer");

}
