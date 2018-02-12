/*
 * Copyright (C) 2018 Kernkonzept GmbH.
 * Author(s): Sarah Hoffmann <sarah.hoffmann@kernkonzept.com>
 *
 * This file is distributed under the terms of the GNU General Public
 * License, version 2.  Please see the COPYING-GPL-2 file for details.
 */

#include <l4/re/env>
#include <l4/sys/arm_smccc>

#include "hw_device.h"

class Optee_memory : public Hw::Device
{
  enum
  {
    Smc_call_trusted_os_uid      = 0xbf00ff01,
    Smc_call_trusted_os_revision = 0xbf00ff03,
    Optee_call_get_shm_config    = 0xb2000007,
    Optee_call_exchange_caps     = 0xb2000009,

    Optee_uuid_word0 = 0x384fb3e0,
    Optee_uuid_word1 = 0xe7f811e3,
    Optee_uuid_word2 = 0xaf630002,
    Optee_uuid_word3 = 0xa5d5c51b,

    Optee_api_major = 2,
    Optee_api_minor = 0
  };

public:
  Optee_memory()
  {
    set_hid("optee");
    add_cid("optee-mem");
  }

  void init()
  {
    Hw::Device::init();

    auto smc = L4Re::Env::env()->get_cap<L4::Arm_smccc>("arm_smc");

    if (!smc)
      {
        d_printf(DBG_WARN, "OP-TEE: SMC capability 'arm_smc' not found, disabling.\n");
        return;
      }

    l4_umword_t p[4];

    // check for OP-TEE OS
    long ret = fast_call(smc, Smc_call_trusted_os_uid, p);

    if (ret < 0 || p[0] != Optee_uuid_word0 || p[1] != Optee_uuid_word1 ||
        p[2] != Optee_uuid_word2 || p[3] != Optee_uuid_word3)
      {
        d_printf(DBG_WARN, "OP-TEE: OP-TEE not detected.\n");
        return;
      }

    // check for correct API version
    ret = fast_call(smc, Smc_call_trusted_os_revision, p);

    if (ret < 0 || p[0] != Optee_api_major || p[1] != Optee_api_minor)
      {
        d_printf(DBG_WARN, "OP-TEE: OS has wrong API (%ld.%ld). Need %d.%d.\n",
                 p[0], p[1], Optee_api_major, Optee_api_minor);
        return;
      }

    // check if the OS exports memory
    ret = fast_call(smc, Optee_call_exchange_caps, p);

    if (ret < 0 || p[0] || !(p[1] & 1))
      {
        d_printf(DBG_WARN, "OP-TEE: OP-TEE does not export shared memory.\n");
        return;
      }

    // get the memory area
    ret = fast_call(smc, Optee_call_get_shm_config, p);

    if (ret < 0 || p[0])
      {
        d_printf(DBG_WARN, "OP-TEE: failed to get shared memory configuration from OP-TEE.\n");
        return;
      }

    unsigned long flags = 0;
    if (p[3] & 1)
      flags |= Resource::F_prefetchable | Resource::F_cached_mem;

    Resource *r = new Resource(Resource::Mmio_res, flags, p[1],
                               p[1] + p[2] - 1);
    add_resource(r);

    d_printf(DBG_DEBUG, "OP-TEE module initialized successfully.\n");
  }

private:
  long fast_call(L4::Cap<L4::Arm_smccc> cap, l4_umword_t func, l4_umword_t out[])
  {
    return l4_error(cap->call(func, 0, 0, 0, 0, 0, 0,
                              out, out + 1, out + 2, out + 3, 0));
  }
};

static Hw::Device_factory_t<Optee_memory> __hw_optee_factory("Optee_memory");
