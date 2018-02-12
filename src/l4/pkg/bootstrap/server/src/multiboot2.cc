/*
 * Copyright (C) 2017 Kernkonzept GmbH.
 * Author: Jakub Jermar <jakub.jermar@kernkonzept.com>
 *
 * This file is distributed under the terms of the GNU General Public
 * License, version 2. Please see the COPYING-GPL-2 file for details.
 */

// This file contains code that enables multiboot2 boots. With many other parts
// of the system depending on multiboot (v1) data structures and with the goal
// of making a low-impact change, the method chosen is to extract essential
// information from the multiboot2 info structure and convert it into a v1
// structure as soon as possible so that the rest of the bootstrap code remains
// untouched.

#include <l4/util/mb_info.h>
#include <l4/sys/l4int.h>
#include <l4/sys/consts.h>
#include <l4/cxx/minmax>

#include <cstring>

#include <assert.h>
#include <stddef.h>

// This class can be used to convert a multiboot2 information buffer into a
// multiboot information structure and associated data structures. It works in
// place and does not allocate additional memory except temporary stack space.
//
// The space for the multiboot info structure comes from the multiboot2 tag
// headers. There must be enough tags (luckily this is under our control) to
// create enough space. Memory for the other objects, such as strings, module
// arrays and memory maps comes from the multiboot2 tag payloads (i.e.  the
// multiboot2 payloads are rotated to the end of the buffer and the buffer is
// shrunk accordingly).
class Mbi2_convertible_buffer
{
public:
  Mbi2_convertible_buffer(l4util_mb2_info_t *mbi2)
  : _buf(reinterpret_cast<char *>(mbi2)),
    _size(mbi2->total_size),
    _total_size(mbi2->total_size),
    _mbi({})
  {}

  l4util_mb2_tag_t *begin()
  {
    return reinterpret_cast<l4util_mb2_tag_t *>(_buf
                                                + sizeof(l4util_mb2_info_t));
  }

  void process_cmdline(l4util_mb2_tag_t *tag)
  {
    auto dst_tag = rotate_to_end(tag);
    reserve_from_end(dst_tag->size - offsetof(l4util_mb2_tag_t, cmdline));

    _mbi.flags |= L4UTIL_MB_CMDLINE;
    _mbi.cmdline = reinterpret_cast<l4_uint32_t>(dst_tag->cmdline.string);
  }

  // Module tags must appear in a consecutive block (they may not be interleaved
  // by some other tag type)
  void process_modules(l4util_mb2_tag_t *tag)
  {
    std::size_t cnt = 0;

    while (tag->type == L4UTIL_MB2_MODULE_INFO_TAG)
      {
        cnt++;
        auto dst_tag = rotate_to_end(tag);

        // Reserve only the command line
        reserve_from_end(dst_tag->size
                         - (offsetof(l4util_mb2_tag_t, module)
                            + offsetof(l4util_mb2_module_tag_t, string)));

        // Convert to v1 entry
        // Both versions happen to be the same size without the module name
        l4util_mb_mod_t mod = {.mod_start = dst_tag->module.mod_start,
                               .mod_end = dst_tag->module.mod_end,
                               .cmdline = reinterpret_cast<l4_uint32_t>(
                                 dst_tag->module.string),
                               .pad = 0};
        *reinterpret_cast<l4util_mb_mod_t *>(dst_tag) = mod;
      }

    if (cnt)
      {
        // Reserve the converted v1 structures
        // They happen to be adjacent at the end of the buffer
        reserve_from_end(sizeof(l4util_mb_mod_t) * cnt);

        _mbi.flags |= L4UTIL_MB_MODS;
        _mbi.mods_count = cnt;
        _mbi.mods_addr = reinterpret_cast<l4_uint32_t>(end());
      }
  }

