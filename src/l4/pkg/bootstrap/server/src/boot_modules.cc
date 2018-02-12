#include "support.h"
#include "panic.h"
#include <cassert>
#include "mod_info.h"
#ifdef COMPRESS
#include "uncompress.h"
#endif

static char const Mod_reg[] = ".Module";
char const *const Boot_modules::Mod_reg = ::Mod_reg;

// get c if it is printable '.' else
static char
get_printable(int c)
{
  if (c < 32 || c >= 127)
    return '.';
  return c;
}
Region
Boot_modules::mod_region(unsigned index, l4_addr_t start, l4_addr_t size,
                         Region::Type type)
{
  return Region::start_size(start, size, ::Mod_reg,
                            type, index);
}

void
Boot_modules::merge_mod_regions()
{
  Region_list *regions = mem_manager->regions;
  for (Region *r = regions->begin(); r != regions->end(); ++r)
    {
      if (r->name() == ::Mod_reg)
        r->sub_type(0);
    }

  regions->optimize();
}


/**
 * \brief move a boot module from src to dest.
 * \param i The module index
 * \param dest The destination address
 * \param src  The source address
 * \param size The size of the module in bytes
 * \param overlap_check if true check for overlapping regions at
 *                      the destination.
 *
 * The src and dest buffers may overlap, at the destination set size is
 * rounded to the next page boundary.
 */
void
Boot_modules::_move_module(unsigned i, void *_dest,
                           void const *_src, unsigned long size,
                           bool overlap_check)
{
  char *dest = (char *)_dest;
  char const *src = (char const *)_src;

  if (src == dest)
    {
      mem_manager->regions->add(Region::n(dest, dest + size,
                                          ::Mod_reg, Region::Root, i));
      return;
    }

  auto p = Platform_base::platform;
  char const *vsrc = (char const *)p->to_virt((l4_addr_t)src);
  char *vdest = (char *)p->to_virt((l4_addr_t)dest);

  if (Verbose_load)
    {
      unsigned char c[5];
      c[0] = get_printable(vsrc[0]);
      c[1] = get_printable(vsrc[1]);
      c[2] = get_printable(vsrc[2]);
      c[3] = get_printable(vsrc[3]);
      c[4] = 0;
      printf("  moving module %02d { %lx, %lx } (%s) -> { %lx - %lx } [%ld]\n",
             i, (unsigned long)src, (unsigned long)src + size - 1, c,
             (unsigned long)dest, (unsigned long)dest + size - 1, size);

      for (int a = 0; a < 0x100; a += 4)
        printf("%08x%s", *(unsigned *)(vsrc + a), (a % 32 == 28) ? "\n" : " ");
      printf("\n");
    }
  else
    printf("  moving module %02d { %lx-%lx } -> { %lx-%lx } [%ld]\n",
           i, (unsigned long)src, (unsigned long)src + size - 1,
           (unsigned long)dest, (unsigned long)dest + size - 1, size);

  if (!mem_manager->ram->contains(dest))
    panic("Panic: Would move outside of RAM");

  if (overlap_check)
    {
      Region *overlap = mem_manager->regions->find(dest);
      if (overlap)
        {
          printf("ERROR: module target [%p-%p) overlaps\n",
                 dest, dest + size - 1);
          overlap->vprint();
          mem_manager->regions->dump();
          panic("cannot move module");
        }
    }
  memmove(vdest, vsrc, size);
  char *x = vdest + size;
  memset(x, 0, l4_round_page(x) - x);
  mem_manager->regions->add(Region::n(dest, dest + size,
                                      ::Mod_reg, Region::Root, i));
}

/// sorter data for up to 256 modules (stores module indexes)
static unsigned short mod_sorter[256];
/// the first unused entry of the sorter
static unsigned short const *mod_sorter_end = mod_sorter;

/// add v to the sorter (using insert sort)
template< typename CMP > void
mod_insert_sorted(unsigned short v, CMP const &cmp)
{
  enum { Max_mods = sizeof(mod_sorter) / sizeof(mod_sorter[0]) };

  if (mod_sorter_end >= mod_sorter + Max_mods)
    panic("too much modules for module sorter");

  unsigned short *i = mod_sorter;
  unsigned short const *const e = mod_sorter_end;

  for (; i < e; ++i)
    {
      if (cmp(v, *i))
        {
          memmove(i + 1, i, (e - i) * sizeof(*i));
          break;
        }
    }
  *i = v;
  ++mod_sorter_end;
}

