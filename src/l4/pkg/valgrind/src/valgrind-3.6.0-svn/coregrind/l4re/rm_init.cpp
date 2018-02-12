/*
 * This file is part of the Valgrind port to L4Re.
 *
 * (c) 2009-2010 Aaron Pohle <apohle@os.inf.tu-dresden.de>,
 *               Bjoern Doebel <doebel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universitaet Dresden (Germany)
 */
#include <l4/re/util/cap_alloc>
#include <l4/re/env>
#include <l4/re/util/region_mapping>
#include <l4/cxx/slab_alloc>
#include <l4/cxx/ipc_server>

__BEGIN_DECLS
#include "l4re/myelf.h"
#include "pub_core_basics.h"
#include "pub_l4re.h"
#include "pub_l4re_consts.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_libcfile.h"
#include "pub_core_libcprint.h"
__END_DECLS
#include "pub_core_vki.h"
__BEGIN_DECLS
#include "pub_core_tooliface.h"
#include "pub_core_libcassert.h" // VG_(exit)
#include "l4re_helper.h"
__END_DECLS

/***********************************************************************************
 *
 *                          L4Re kernel compatibility cruft
 *                          -------------------------------
 *
 * L4Re executes the L4Re kernel within every application. The kernel's is respon-
 * sible for loading the application (until startup of the main thread) and there-
 * after acts as the application's region manager and page fault handler. Both of
 * these tasks are in our case taken over by VCap. However, in order to do this,
 * Valgrind needs info about the initial memory layout upon startup of its address
 * space manager.
 *
 * Since there is no easy way to gain this information from the L4Re kernel (as it
 * is deliberately kept private, because we don't want anyone messing aroung with
 * the region map), we need to take the slightly fragile way of mirroring parts of
 * the L4Re kernel's class declarations here in order to be able to use them later
 * on.
 *
 * This means
 *   (1) Be careful, when to use this stuff. By now, only VG_(am_startup)() should
 *       need to call this.
 *       ==> There is a check in vrm_get_lists() to make sure this won't work
 *           after VCap took over.
 *   (2) Be careful with copied class declarations.
 *       ==> Clearly mark where these decls come from.
 *
 ***********************************************************************************/

// l4re_kernel/server/src/page_alloc.h, revision 37866
class Region_map;

class Single_page_alloc_base
{
public:
  static void init(L4::Cap<L4Re::Mem_alloc> alloc);
  static void init_local_rm(Region_map *lrm);
protected:
  Single_page_alloc_base() {}
  static void *_alloc();
  static void _free(void *) {}
};

template<typename A>
class Single_page_alloc : public Single_page_alloc_base
{
public:
  enum { can_free = true };
  A *alloc() { return reinterpret_cast<A*>(_alloc()); }
  void free(A *o) { _free(o); }
};


// l4re_kernel/server/src/slab_alloc.h, revision 37866
template< typename Type >
class Slab_alloc 
: public cxx::Slab_static<Type, L4_PAGESIZE, 2, Single_page_alloc>
{};


// l4re_kernel/server/src/region.h, revision 37866
class Region_ops;

typedef L4Re::Util::Region_handler<L4::Cap<L4Re::Dataspace>, Region_ops> Region_handler;

class Region_ops
{
public:
  typedef l4_umword_t Map_result;
  static int map(Region_handler const *h, l4_addr_t addr,
                 L4Re::Util::Region const &r, bool writable,
                 l4_umword_t *result);

  static void unmap(Region_handler const *h, l4_addr_t vaddr,
                    l4_addr_t offs, unsigned long size);

  static void free(Region_handler const *h, l4_addr_t start, unsigned long size);

  static void take(Region_handler const *h);
  static void release(Region_handler const *h);
};


class Region_map
: public L4Re::Util::Region_map<Region_handler, Slab_alloc>
{
private:
  typedef L4Re::Util::Region_map<Region_handler, Slab_alloc> Base;

public:
  Region_map();
  //void setup_wait(L4::Ipc::Istream &istr);
  int handle_pagefault(L4::Ipc::Iostream &ios);
  int handle_rm_request(L4::Ipc::Iostream &ios);
  virtual ~Region_map() {}

  void init();

  void debug_dump(unsigned long function) const;
private:
  int reply_err(L4::Ipc::Iostream &ios);
};


EXTERN_C void* mmap(void *, size_t, int, int, int, off_t) throw();
EXTERN_C int munmap(void *, size_t) throw();


