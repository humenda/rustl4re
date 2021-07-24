/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2010-2020 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *            Alexander Warg <warg@os.inf.tu-dresden.de>
 */
#pragma once

#include <l4/cxx/bitfield>
#include <l4/sys/l4int.h>

namespace Hw { namespace Pci {

/**
 * Empty base for a config space capability class.
 *
 * The main purpose of the class is to provide a Pci::Config
 * compatible base for specifying PCI config-space registers,
 * see `R`.
 */
struct Capability
{
  /**
   * Base class for defining a config-space register.
   *
   * \tparam OFS  The offset of the register in bytes relative to
   *              the PCI capability.
   * \tparam T    Datatype of the config-space register. Note, this
   *              should be l4_uint8_t, l4_uint16_t, or l4_uint32_t.
   */
  template<unsigned OFS, typename T> struct R
  {
    enum { Ofs = OFS /**< Offset of the register in bytes */ };
    T v; /**< Value container for the register */
  };
};

struct Pm_cap : Capability
{
  struct Pmc : R<0x02, l4_uint16_t>
  {
    CXX_BITFIELD_MEMBER( 0,  2, version, v);
    CXX_BITFIELD_MEMBER( 3,  3, pme_clock, v);
    CXX_BITFIELD_MEMBER( 5,  5, dsi, v);
    CXX_BITFIELD_MEMBER( 6,  8, aux_current, v);
    CXX_BITFIELD_MEMBER( 9,  9, d1, v);
    CXX_BITFIELD_MEMBER(10, 10, d2, v);
    CXX_BITFIELD_MEMBER(11, 15, pme, v);
    CXX_BITFIELD_MEMBER(11, 11, pme_d0, v);
    CXX_BITFIELD_MEMBER(12, 12, pme_d1, v);
    CXX_BITFIELD_MEMBER(13, 13, pme_d2, v);
    CXX_BITFIELD_MEMBER(14, 14, pme_d3hot, v);
    CXX_BITFIELD_MEMBER(15, 15, pme_d3cold, v);
  };

  struct Pmcsr : R<0x04, l4_uint16_t>
  {
    CXX_BITFIELD_MEMBER( 0,  2, state, v);
    CXX_BITFIELD_MEMBER( 3,  3, no_soft_reset, v);
    CXX_BITFIELD_MEMBER( 8,  8, pme_enable, v);
    CXX_BITFIELD_MEMBER( 9, 12, data_sel, v);
    CXX_BITFIELD_MEMBER(13, 14, data_scale, v);
    CXX_BITFIELD_MEMBER(15, 15, pme_status, v);
  };
};

struct Sr_iov_cap : Capability
{
  enum
  {
    Id   = 0x10,
    Size = 0x40,
  };

  struct Caps : R<4, l4_uint32_t>
  {
    CXX_BITFIELD_MEMBER( 0,  0, vf_migration, v);
    CXX_BITFIELD_MEMBER( 1,  1, ari_preserved, v);
    CXX_BITFIELD_MEMBER(21, 31, vf_migration_msg, v);
  };

  struct Ctrl : R<8, l4_uint16_t>
  {
    CXX_BITFIELD_MEMBER( 0,  0, vf_enable, v);
    CXX_BITFIELD_MEMBER( 1,  1, vf_migration_enable, v);
    CXX_BITFIELD_MEMBER( 2,  2, vf_migration_irq_enable, v);
    CXX_BITFIELD_MEMBER( 3,  3, vf_memory_enable, v);
    CXX_BITFIELD_MEMBER( 4,  4, ari_capable_hierarchy, v);
  };

  struct Status : R<0xA, l4_uint16_t>
  {
    CXX_BITFIELD_MEMBER( 0,  0, vf_migartion_status, v);
  };

  using Initial_vfs  = R<0x0C, l4_uint16_t>;
  using Total_vfs    = R<0x0E, l4_uint16_t>;
  using Num_vfs      = R<0x10, l4_uint16_t>;
  using Fn_dep       = R<0x12, l4_uint8_t>;
  using Vf_offset    = R<0x14, l4_uint16_t>;
  using Vf_stride    = R<0x16, l4_uint16_t>;
  using Vf_device_id = R<0x1A, l4_uint16_t>;
  using Supported_ps = R<0x1C, l4_uint32_t>;
  using System_ps    = R<0x20, l4_uint32_t>;
  using Vf_bar0      = R<0x24, l4_uint32_t>;
  using Vf_migration_state = R<0x3C, l4_uint32_t>;
};

struct Ari_cap : Capability
{
  enum
  {
    Id   = 0x0e,
    Size = 0x08,
  };

  struct Caps : R<0x04, l4_uint16_t>
  {
    CXX_BITFIELD_MEMBER( 0,  0, mfvc_func_groups, v);
    CXX_BITFIELD_MEMBER( 1,  1, acs_func_groups, v);
    CXX_BITFIELD_MEMBER( 8, 15, next_func, v);
  };

  struct Ctrl : R<0x06, l4_uint16_t>
  {
    CXX_BITFIELD_MEMBER( 0,  0, mfvc_func_groups, v);
    CXX_BITFIELD_MEMBER( 1,  1, acs_func_groups, v);
    CXX_BITFIELD_MEMBER( 4,  6, group, v);
  };
};

struct Acs_cap : Capability
{
  enum
  {
    Id = 0x0d,
  };

  struct Caps : R<0x04, l4_uint16_t>
  {
    CXX_BITFIELD_MEMBER( 0, 0, src_validation, v);
    CXX_BITFIELD_MEMBER( 1, 1, translation_blocking, v);
    CXX_BITFIELD_MEMBER( 2, 2, p2p_request_redirect, v);
    CXX_BITFIELD_MEMBER( 3, 3, p2p_completion_redirect, v);
    CXX_BITFIELD_MEMBER( 4, 4, upstream_fwd, v);
    CXX_BITFIELD_MEMBER( 0, 4, f, v);
    CXX_BITFIELD_MEMBER( 5, 5, p2p_egress_ctrl, v);
    CXX_BITFIELD_MEMBER( 6, 6, direct_translated_p2p, v);
    CXX_BITFIELD_MEMBER( 0, 6, features, v);
    CXX_BITFIELD_MEMBER( 8, 15, egress_ctrl_vector_size, v);
  };

  struct Ctrl : R<0x06, l4_uint16_t>
  {
    CXX_BITFIELD_MEMBER( 0, 0, src_validation_enable, v);
    CXX_BITFIELD_MEMBER( 1, 1, translation_blocking_enable, v);
    CXX_BITFIELD_MEMBER( 2, 2, p2p_request_redirect_enable, v);
    CXX_BITFIELD_MEMBER( 3, 3, p2p_completion_redirect_enable, v);
    CXX_BITFIELD_MEMBER( 4, 4, upstream_fwd_enable, v);
    CXX_BITFIELD_MEMBER( 0, 4, f, v);
    CXX_BITFIELD_MEMBER( 5, 5, p2p_egress_ctrl_enable, v);
    CXX_BITFIELD_MEMBER( 6, 6, direct_translated_p2p_enable, v);
    CXX_BITFIELD_MEMBER( 0, 6, enabled, v);
  };
};

} }