/// Compare start addresses for two modules
struct Mbi_mod_cmp
{
  Boot_modules *m;
  Mbi_mod_cmp(Boot_modules *m) : m(m) {}
  bool operator () (unsigned l, unsigned r) const
  { return m->module(l, false).start < m->module(r, false).start; }
};

/// calculate the total size of all modules (rounded to the next p2align)
static unsigned long
calc_modules_size(Boot_modules *mbi, unsigned p2align,
                  unsigned min = 0, unsigned max = ~0U)
{
  unsigned long s = 0;
  unsigned cnt = mbi->num_modules();
  for (unsigned i = min; i < max && i < cnt; ++i)
    s += l4_round_size(mbi->module(i).size(), p2align);

  return s;
}


/**
 * Move modules to another address.
 *
 * Source and destination regions may overlap.
 */
void
Boot_modules::move_modules(unsigned long modaddr)
{
  unsigned count = num_modules();

  // Remove regions for module contents from region list.
  // The memory for the boot modules is marked as reserved up to now,
  // however to not collide with the ELF binaries to be loaded we drop those
  // regions here and add new reserved regions in move_modules() afterwards.
  // NOTE: we must be sure that we do not need to allocate any memory from here
  // to move_modules()
  for (Region *i = mem_manager->regions->begin();
      i < mem_manager->regions->end();)
    if (i->name() == ::Mod_reg)
      i = mem_manager->regions->remove(i);
    else
      ++i;

  printf("  Moving up to %d modules behind %lx\n", count, modaddr);
  unsigned long req_size = calc_modules_size(this, L4_PAGESHIFT);

  // find a spot to insert the modules
  mem_manager->ram->sort();
  char *to = (char *)mem_manager->find_free_ram(req_size, modaddr);
  if (!to)
    {
      printf("fatal: could not find free RAM region for modules\n"
             "       need %lx bytes above %lx\n", req_size, modaddr);
      mem_manager->ram->dump();
      mem_manager->regions->dump();
      exit (5);
    }

  // sort the modules according to the start address
  for (unsigned i = 0; i < count; ++i)
    mod_insert_sorted(i, Mbi_mod_cmp(this));

  // move modules around ...
  // The goal is to move all modules in a contiguous region in memory.
  // The idea is to keep the order of the modules in memory and compact them
  // into our target region. Therefore we split the modules into two groups.
  // The first group needs to be moved to higher memory addresses to get to
  // the target location and the second group needs to be moved backwards.

  // Let's find the first module that needs to be moved backwards and record
  // target address for this module.
  unsigned end_fwd; // the index of the first module that needs to move backwards
  char *cursor = to; // the target address for the first backwards module
  for (end_fwd = 0; end_fwd < count; ++end_fwd)
    {
      Module mod = module(mod_sorter[end_fwd]);
      if (cursor < mod.start)
        break;

      cursor += l4_round_page(mod.size());
    }

  // end_fwd is now the index of the first module that needs to move backwards
  // cursor cursor is now the target address for the first backwards module

  // move modules forwards, in reverse order so that overlapping is handled
  // properly
  char *end_fwd_addr = cursor;
  for (unsigned i = end_fwd; i > 0; --i)
    {
      Module mod = module(mod_sorter[i - 1]);
      end_fwd_addr -= l4_round_page(mod.size());
      move_module(mod_sorter[i - 1], end_fwd_addr);
    }

  // move modules backwards, in normal order so that overlapping is handled
  // properly
  for (unsigned i = end_fwd; i < count; ++i)
    {
      Module mod = module(mod_sorter[i]);
      move_module(mod_sorter[i], cursor);
      cursor += l4_round_page(mod.size());
    }

  merge_mod_regions();
}


/*
 * Code for IMAGE_MODE, modules are linked into bootstrap
 */
#ifdef IMAGE_MODE

/// array of all module headers
extern Mod_info _module_info_start[];
/// the end of the module headers
extern Mod_info _module_info_end[];

