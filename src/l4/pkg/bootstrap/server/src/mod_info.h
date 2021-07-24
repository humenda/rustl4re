#pragma once

#define BOOTSTRAP_MOD_INFO_MAGIC_HDR "<< L4Re-bootstrap-modinfo-hdr >>"
#define BOOTSTRAP_MOD_INFO_MAGIC_MOD "<< L4Re-bootstrap-modinfo-mod >>"

/*
 * Flags for a module
 *
 * Bits 0..2: Type of module
 *            0:   unspecified, any module
 *            1:   kernel
 *            2:   sigma0
 *            3:   roottask
 *            4-7: reserved
 */

enum Mod_info_flags
{
  Mod_info_flag_mod_unspec    = 0,
  Mod_info_flag_mod_kernel    = 1,
  Mod_info_flag_mod_sigma0    = 2,
  Mod_info_flag_mod_roottask  = 3,
  Mod_info_flag_mod_mask      = 7 << 0,

  Mod_info_flag_direct_addressing   = 1 << 4,
  Mod_info_flag_relative_addressing = 0 << 4, //< Relative to Mod_info structure
};

enum Mod_header_flags
{
  Mod_header_flag_direct_addressing   = 1 << 4, //< at the end of this conversion we're not using it anymore -- so remove it
  Mod_header_flag_relative_addressing = 0 << 4,
};

/// Info for each module
struct Mod_info
{
  char magic[32];
  unsigned flags;

  unsigned long long start;
  unsigned size;
  unsigned size_uncompressed;
  unsigned long long name;
  unsigned long long cmdline;
  unsigned long long md5sum_compr;
  unsigned long long md5sum_uncompr;
  unsigned long long attrs; // relative to this structure
} __attribute__((packed));

struct Mod_header
{
  char magic[32];
  unsigned num_mods;
  unsigned flags;
  unsigned long long mbi_cmdline;
  unsigned long long mods;
} __attribute__((packed));

static inline char *
mod_info_hdr_offs_to_charp(struct Mod_header *hdr,
                           unsigned long long offset)
{
  return (char *)((char *)hdr + offset);
}

/* Header */

static inline char *
mod_info_mbi_cmdline(struct Mod_header *hdr)
{
  if (hdr->flags & Mod_header_flag_direct_addressing)
    return (char *)(unsigned long)hdr->mbi_cmdline;
  return mod_info_hdr_offs_to_charp(hdr, hdr->mbi_cmdline);
}

static inline struct Mod_info *
mod_info_mods(struct Mod_header *hdr)
{
  if (hdr->flags & Mod_header_flag_direct_addressing)
    return (struct Mod_info *)(unsigned long)hdr->mods;
  return (struct Mod_info *)mod_info_hdr_offs_to_charp(hdr, hdr->mods);
}

/* Modules */
static inline unsigned long long
mod_info_mod_ull(struct Mod_info *mi, unsigned long long v)
{
  if (mi->flags & Mod_info_flag_direct_addressing)
    return v;
  return (unsigned long long)(unsigned long)mi + v;
}