static Region_map *vrm_determine_local_rm_address()
{
    /*
     * Determine the address of the RE kernel's local RM.
     */

    /*
     * 1. Try to find symbol in ELF image file
     */
    if (dbg_elf) VG_(printf)("opening rom/l4re\n");
    SysRes res = VG_(open)((const Char*)"rom/l4re", VKI_O_RDONLY, 0);
    if (dbg_elf) VG_(printf)("opened: %ld\n", sr_Res(res));
    if (sr_isError(res)) {
        VG_(printf)("Error opening file: %ld\n", sr_Err(res));
        VG_(exit)(1);
    }

    int fd = sr_Res(res);

    struct vg_stat statbuf;
    int err = VG_(fstat)(fd, &statbuf);
    if (dbg_elf) VG_(printf)("stat: %d, size %d\n", err, (int)statbuf.size);

    if (err) {
        VG_(printf)("error on fstat(): %d\n", err);
        VG_(exit)(1);
    }

    void *a = 0;
    a = mmap(a, statbuf.size, VKI_PROT_READ, VKI_MAP_PRIVATE, fd, 0);
    if (dbg_elf) VG_(printf)("mmap to %p\n", a);

    melf_global_elf_info i;
    err = melf_parse_elf(a, &i);
    if (dbg_elf) VG_(printf)("parsed: %d\n", err);

    char *rm_addr = melf_find_symbol_by_name(&i, (char*)"__local_rm");
    if (dbg_elf) VG_(printf)("Found symbol %s @ %p\n", (char*)"__local_rm", rm_addr);

    munmap(a, VG_PGROUNDUP(statbuf.size));
    VG_(close)(fd);

    if (rm_addr)
        return reinterpret_cast<Region_map*>(rm_addr);

    /*
     * 2. If not successful parsing binary, try hard-coded value
     */
    VG_(printf)("Did not find local-rm, aborting\n");
    VG_(exit)(1);

    /* Not found? */
}


EXTERN_C void vrm_get_lists(struct vrm_region_lists *rlist, unsigned num_regions,
                            unsigned num_areas)
{
    /*
     * If we happen to need this function while vcap is running (which should not be
     * the case), then implement it as a call to vcap in order to serialize segment
     * list access...
     */
    if (vcap_running) {
        VG_(debugLog)(0, "vcap", "Attention: VCAP thread is already running.");
        VG_(exit)(1);
    }

    /*
     * ... otherwise we'll now assume that we are running single-threaded and
     * vcap has not been started yet. (This is the case where am_startup() uses
     * this function to populate the segment list initially.)
     */

    Region_map *lrm = vrm_determine_local_rm_address();
    if (dbg_elf) VG_(printf)("LRM found @ %p\n", lrm);
    if (lrm) {

        /* LRM generic stats */
        rlist->min_addr     = lrm->min_addr();
        rlist->max_addr     = lrm->max_addr();
        rlist->region_count = 0;
        rlist->area_count   = 0;
        if (dbg_rm) VG_(debugLog)(2, "vcap", "rlist min: %08lx\n", rlist->min_addr);
        if (dbg_rm) VG_(debugLog)(2, "vcap", "rlist max: %08lx\n", rlist->max_addr);

        /* Read LRM areas */
        struct vrm_area *area = rlist->areas;
        for (Region_map::Const_iterator i = lrm->area_begin();
             i != lrm->area_end();
             ++i) {
            if (dbg_rm) VG_(debugLog)(2, "vcap", "AREA %08lx - %08lx\n", i->first.start(), i->first.end());
            area->start = i->first.start();
            area->end   = i->first.end();
            area->flags = i->second.flags();

            ++area;
            ++rlist->area_count;
        }

        /* Read LRM regions */
        struct vrm_region *reg = rlist->regions;
        for (Region_map::Const_iterator i = lrm->begin();
             i != lrm->end();
             ++i) {
            if (dbg_rm) VG_(debugLog)(2, "vcap", "REGION %08lx - %08lx\n", i->first.start(), i->first.end());
            reg->start        = i->first.start();
            reg->end          = i->first.end();
            reg->flags        = i->second.flags();
            reg->offset_in_ds = i->second.offset();
            reg->ds           = i->second.memory().cap();

            ++reg;
            ++rlist->region_count;
        }
    }
    else {
        VG_(printf)("implement probing fallback...\n");
        VG_(exit)(1);
    }
}