namespace {
/*
 * Helper functions for modules
 */

/// number of linked modules
static inline unsigned _num_modules()
{ return _module_info_end - _module_info_start; }

static inline char const *mod_cmdline(Mod_info const *mod)
{ return (char const *)(l4_addr_t)mod->cmdline; }

static inline char const *mod_name(Mod_info const *mod)
{ return (char const *)(l4_addr_t)mod->name; }

static inline char const *mod_start(Mod_info const *mod)
{ return mod->start; }

static inline Region mod_region(Mod_info const *mod, bool round = false,
                                Region::Type type = Region::Boot)
{
  unsigned long sz = mod->size;
  if (round)
    sz = l4_round_page(sz);
  return Region::array(mod_start(mod), sz,
                       ::Mod_reg,
                       type, mod - _module_info_start);
}

#ifdef COMPRESS // only used with compression
static bool
drop_mod_region(Mod_info const *mod)
{
  // remove the module region for now
  for (Region *r = mem_manager->regions->begin();
       r != mem_manager->regions->end();)
    {
      if (r->name() == ::Mod_reg
          && r->sub_type() == mod - _module_info_start)
        {
          mem_manager->regions->remove(r);
          return true;
        }
      else
        ++r;
    }

  return false;
}
#endif

static inline char const *mod_md5compr(Mod_info const *mod)
{ return (char const *)(l4_addr_t)mod->md5sum_compr; }

static inline char const *mod_md5(Mod_info const *mod)
{ return (char const *)(l4_addr_t)mod->md5sum_uncompr; }

static inline bool mod_compressed(Mod_info const *mod)
{ return mod->size != mod->size_uncompressed; }

static inline void
print_mod(Mod_info const *mod)
{
  unsigned index = mod - _module_info_start;
  printf("  mod%02u: %8p-%8p: %s\n",
         index, mod_start(mod), mod_start(mod) + mod->size,
         mod_name(mod));
}

#ifdef DO_CHECK_MD5
#include <bsd/md5.h>

static void check_md5(const char *name, u_int8_t *start, unsigned size,
                      const char *md5sum)
{
  MD5_CTX md5ctx;
  unsigned char digest[MD5_DIGEST_LENGTH];
  char s[MD5_DIGEST_STRING_LENGTH];
  static const char hex[] = "0123456789abcdef";
  int j;

  printf("  Checking checksum of %s ... ", name);

  MD5Init(&md5ctx);
  MD5Update(&md5ctx, start, size);
  MD5Final(digest, &md5ctx);

  for (j = 0; j < MD5_DIGEST_LENGTH; j++)
    {
      s[j + j] = hex[digest[j] >> 4];
      s[j + j + 1] = hex[digest[j] & 0x0f];
    }
  s[j + j] = '\0';

  if (strcmp(s, md5sum))
    panic("\nmd5sum mismatch");
  else
    printf("Ok.\n");
}
#else // DO_CHECK_MD5
static inline void check_md5(const char *, u_int8_t *, unsigned, const char *)
{}
#endif // ! DO_CHECK_MD5

#ifdef COMPRESS
static void
decompress_mod(Mod_info *mod, l4_addr_t dest, Region::Type type = Region::Boot)
{
  unsigned long dest_size = mod->size_uncompressed;
  if (!dest)
    panic("fatal: cannot decompress module: %s (no memory)\n",
          mod_name(mod));

  if (!mem_manager->ram->contains(Region::n(dest, dest + dest_size)))
    panic("fatal: module %s does not fit into RAM", mod_name(mod));

  l4_addr_t image =
    (l4_addr_t)decompress(mod_name(mod), (void*)mod_start(mod),
                          (void*)dest, mod->size, mod->size_uncompressed);
  if (image != dest)
    panic("fatal cannot decompress module: %s (decompression error)\n",
          mod_name(mod));

  drop_mod_region(mod);

  mod->start = (char *)dest;
  mod->size = mod->size_uncompressed;
  mem_manager->regions->add(mod_region(mod, true, type));
}
#endif // COMPRESS
}


void
Boot_modules_image_mode::reserve()
{
  for (Mod_info *m = _module_info_start; m != _module_info_end; ++m)
    {
      m->start = (char const *)Platform_base::platform->to_phys((l4_addr_t)m->start);
      mem_manager->regions->add(::mod_region(m));
    }
}


/// Get module at index (decompress if needed)
Boot_modules_image_mode::Module
Boot_modules_image_mode::module(unsigned index, bool uncompress) const
{
  // want access to the module, if we have compression we need to decompress
  // the module first
  Mod_info *mod = _module_info_start + index;
#ifdef COMPRESS
  // we currently assume a module as compressed when the size != size_compressed
  if (uncompress && mod_compressed(mod))
    {
      check_md5(mod_name(mod), (u_int8_t *)mod_start(mod),
                mod->size, mod_md5compr(mod));

      unsigned long dest_size = l4_round_page(mod->size_uncompressed);
      decompress_mod(mod, mem_manager->find_free_ram_rev(dest_size));

      check_md5(mod_name(mod), (u_int8_t *)mod_start(mod),
                mod->size, mod_md5(mod));
    }
#else
  (void)uncompress;
#endif
  Module m;
  m.start   = mod->start;
  m.end     = mod->start + mod->size;
  m.cmdline = mod_name(mod);
  return m;
}

unsigned
Boot_modules_image_mode::num_modules() const
{
  return _num_modules();
}