  void process_memmap(l4util_mb2_tag_t *tag)
  {
    auto dst_tag = rotate_to_end(tag);
    auto size = dst_tag->size - (offsetof(l4util_mb2_tag_t, memmap)
                                 + offsetof(l4util_mb2_memmap_tag_t, entries));

    auto entry_size = dst_tag->memmap.entry_size;

    // Convert to v1 entries
    for (auto entry = dst_tag->memmap.entries;
         reinterpret_cast<l4_addr_t>(entry)
         < reinterpret_cast<l4_addr_t>(dst_tag) + dst_tag->size;)
      {
        auto next = reinterpret_cast<decltype(entry)>(
           reinterpret_cast<l4_addr_t>(entry) + entry_size);

        // Each v1 entry has the same length as the corresponding v2 entry
        l4util_mb_addr_range_t range = {.struct_size =
                                          entry_size - sizeof(range.struct_size),
                                        .addr = entry->base_addr,
                                        .size = entry->length,
                                        .type = entry->type};

        *reinterpret_cast<decltype(range) *>(entry) = range;
        entry = next;
      }

    reserve_from_end(size);

    _mbi.flags |= L4UTIL_MB_MEM_MAP;
    _mbi.mmap_length = size;
    _mbi.mmap_addr = reinterpret_cast<l4_addr_t>(end());
  }

  void process_rsdp(l4util_mb2_tag_t *tag, l4_uint32_t *rsdp_start,
                    l4_uint32_t *rsdp_end)
  {
    auto dst_tag = rotate_to_end(tag);
    auto size = dst_tag->size - offsetof(l4util_mb2_tag_t, rsdp);

    reserve_from_end(size);

    *rsdp_start = reinterpret_cast<l4_uint32_t>(end());
    *rsdp_end = reinterpret_cast<l4_uint32_t>(end() + size);
  }

  // Finalize conversion from multiboot2 to multiboot info buffer
  void finalize()
  {
    assert(sizeof(_mbi) <= _size);
    std::memcpy(_buf, &_mbi, sizeof(_mbi));
    std::memset(_buf + sizeof(_mbi), 0, _size - sizeof(_mbi));
  }

private:
  char *end() { return _buf + _size; }

  l4util_mb2_tag_t *rotate_to_end(l4util_mb2_tag_t *tag)
  {
    char buf[1024];

    std::size_t size = l4_round_size(tag->size, L4UTIL_MB2_TAG_ALIGN_SHIFT);
    l4util_mb2_tag_t *dst_tag =
      reinterpret_cast<l4util_mb2_tag_t *>(end() - size);
    char *_src = reinterpret_cast<char *>(tag);

    while (size)
      {
        std::size_t copied = cxx::min(sizeof(buf), size);
        char *_dst = end() - copied;
        std::memcpy(buf, _src, copied);
        std::memmove(_src, _src + copied, (end() - _src) - copied);
        std::memcpy(_dst, buf, copied);
        size -= copied;
      }

    return dst_tag;
  }

  void reserve_from_end(std::size_t size)
  {
    size = l4_round_size(size, L4UTIL_MB2_TAG_ALIGN_SHIFT);
    assert(_size >= size);
    _size -= size;
  }

  char *_buf;
  std::size_t _size;
  const std::size_t _total_size;

  l4util_mb_info_t _mbi;
};

extern l4_uint32_t rsdp_start;
extern l4_uint32_t rsdp_end;

extern "C" void _multiboot2_to_multiboot(l4util_mb2_info_t *);

void _multiboot2_to_multiboot(l4util_mb2_info_t *mbi2)
{
  Mbi2_convertible_buffer buf(mbi2);

  l4util_mb2_tag_t *tag = buf.begin();

  while (tag->type != L4UTIL_MB2_TERMINATOR_INFO_TAG)
    {
      switch (tag->type)
        {
        case L4UTIL_MB2_BOOT_CMDLINE_INFO_TAG:
          buf.process_cmdline(tag);
          break;
        case L4UTIL_MB2_MODULE_INFO_TAG:
          buf.process_modules(tag);
          break;
        case L4UTIL_MB2_MEMORY_MAP_INFO_TAG:
          buf.process_memmap(tag);
          break;
        case L4UTIL_MB2_RSDP_OLD_INFO_TAG:
        case L4UTIL_MB2_RSDP_NEW_INFO_TAG:
          buf.process_rsdp(tag, &rsdp_start, &rsdp_end);
          break;
        default:
          tag = reinterpret_cast<l4util_mb2_tag_t *>(
            reinterpret_cast<char *>(tag)
            + l4_round_size(tag->size, L4UTIL_MB2_TAG_ALIGN_SHIFT));
          break;
        }
    }

  buf.finalize();
}

