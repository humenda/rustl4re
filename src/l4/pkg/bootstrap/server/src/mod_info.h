#pragma once

/// Info for each module
struct Mod_info
{
  char const *start;
  unsigned size;
  unsigned size_uncompressed;
  char const *name;
  char const *cmdline;
  char const *md5sum_compr;
  char const *md5sum_uncompr;
} __attribute__((packed));