#ifdef COMPRESS
static void
decomp_move_mod(Mod_info *mod, char *destbuf)
{
  if (mod_compressed(mod))
    decompress_mod(mod, (l4_addr_t)destbuf, Region::Root);
  else
    {
      memmove(destbuf, mod_start(mod), mod->size_uncompressed);
#if 0 // cannot simply zero this out, this might overlap with
      // the next module to decompress
      l4_addr_t dest_size = l4_round_page( mod->size_uncompressed);

      if (dest_size > mod->size_uncompressed)
        memset((void *)(destbuf + mod->size_uncompressed), 0,
            dest_size -  mod->size_uncompressed);
#endif
      drop_mod_region(mod);
      mod->start = destbuf;
      mem_manager->regions->add(mod_region(mod, true, Region::Root));
    }
  print_mod(mod);
}

void
Boot_modules_image_mode::decompress_mods(unsigned mod_count, unsigned skip,
                                         l4_addr_t total_size, l4_addr_t mod_addr)
{
  Mod_info *mod_info = _module_info_start;
  Region_list *regions = mem_manager->regions;
  assert (mod_count > skip);

  printf("Compressed modules:\n");
  for (Mod_info const *mod = mod_info; mod != _module_info_end;
       ++mod)
    if (mod_compressed(mod))
      print_mod(mod);

  // sort the modules according to the start address
  for (unsigned i = skip; i < mod_count; ++i)
    mod_insert_sorted(i, Mbi_mod_cmp(this));

  // possibly decompress directly behind the end of the first module
  // We can do this, when we start decompression from the last module
  // and ensure that no decompressed module overlaps its compressed data
  Mod_info const *m0 = &mod_info[mod_sorter[0]];
  char const *rdest = l4_round_page(mod_start(m0) + m0->size);
  char const *ldest = l4_trunc_page(mod_start(m0) - m0->size_uncompressed);

  // try to find a free spot for decompressing the modules
  char *destbuf = (char *)mem_manager->find_free_ram(total_size, mod_addr);
  bool fwd = true;

  if (!destbuf || destbuf > rdest)
    {
      // we found free memory behind the compressed modules, we try to find a
      // spot overlapping with the compressed modules, to safe contiguous
      // memory
      char const *rpos = rdest;
      char const *lpos = ldest;
      for (unsigned i = 0; i < mod_count - skip; ++i)
        {
          Mod_info const *mod = &mod_info[mod_sorter[i]];
          char const *mstart = mod_start(mod);
          char const *mend = mod_start(mod) + mod->size;
          // remove the module region for now
          for (Region *r = regions->begin(); r != regions->end();)
            {
              if (r->name() == ::Mod_reg && r->sub_type() == mod - mod_info)
                r = regions->remove(r);
              else
                ++r;
            }

          if (rpos < mend)
            {
              l4_addr_t delta = l4_round_page(mend) - rpos;
              rpos += delta;
              rdest += delta;
            }

          if (ldest && lpos + mod->size_uncompressed > mstart)
            {
              l4_addr_t delta = l4_round_page(lpos + mod->size_uncompressed - mstart);
              if ((l4_addr_t)ldest > delta)
                {
                  lpos -= delta;
                  ldest -= delta;
                }
              else
                ldest = 0;
            }

          rpos += l4_round_page(mod->size_uncompressed);
          lpos += l4_round_page(mod->size_uncompressed);
        }

      Region dest = Region::array(ldest, total_size);
      destbuf = const_cast<char *>(ldest);
      fwd = true;
      if (!ldest || !mem_manager->ram->contains(dest) || regions->find(dest))
        {
          printf("  cannot decompress at %p, try %p\n", ldest, rdest);
          dest = Region::array(rdest, total_size); // try the move behind version
          destbuf = const_cast<char *>(rdest);
          fwd = false;

          if (!mem_manager->ram->contains(dest) || regions->find(dest))
            {
              destbuf = (char *)mem_manager->find_free_ram(total_size, (l4_addr_t)rdest);
              printf("  cannot decompress at %p, use %p\n", rdest, destbuf);
            }
        }
    }

  if (!destbuf)
    panic("fatal: cannot find memory to  decompress modules");

  printf("Uncompressing modules (modaddr = %p (%s)):\n", destbuf,
         fwd ? "forwards" : "backwards");
  if (!fwd)
    {
      // advance to last module end
      destbuf += total_size;

      for (unsigned i = mod_count - skip; i > 0; --i)
        {
          Mod_info *mod = _module_info_start + mod_sorter[i - 1];
          unsigned long dest_size = l4_round_page(mod->size_uncompressed);
          destbuf -= dest_size;
          decomp_move_mod(mod, destbuf);
        }
    }
  else
    {
      for (unsigned i = 0; i < mod_count - skip; ++i)
        {
          Mod_info *mod = _module_info_start + mod_sorter[i];
          unsigned long dest_size = l4_round_page(mod->size_uncompressed);
          decomp_move_mod(mod, destbuf);
          destbuf += dest_size;
        }
    }

  // move modules 0-2 out of the way
  for (unsigned i = 0; i < skip; ++i)
    {
      Mod_info *mod = _module_info_start + i;
      Region mr = ::mod_region(mod);
      for (Region *r = regions->begin(); r != regions->end(); ++r)
        {
          if (r->overlaps(mr) && r->name() != ::Mod_reg)
            {
              // overlaps with some non-module, assumingly an ELF region
              // so move us out of the way
              char *to = (char *)mem_manager->find_free_ram(mod->size);
              if (!to)
                {
                  printf("fatal: could not find free RAM region for module\n"
                         "       need %lx bytes\n", (unsigned long)mod->size);
                  mem_manager->ram->dump();
                  regions->dump();
                  panic("\n");
                }
              move_module(i, to);
              break;
            }
        }
    }
}
#endif // COMPRESS

/**
 * Create the basic multi-boot structure in IMAGE_MODE
 */
l4util_mb_info_t *
Boot_modules_image_mode::construct_mbi(unsigned long mod_addr)
{
  unsigned long mod_count = _num_modules();
  unsigned long mbi_size = sizeof(l4util_mb_info_t);
  mbi_size += sizeof(l4util_mb_mod_t) * mod_count;

  enum { Skip_num_mods = 3 }; // skip fiasco, sigma0, and roottask
  assert(mod_count >= Skip_num_mods);

  for (Mod_info const *mod = _module_info_start; mod != _module_info_end;
       ++mod)
    mbi_size += round_wordsize(strlen(mod_cmdline(mod)) + 1);

  l4util_mb_info_t *mbi = (l4util_mb_info_t *)mem_manager->find_free_ram(mbi_size);
  if (!mbi)
    panic("fatal: could not allocate MBI memory: %lu bytes\n", mbi_size);

  Region_list *regions = mem_manager->regions;
  regions->add(Region::start_size((l4_addr_t)mbi, mbi_size, ".mbi_rt",
                                  Region::Root));
  memset(mbi, 0, mbi_size);

  l4util_mb_mod_t *mods = reinterpret_cast<l4util_mb_mod_t *>(mbi + 1);
  char *mbi_strs = (char *)(mods + mod_count);

  mbi->mods_count  = mod_count;
  mbi->flags      |= L4UTIL_MB_MODS;
  mbi->mods_addr   = (l4_addr_t)mods;


  Mod_info *const mod_info = _module_info_start;
  unsigned long total_size = 0;
  for (Mod_info const *mod = mod_info + Skip_num_mods;
       mod < _module_info_end;
       ++mod)
    {
      total_size += l4_round_page(mod->size_uncompressed);
      if (mod_compressed(mod))
        check_md5(mod_name(mod), (u_int8_t*)mod_start(mod),
                  mod->size, mod_md5compr(mod));
    }

#ifdef COMPRESS
  if (mod_count > Skip_num_mods)
    decompress_mods(mod_count, Skip_num_mods, total_size, mod_addr);
  merge_mod_regions();
#else // COMPRESS
  move_modules(mod_addr);
#endif // ! COMPRESS

  for (unsigned i = 0; i < mod_count; ++i)
    {
      Mod_info const *mod = &_module_info_start[i];
      check_md5(mod_name(mod), (u_int8_t*)mod_start(mod),
                mod->size_uncompressed, mod_md5(mod));

      if (char const *c = mod_cmdline(mod))
        {
          unsigned l = strlen(c) + 1;
          mods[i].cmdline = (l4_addr_t)mbi_strs;
          memcpy(mbi_strs, c, l);
          mbi_strs += round_wordsize(l);
        }

      mods[i].mod_start = (l4_addr_t)mod_start(mod);
      mods[i].mod_end   = (l4_addr_t)mod_start(mod) + mod->size;
    }

  return mbi;
}


void
Boot_modules_image_mode::move_module(unsigned index, void *dest,
                                     bool overlap_check)
{
  Mod_info *mod = &_module_info_start[index];
  unsigned long size = mod->size;
  _move_module(index, dest, mod_start(mod), size, overlap_check);
  mod->start = (char *)dest;
}

#endif // IMAGE_MODE

