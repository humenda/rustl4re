// vim: expandtab

/*
 * This file is part of the Valgrind port to L4Re.
 *
 * (c) 2009-2010 Aaron Pohle <apohle@os.inf.tu-dresden.de>,
 *               Bjoern Doebel <doebel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universitaet Dresden (Germany)
 */

#include <l4/sys/compiler.h>
// C++'s definition of NULL disagrees with Valgrind's
#undef NULL
#define NULL 0

#include <l4/re/env.h>
#include <l4/re/c/util/cap_alloc.h>
#include <l4/util/bitops.h>
#include <l4/re/c/rm.h>
#include <l4/re/util/object_registry>
#include <l4/re/util/br_manager>
#include <l4/sys/factory>
#include <l4/sys/ipc.h>
#include <l4/sys/utcb.h>
#include <l4/sys/vcon.h>
#include <l4/sys/scheduler.h>
#include <l4/sys/kdebug.h>
#include <l4/sys/debugger.h>
#include <l4/cxx/list>
#include <l4/sys/thread>
#include <l4/sys/scheduler>
#include <l4/re/util/cap_alloc>
#include <l4/re/env>
#include <l4/re/dataspace>
#include <l4/cxx/ipc_server>
#include <l4/re/util/vcon_svr>
#include <l4/re/util/region_mapping_svr>
#include <l4/re/util/region_mapping>
#include <l4/re/util/debug>


__BEGIN_DECLS
#include "pub_l4re.h"
#include "pub_l4re_consts.h"
#include "pub_core_basics.h"
#include "pub_core_ume.h"
#include "pub_core_aspacemgr.h"
#include "pub_core_debuginfo.h"  //DebugInfo
#include "pub_tool_libcbase.h"
#include "pub_tool_libcfile.h"
#include "pub_core_libcprint.h"
__END_DECLS
// HACK:
// We include all the pub* headers as C declarations to work around a huge
//   bunch of C++ warnings. However, pub_core_vki.h really contains a template
//   definition, so we need to include it as proper C++ header.
#include "pub_core_vki.h"
__BEGIN_DECLS
#include "pub_core_libcsetjmp.h"
#include "pub_core_threadstate.h"
#include "pub_core_scheduler.h"
#include "pub_core_tooliface.h"
#include "pub_tool_mallocfree.h"
#include "pub_core_libcassert.h" // VG_(show_sched_status)
#include "l4re/myelf.h"
__END_DECLS

extern const Bool dbg_pf   = DEBUG_OFF;
extern const Bool dbg_elf  = DEBUG_OFF;
extern const Bool dbg_rm   = DEBUG_OFF;
extern const Bool dbg_ds   = DEBUG_OFF;
extern const Bool dbg_vcap = DEBUG_OFF;

L4::Cap<L4::Thread> vcap_thread;
char vcap_stack[1 << 13]; // 8 kB

#include "l4re/allocator"
#include "l4re/dbg"
#include "l4re/vcon"
#include "l4re/loop_hooks"

namespace Vcap
{

namespace Rm
{

// see l4/pkg/loader/server/src/region.cc:Rm_server
class svr
{
    public:
        typedef L4::Cap<L4Re::Dataspace> Dataspace;

        enum { Have_find = true };

        static int validate_ds(L4::Ipc_svr::Server_iface *,
                                L4::Ipc::Snd_fpage const & ds_cap, unsigned flags,
                               L4::Cap<L4Re::Dataspace> *ds)
        {
            if (dbg_ds) VG_(debugLog)(4, "vcap", "flags 0x%x\n", flags);
            if (dbg_ds) VG_(debugLog)(4, "vcap", "ds @ %p\n", ds);
            if (dbg_ds) VG_(debugLog)(4, "vcap", "ds_cap received: %d\n", ds_cap.id_received());
            return L4_EOK;
        }

        static l4_umword_t find_res(L4::Cap<void> const &ds) { return ds.cap(); }
};

static l4_addr_t the_map_area;
static l4_addr_t the_map_area_end;
static L4::Cap<L4Re::Rm> real_rm;

/*
 * Modify the current environment's region manager and return the old one.
 * This is used when switching into non-VRM mode in __notify_tool_of_mapping.
 */
L4::Cap<L4Re::Rm> set_environment_rm(L4::Cap<void> cap)
{
    L4::Cap<L4Re::Rm> ret = L4Re::Env::env()->rm();

    L4::Cap<L4Re::Rm> rm(cap.cap());
    const_cast<L4Re::Env*>(L4Re::Env::env())->rm(rm);

    return ret;
}



// see: loader/server/src/region.cc
class Region_ops
{
    public:
        typedef L4::Ipc::Snd_fpage Map_result;
        typedef L4Re::Util::Region_handler<L4::Cap<L4Re::Dataspace>,
                Vcap::Rm::Region_ops> MyRegion_handler;

        static void init()
        {
            real_rm->reserve_area(&the_map_area, L4_PAGESIZE,
                                  L4Re::Rm::Reserved | L4Re::Rm::Search_addr);
            the_map_area_end = the_map_area + L4_PAGESIZE -1;
            unsigned prot =   VKI_PROT_READ
                            | VKI_PROT_WRITE
                            | VKI_PROT_EXEC;

            unsigned flags = VKI_MAP_ANONYMOUS;

            VG_(am_notify_valgrind_mmap)((Addr)the_map_area,
                                         VG_PGROUNDUP(the_map_area_end - the_map_area),
                                         prot, flags, -1, 0);

            VG_(debugLog)(1, "vcap", "Region ops map area: 0x%lx - 0x%lx\n",
                          the_map_area, the_map_area_end);
        }

        static int map(MyRegion_handler const *h,
                       l4_addr_t local_addr, L4Re::Util::Region const &r, bool writable,
                       L4::Ipc::Snd_fpage *result)
        {
            int err;
            if (dbg_ds) VG_(debugLog)(4, "vcap", "%s handler %p, local_addr 0x%lx, writable %s\n",
                          __func__, (void * )h, local_addr, writable ? "true" : "false");

            if ((h->flags() & L4Re::Rm::Reserved) || !h->memory().is_valid()) {
                VG_(debugLog)(4, "vcap", "Reserved || invalid\n");
                return -L4_ENOENT;
            }

            if (h->flags() & L4Re::Rm::Pager) {
                VG_(debugLog)(4, "vcap", "pager\n");
                return -L4_ENOENT;
            }

            if (h->is_ro() && writable)
                Dbg(Dbg::Warn).printf("WARNING: Writable mapping request on read-only region at 0x%lx!\n",
                                      local_addr);

            l4_addr_t offset = local_addr - r.start() + h->offset();
            L4::Cap<L4Re::Dataspace> ds = L4::cap_cast<L4Re::Dataspace>(h->memory());

            if (dbg_ds) VG_(debugLog)(4, "vcap", "Dataspace size 0x%lx\n", ds->size());
            if (dbg_ds) VG_(debugLog)(4, "vcap", "Region: 0x%lx - 0x%lx\n", r.start(), r.end());
            if (dbg_ds) VG_(debugLog)(4, "vcap", "map(0x%lx, 0x%x, 0x%lx, 0x%lx, 0x%lx)\n", offset, writable,
                          the_map_area, the_map_area, the_map_area_end);
            if (err = ds->map(offset, writable, the_map_area, the_map_area, the_map_area_end)) {
                VG_(debugLog)(0, "vcap", "map failed: %d %s\n", err, l4sys_errtostr(err));
                l4re_rm_show_lists();
                return err;
            }

            if (dbg_ds) VG_(debugLog)(4, "vcap", "ds->map(): %d\n", err);

            *result = L4::Ipc::Snd_fpage::mem(the_map_area, L4_PAGESHIFT, L4_FPAGE_RWX,
                                         local_addr, L4::Ipc::Snd_fpage::Grant);
            return L4_EOK;
        }

        static void unmap(MyRegion_handler const *h, l4_addr_t vaddr,
                        l4_addr_t offs, unsigned long size)
        {
            (void)h; (void)vaddr; (void)offs; (void)size;
            VG_(debugLog)(0, "vcap", "\n");
        }

        static void take(MyRegion_handler const *h)
        {
            (void)h;
            VG_(debugLog)(0, "vcap", "\n");
        }

        static void release(MyRegion_handler const *h)
        {
            (void)h;
            VG_(debugLog)(0, "vcap", "\n");
        }
};


/* Explanation: region_mapping_svr relies on the region map consisting of Nodes.
 *              In fact it relies on an implementation using the cxx avl tree
 *              and the underlying node types. However, we handle regions using
 *              Valgrind's data types here, but need an adaptor node type.
 */
class node
{
    public:
        node() { }

        node(L4Re::Util::Region const &f,
             L4Re::Util::Region_handler<L4::Cap<L4Re::Dataspace>, Vcap::Rm::Region_ops> rh)
            : first(f), second(rh)
        { }

        L4Re::Util::Region first;
        L4Re::Util::Region_handler<L4::Cap<L4Re::Dataspace>,
                                        Vcap::Rm::Region_ops> second;
};


/*
 * For reserved areas, we need to store information about their original size, because
 * during runtime regions may be attached into areas and these reserved regions may
 * be split into pieces that need to be rejoined if the regions get detached.
 */
struct area_info
{
    public:
        Addr _start;
        Addr _end;

        area_info(Addr start, Addr end)
            :_start(start), _end(end)
        { }


        bool match_resvn(NSegment const * const seg) const
        {
            return ((seg->kind == SkResvn) &&
                    (seg->start == _start) &&
                    (seg->end   == _end));
        }


        bool contains_nsegment(NSegment const * const seg) const
        {
            return ((seg->start >= _start) &&
                    (seg->end   <= _end));
        }
};


#define DEBUG_RM_LAYOUT do { VG_(am_show_nsegments)(0, (HChar*)"current AS layout"); } while (0)

#define RM_ERROR do { DEBUG_RM_LAYOUT; VG_(exit)(1); } while (0)

/*
 * VCap::rm -- virtual region manager.
 *
 * This class implements a region manager on top of Valgrind's segment array.
 * We map L4RM functionality as follows:
 *
 *  1) attach/detach -> create/delete new segments
 *
 * Area creation is harder. First, we create a reservation in the segment
 * array. However, we need to keep track of this area for later attach()es and
 * therefore keep an additional list of all known regions. This is because the
 * reservations in the segment array don't suffice -- the area may be
 * completely mapped with regions and therefore unobtainable from the segment
 * array reservation.
 *
 *  2) attach_area   -> create new reservation and add to list
 *  3) detach_area   -> remove all reservations in area and list entry
 */
class rm
{
    private:
        cxx::List<struct area_info *, Valgrind_allocator> _areas;
        unsigned _id;

    public:
        typedef Vcap::Rm::node          * Node;
        typedef L4Re::Util::Region_handler<L4::Cap<L4Re::Dataspace>,
                                           Vcap::Rm::Region_ops>  Region_handler;
        typedef L4Re::Util::Region        Region;

        enum Attach_flags
        {
            None      = 0,
            Search    = L4Re::Rm::Search_addr,
            In_area   = L4Re::Rm::In_area,
        };

        rm(unsigned id) : _id(id) { }

        int dispatch(L4::Ipc_svr::Server_iface *svr_iface, l4_msgtag_t tag,
                     l4_umword_t obj, L4::Ipc::Iostream &ios)
            throw()
        {
            int err;

            switch(tag.label()) {
                case L4_PROTO_PAGE_FAULT:
                    return L4Re::Util::region_pf_handler<Vcap::Dbg>(this, ios);

                case L4Re::Rm::Protocol:
                    err = L4Re::Util::region_map_server<Vcap::Rm::svr>(svr_iface, this, ios);
                    if (dbg_rm) VG_(debugLog)(4, "vcap", "rm_server() returns %d\n", err);
                    return err;

                default:
                    return -L4_EBADPROTO;
            }
        }


    private:

        /*
         * Find area info for addr.
         */
        struct area_info *__find_area_info(void *addr)
        {
            typedef cxx::List<struct area_info *, Valgrind_allocator> LIST;
            for (LIST::Iter i = _areas.items(); *i; ++i) {
                struct area_info *inf = *i;
                if (inf->_start <= (Addr)addr && inf->_end >= (Addr)addr)
                    return inf;
            }
            return NULL;
        }


        /*
         * Remove area info for addr.
         */
        bool __remove_area_info(void *addr) {
            typedef cxx::List<struct area_info *, Valgrind_allocator> LIST;
            LIST::Iter i = _areas.items();
            for (; *i; ++i) {
                struct area_info *inf = *i;
                if (inf->_start <= (Addr)addr && inf->_end >= (Addr)addr)
                    break;
            }

            if (*i) {
                _areas.remove(i);
                return true;
            }

            return false;
        }


        /*
         * Do what is necessary to attach a region into an already existing area.
         *
         * \return L4_INVALID_PTR   on error
         *         start address    on success
         */
        void *__handle_attach_in_area(void *addr, unsigned long size, unsigned flags)
        {
            if (dbg_rm) VG_(debugLog)(4, "vcap", "%s: %p %lx %x\n", __func__, addr, size, flags);

            /* We need to find the reservation for this area.
             *
             * Case A: The area is still empty, then we'll find the resvn by simply
             *         looking it up.
             *
             * Case B: There are already areas attached to this resvn. In this case
             *         a simple lookup may give us an area. Hence, we need to search
             *         forwards and backwards in the segment array to find the resvn.
             */
            NSegment *seg = const_cast<NSegment*>(VG_(am_find_nsegment)((Addr)addr));

            if (!seg) {
                VG_(printf)("No reservation found for attach in area. Addr %p, size %ld\n",
                            addr, size);
                RM_ERROR;
            }

            if (dbg_rm) VG_(debugLog)(4, "vcap", "Segment @ %p (%08lx - %08lx, type: %s)\n",
                        seg, seg->start, seg->end, vcap_segknd_str(seg->kind));

            struct area_info *info = __find_area_info(addr);
            if (!info) {
                VG_(printf)("ERROR: could not find area for address %p\n", addr);
                RM_ERROR;
            }
            if (dbg_rm) VG_(debugLog)(4, "vcap", "Area @ %p (%08lx - %08lx)\n",
                          info, info->_start, info->_end);

            /*
             * Case B
             */
            if (!info->match_resvn(seg) && seg->kind != SkResvn) {
                VG_(debugLog)(0, "vcap", "resvn not matching");
                VG_(exit)(1);
            }

            /*
             * Now we found a reservation suiting the new region. We now go and unmap
             * the reservation, create a new region and if necessary mark chunks before
             * and after the region as reservations.
             */
            Addr old_start = seg->start;
            Addr old_end   = seg->end;

            void *ret      = addr;

            /* unmap all */
            Bool needDiscard = VG_(am_notify_munmap)(seg->start, seg->end - seg->start + 1);
            if (dbg_rm) VG_(debugLog)(4, "vcap", "unmapped, need discard? %d\n", needDiscard);

            /* new reservation before this area? */
            if (old_start < (Addr)addr) {
                if (dbg_rm) VG_(debugLog)(4, "vcap", "in area: split before");
                Addr new_start    = old_start;
                unsigned new_size = (Addr)addr - old_start + 1;
                if (dbg_rm) VG_(debugLog)(4, "vcap", "new resvn @ %p, size %lx\n", new_start, new_size);
                attach_area(new_start, new_size);
            }

            /* new reservation after this area? */
            if (old_end > (Addr)addr + size - 1) {
                if (dbg_rm) VG_(debugLog)(4, "vcap", "in area: split after");
                Addr new_start    = (Addr)addr + size;
                unsigned new_size = old_end - new_start + 1;
                if (dbg_rm) VG_(debugLog)(4, "vcap", "new resvn @ %p, size %lx\n", new_start, new_size);
                attach_area(new_start, new_size);
            }

            return ret;
        }


        /*
         * Find a free segment with start address >= start and a given size.
         * We use Valgrind's dynamic memory manager to determine such an
         * address.
         *
         * \return L4_INVALID_PTR      on error
         *         valid segment start on success
         */
        void *__find_free_segment(void *start, unsigned size)
        {
            Bool ok;
            void *ret;

            if (_id == VRMcap_client)
                ret = (void*)VG_(am_get_advisory_client_simple)((Addr)start, size, &ok);
            else if (_id == VRMcap_valgrind)
                ret = (void*)VG_(am_get_advisory_valgrind_simple)((Addr)start, size, &ok);
            else {
                // should not happen!
                VG_(printf)("Cannot determine the type of mapping here: %lx\n", _id);
                return L4_INVALID_PTR;
            }

            if (!ok) {
                VG_(debugLog)(0, "vcap", "Advisor has no free area for us!\n");
                RM_ERROR;
                return L4_INVALID_PTR;
            }

            return ret;
        }


        /*
         * Valgrind notifies tools about successful mmap() using the VG_TRACK
         * functionality. The only place where we can keep track of all
         * mappings is inside VRM, which means in the tool's page fault
         * handler. Unfortunately, tools tend to do arbitrary things (such as
         * allocating memory) in response to such notifications and we cannot
         * simply execute Valgrind code here, because it may try calling the
         * pager (ourselves!).
         *
         * Therefore, for sending such notifications, we switch back to the
         * mode, where we pretend that VRM is not there yet. This means, memory
         * allocations are redirected to the original RM and segments are
         * updated internally.
         *
         * A word on thread-safety: We don't need additional locking here,
         * because we are sure that the tool code is currently waiting for us
         * to handle its page fault or RM request. So this is as thread-safe as
         * the tool was before.
         */
        void __notify_tool_of_mapping(Addr addr, size_t size, unsigned prot)
        {
            L4::Cap<L4Re::Rm> __rm__ = Vcap::Rm::set_environment_rm(Vcap::Rm::real_rm);

            vcap_running = 0;
            VG_TRACK(new_mem_mmap, (Addr)addr, size, prot & VKI_PROT_READ,
                     prot & VKI_PROT_WRITE, prot & VKI_PROT_EXEC, 0);
            vcap_running = 1;
            Vcap::Rm::set_environment_rm(__rm__);
        }

    public:
        void *attach(void *addr, unsigned long size, Region_handler const &hdlr,
                     unsigned flags = None,
                     unsigned char align = L4_PAGESHIFT) throw()
        {
            if (dbg_rm) VG_(debugLog)(3, "vcap", "%s: addr %p, size 0x%lx, flags 0x%x=%s%s\n",
                          __func__, addr, size, flags,
                          flags & L4Re::Rm::In_area ? "|In_area" : "",
                          flags & L4Re::Rm::Search_addr ? "|Search_addr" : ""
                          );

            /*
             * Special case 1: In_area flag is set -> we need to lookup the area and then
             *                 find a free reservation inside it.
             */
            if (flags & L4Re::Rm::In_area) {
                addr = __handle_attach_in_area(addr, size, flags);
                if (dbg_rm) VG_(debugLog)(4, "vcap", "__handle_attach_in_area: %p\n", addr);
                if (addr == L4_INVALID_PTR)
                    return addr;
            }

            /*
             * Special case 2: Search_addr flag is set -> we need to find a free, unreserved
             *                 segment.
             */
            if (flags & L4Re::Rm::Search_addr) {
                addr = __find_free_segment(0, size);
                if (dbg_rm) VG_(debugLog)(4, "vcap", "__find_free_segment: %p\n", addr);
                if (!addr)
                    return L4_INVALID_PTR;
            }

            /*
             * Next we need to establish the necessary information. Depending on which VRM
             * instance this is, we either mark the mapping as Valgrind or Client. Access
             * rights are determined from the RM handler info.
             */

            unsigned prot = VKI_PROT_READ;
            if (!hdlr.is_ro())
                prot |= VKI_PROT_WRITE;

            Off64T offs = hdlr.offset();

            /*
             * By default, we register all memory mappings as anonymous, because at this
             * point in the MM chain all we know is data spaces and these are equivalent
             * to anonymous memory. Valgrind however needs more detailed information for
             * file mappings. This is handled outside, e.g., in the _dl_mmap trap handler.
             */
            unsigned vgflags = VKI_MAP_ANONYMOUS;

            // Notify aspacemgr of Valgrind ...
            if (_id == VRMcap_valgrind) {
                if (dbg_rm) VG_(debugLog)(4, "vcap", "am_notify_valgrind_mmap(a=%08lx, size %lx)\n",
                                          addr, size);
                VG_(am_notify_valgrind_mmap)((Addr)addr, size, prot, vgflags, -1, 0);
            }
            else if (_id == VRMcap_client) { // ... or client mapping
                    VG_(am_notify_client_mmap)((Addr)addr, size, prot, vgflags, -1, 0);
                    __notify_tool_of_mapping((Addr)addr, size, prot);
            }
            else {
                // should not happen
                VG_(printf)("I can't determine what kind of mapping this is -- id = %lX\n", _id);
                return L4_INVALID_PTR;
            }

            /*
             * Aspacemgr now added the reservation/mapping to the segment array. Now we need to
             * add a RM handler to this segment.
             */
            vrm_segment_notify((Addr)addr, (Addr)addr + size - 1, hdlr.client_cap_idx(), offs, flags);

            if (dbg_rm) VG_(debugLog)(4, "vcap", "returning %p\n", addr);
            return addr;
        }


        /*
         * Reserve a region. We do this using Valgrind's RSVN segment type. When attaching areas to
         * these reservations, we adapt accordingly. This may lead to a point, where no RSVN segment
         * is there anymore. Hence, we cannot rely only on the segment array to store information on
         * segments, but need to additionally keep track of regions in an area_info list.
         */
        l4_addr_t attach_area(l4_addr_t addr, unsigned long size,
                              unsigned flags = None,
                              unsigned char align = L4_PAGESHIFT) throw()
        {
            if (dbg_rm) VG_(debugLog)(3, "vcap", "%s: addr 0x%lx, size 0x%lx, flags 0x%x, align 0x%x\n",
                    __func__, addr, size, flags, align);

            if (flags & L4Re::Rm::Search_addr) {
                addr = (l4_addr_t)__find_free_segment((void*)addr, size);
                if (dbg_rm) VG_(debugLog)(4, "vcap", "__find_free_segment: %08lx\n", addr);
                if (!addr)
                    return (l4_addr_t)L4_INVALID_PTR;
            }

            // Now we mark this area as reserved.
            Bool ok = VG_(am_create_reservation)(addr, VG_PGROUNDUP(size), SmFixed, 0);
            if (!ok) {
                /* 
                 * Failing here is ok. This just means, there is already
                 * another reservation existing. L4Re apps must handle this
                 * themselves.
                 */
                if (dbg_rm) VG_(debugLog)(4, "vcap", "Error creating reservation for area 0x%lx, size 0x%lx\n",
                             addr, size);
                return L4_INVALID_ADDR;
            }

            NSegment *seg = const_cast<NSegment*>(VG_(am_find_nsegment)((Addr)addr));
            area_info *info = new area_info(addr, addr + VG_PGROUNDUP(size) - 1);
            VG_(am_set_nodeptr)(seg, (Addr)info);
            if (dbg_rm) VG_(debugLog)(3, "vcap", "AREA segment %08lx node %p\n", (Addr)seg, info);

            _areas.push_back(info);

            return addr;
        }


        bool detach_area(l4_addr_t addr) throw()
        {
            if (dbg_rm) VG_(debugLog)(3, "vcap", "%s: addr 0x%lx\n", __func__, addr);

            struct area_info *info = __find_area_info((void*)addr);
            if (!info) {
                VG_(printf)("Cannot find area for address %p\n", (void*)addr);
                RM_ERROR;
                return false;
            }

            VG_(debugLog)(4, "vcap", "detach area: %08lx - %08lx\n", info->_start, info->_end);
            NSegment *seg = const_cast<NSegment*>(VG_(am_find_nsegment)(info->_start));
            /*
             * Run through all segments within the area and unmap them if they are
             * only reservations for this area.
             */
            while (seg->start < info->_end) {
                if (seg->kind == SkResvn && seg->dsNodePtr != NULL) {
                    if (dbg_rm) VG_(debugLog)(4, "vcap", "unmapping resvn %08lx - %08lx\n",
                                  seg->start, seg->end);
                    NSegment *next_seg = seg + 1;
                    Bool needDiscard = VG_(am_notify_munmap)(seg->start,
                                                             seg->end - seg->start + 1);
                    /*
                     * we need to re-lookup the segment, because by removing
                     * the mapping above, we invalidated our seg pointer
                     */
                    seg = next_seg;
                }
                else
                    seg++;
            }

            if (dbg_rm) VG_(debugLog)(4, "vcap", "removed all reservations. Deleting area_info\n");
            __remove_area_info((void*)addr);

            return true;
        }

        int detach(void *addr, unsigned long sz, unsigned flags,
                   Region *reg /* OUT */,
                   Region_handler *hdlr /* OUT */) throw()
        {
            // XXX: overlapping detach?

            if (dbg_rm) VG_(debugLog)(3, "vcap", "%s: addr %p, size 0x%lx, flags 0x%x, region %p, hdlr %p\n",
                          __func__, addr, sz, flags, reg, hdlr);

            NSegment const * seg = VG_(am_find_nsegment)((Addr)addr);
            if (!seg ||(seg && seg->kind==SkResvn))
                return -L4_ENOENT;

            unsigned size = l4_round_page(seg->end - seg->start);
            Node n = reinterpret_cast<Node>(seg->dsNodePtr);
            if (n) {
                *reg = n->first;
                *hdlr = n->second;
                delete n;
            }

            Bool needDiscard = VG_(am_notify_munmap)(seg->start, size);
            if (dbg_rm) VG_(debugLog)(4, "vcap", "unmapped, need discard? %d\n", needDiscard);

            return L4Re::Rm::Detached_ds;
        }


        Node find(Region const &reg) const throw()
        {
            if (dbg_pf) VG_(debugLog)(4, "vcap", "Find start 0x%lx, end 0x%lx\n", reg.start(),
                          reg.end());

            Node n;
            NSegment const * vg_seg = VG_(am_find_nsegment)(reg.start());
            if (!vg_seg) {
                VG_(debugLog)(1, "vcap", "Error looking up segment for 0x%lx\n",
                              reg.start());
                return Node();
            }
            if (dbg_pf) VG_(debugLog)(4, "vcap", "Found segment: %p (0x%lx - 0x%lx) nptr %08lx, type %s\n",
                          vg_seg, vg_seg->start, vg_seg->end, vg_seg->dsNodePtr, vcap_segknd_str(vg_seg->kind));

            /*
             * If there's no node ptr, something went wrong
             */
            if (!vg_seg->dsNodePtr) {
                VG_(debugLog)(0, "vcap", "ERROR: no node ptr found for region %lx - %lx\n",
                              reg.start(), reg.end());
                RM_ERROR;
            }
            else
                n = (Node)vg_seg->dsNodePtr;

            if (dbg_pf) VG_(debugLog)(4, "vcap", "Node ptr: %p\n", n);

            return n;
        }


        Node area_find(Region const &) const throw()
        {
            VG_(debugLog)(0, "vcap", "UNIMPLEMENTED: area_find");
            return NULL;
        }

        Node lower_bound(Region const &) const throw()
        {
            VG_(debugLog)(0, "vcap", "UNIMPLEMENTED: lower_bound");
            return NULL;
        }

        Node lower_bound_area(Region const &) const throw()
        {
            VG_(debugLog)(0, "vcap", "UNIMPLEMENTED: lower_bound_area");
            return NULL;
        }

        void get_lists( l4_addr_t) const throw()
        {
            VG_(debugLog)(0, "vcap", "UNIMPLEMENTED: get_lists");
        }

        l4_addr_t max_addr() const { return ~0UL; }


};

}; // Namespace Rm
}; // Namespace Vcap


struct Vcap_if : L4::Kobject_2t<Vcap_if, L4Re::Rm, L4::Kobject_2t<void, L4::Vcon, L4Re::Parent > >
{};

class Vcap_object : public L4::Server_object_t<Vcap_if>
{
    private:
        Vcap::vcon_srv _log;
        Vcap::Rm::rm   _rm;

        unsigned _id;

    public:
        Vcap_object(unsigned id) : _rm(id), _id(id)
        {}


        int handle_exception()
        {
            VG_(printf)("\033[31mEXCEPTION\033[0m\n");
            return -L4_EOK;
        }

        int dispatch(l4_umword_t obj, L4::Ipc::Iostream &ios) L4_NOTHROW
        {
            if (dbg_vcap) VG_(debugLog)(4, "vcap", "dispatch\n");

            l4_msgtag_t t;
            ios >> t;

            if (dbg_vcap) {
                VG_(debugLog)(4, "vcap", "label: %lx (%ld)\n", t.label(), t.label());
            }

            switch(t.label()) {
                case L4_PROTO_LOG:
                    if (dbg_vcap) {
                        VG_(debugLog)(2, "vcap", "log protocol\n");
                    }
                    return _log.dispatch(obj, ios);

                case L4_PROTO_PAGE_FAULT:
                    //if (dbg_vcap) VG_(debugLog)(2, "vcap", "page fault\n");
                    return _rm.dispatch(server_iface(), t, obj, ios);

                case L4Re::Rm::Protocol:
                    return _rm.dispatch(server_iface(), t, obj, ios);

                case L4Re::Parent::Protocol:
                    if (dbg_vcap) VG_(debugLog)(2, "vcap", "parent protocol\n");
                    return -L4_ENOSYS;

                case L4_PROTO_IRQ:
                    // FIXME: why is the RM server called for IRQs protocol
                    if (dbg_vcap) VG_(debugLog)(2, "vcap", "irq protocol\n");
                    return _rm.dispatch(server_iface(), t, obj, ios);

                case L4_PROTO_EXCEPTION:
                    return handle_exception();

                default:
                    VG_(debugLog)(2, "vcap", "Unknown protocol: %lx (%ld)\n",
                                  t.label(), t.label());
                    VG_(show_sched_status)();
                    return -L4_ENOSYS;
            }
        }
};


/*
 * Flag determining whether VRM is running atm. If this is true, we use VRM's normal
 * memory management functions, otherwise (before VRM is started) we fall back to local
 * implementations.
 */
int vcap_running = 0;

/*
 * We use 2 server objects for handling RM requests -- one for the client and one for
 * Valgrind mappings.
 */
static Vcap_object valgrind_obj(VRMcap_valgrind);
static Vcap_object client_obj(VRMcap_client);

/*
 * Main VRM function
 */
static void vcap_thread_fn(void *) L4_NOTHROW
{
    VG_(debugLog)(1, "vcap", "%s: Here, vcap_running @ %p (%d)\n", __func__,
                  &vcap_running, vcap_running);

    Vcap::Rm::Region_ops::init();

    L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks>
      server(l4_utcb(), vcap_thread, L4Re::Env::env()->factory());

    server.registry()->register_obj(&valgrind_obj);
    server.registry()->register_obj(&client_obj);

    vcap_running = 1;

    VG_(debugLog)(0, "vcap", "server.loop()\n");
    server.loop();
}


/*
 * Store info about the real Region Manager before switching everything
 * into VRM mode.
 */
void setup_real_rm()
{
    Vcap::Rm::real_rm = L4Re::Env::env()->rm();
    VG_(debugLog)(2, "vcap", "real rm %lx\n", Vcap::Rm::real_rm.cap());
}


/*
 * Allocate capability and use it to create a thread.
 */
L4::Cap<L4::Thread> allocate_new_thread()
{
    L4::Cap<L4::Thread> ret = L4Re::Util::cap_alloc.alloc<L4::Thread>();
    if (!ret.is_valid()) {
        VG_(debugLog)(0, "vcap", "%s: Error allocating thread cap.",
                      __func__);
        VG_(exit)(1);
    }

    if (dbg_vcap) VG_(debugLog)(1, "vcap", "vcap cap: %lx\n", ret.cap());

    l4_msgtag_t tag = L4Re::Env::env()->factory()->create(ret);
    if (l4_msgtag_has_error(tag)) {
        VG_(debugLog)(0, "vcap", "Error creating vcap thread from factory.\n");
        VG_(exit)(1);
    }

    return ret;
}


/*
 * Basic VRM thread setup
 */
void init_vcap_thread(L4::Cap<L4::Thread>& threadcap)
{
    l4_utcb_t *new_utcb = reinterpret_cast<l4_utcb_t*>(
                            L4Re::Env::env()->first_free_utcb());

    L4::Thread::Attr a;
    a.pager(L4Re::Env::env()->rm());
    a.exc_handler(L4Re::Env::env()->rm());
    a.bind(new_utcb, L4Re::This_task);
    // XXX: do we need to increment first_free_utcb here?

    l4_msgtag_t tag = threadcap->control(a);
    if (l4_msgtag_has_error(tag)) {
        VG_(debugLog)(0, "vcap-thread", "Error committing vcap thread.\n");
    }

    l4_sched_param_t sp;
    // scheduling defaults, copied from loader/server/src/loader.cc
    sp.prio     = 2;
    sp.quantum  = 0;
    sp.affinity = l4_sched_cpu_set(0, ~0, 1);

    tag = L4Re::Env::env()->scheduler()->run_thread(threadcap, sp);
    if (l4_msgtag_has_error(tag)) {
        VG_(debugLog)(0, "vcap-thread", "Error setting scheduling attributes.\n");
    }
}


void run_vcap_thread(L4::Cap<L4::Thread>& threadcap)
{
    // XXX: is memset to 0 necessary or does this modify our test results?
    VG_(memset)(vcap_stack, 0, sizeof(vcap_stack));

    if (dbg_vcap) {
        VG_(debugLog)(2, "vcap", "new stack: %p - %p\n", vcap_stack,
                      vcap_stack + sizeof(vcap_stack));
        VG_(debugLog)(2, "vcap", "ex_regs(0x%lx, 0x%lx, 0x%lx, 0)\n",
                      threadcap.cap(), (l4_umword_t)vcap_thread_fn,
                      (l4_umword_t)vcap_stack + sizeof(vcap_stack));
    }

    l4_msgtag_t tag = threadcap->ex_regs((l4_umword_t)vcap_thread_fn,
                              (l4_umword_t)vcap_stack + sizeof(vcap_stack),
                              0);
    if (l4_msgtag_has_error(tag)) {
            VG_(debugLog)(0, "vcap-thread", "Error enabling vcap thread.\n");
    }

    while (!vcap_running)
        l4_sleep(100);
}


void main_thread_modify_rm(L4::Cap<void> cap)
{
    /*
     * Now that the VCap thread is started, we also want it to become _our_
     * pager, so that it can take care of all region mapping duties.
     */
    L4::Thread::Attr    attr;
    // we assume, we're always executed by the main thread!
    L4::Cap<L4::Thread> self = L4Re::Env::env()->main_thread();

    attr.pager(cap);
    attr.exc_handler(cap);
    self->control(attr);

    /*
     * And now vcap becomes everyone's region manager
     */
    Vcap::Rm::set_environment_rm(cap);
}


void vrm_track_utcb_area()
{
    l4_fpage_t fp = L4Re::Env::env()->utcb_area();
    Addr start = l4_fpage_memaddr(fp);
    Addr end = start + (l4_fpage_size(fp) << L4_PAGESHIFT);
    VG_(debugLog)(4, "vcap", "TRACK(%lx, %lx, 1, 1, 0, 0)\n", start, end-start);
    VG_TRACK(new_mem_startup, start, end-start, True, True, False, 0);
}


EXTERN_C void l4re_vcap_start_thread(void)
{
    VG_(debugLog)(2, "vcap", "%s: Here\n", __func__);

    setup_real_rm();

    vcap_thread = allocate_new_thread();
    init_vcap_thread(vcap_thread);
    run_vcap_thread(vcap_thread);

    l4_debugger_set_object_name(vcap_thread.cap(), "VG::vcap");
    l4_debugger_set_object_name(L4Re::Env::env()->main_thread().cap(),
                                "VG::main");
    l4_debugger_set_object_name(valgrind_obj.obj_cap().cap(), "VRMcap::valgrind");
    l4_debugger_set_object_name(client_obj.obj_cap().cap(), "VRMcap::client");

    main_thread_modify_rm(valgrind_obj.obj_cap());
}

/* The size of an L4Re::Env object is needed in setup_client_stack() which is
   part of a C source code file and thus cannot determine the size of a C++ class
   itself.
 */
EXTERN_C size_t l4re_env_env_size()
{
    return sizeof(L4Re::Env);
}


/*
 * Currently, the client sees all init caps Valgrind sees. In future versions we might
 * want to modify this as well. Therefore we create a copy of all init caps here and 
 * pass the client this copy for usage. This is the place to filter out or modify these
 * caps before starting the client.
 */
static L4Re::Env::Cap_entry* __copy_init_caps(L4Re::Env const * const e)
{
    if (dbg_vcap) VG_(debugLog)(4, "vcap", "counting caps\n");
    L4Re::Env::Cap_entry const *c = e->initial_caps();

    unsigned cnt = 0;
    for ( ; c->flags != ~0UL; ++c, ++cnt)
        ;

    if (dbg_vcap) VG_(debugLog)(4, "vcap", "count: %x\n", cnt+1);

    SysRes res = VG_(am_mmap_anon_float_client)((cnt+1) * sizeof(L4Re::Env::Cap_entry),
                                                VKI_PROT_READ | VKI_PROT_WRITE);
    if (sr_isError(res)) {
        VG_(debugLog)(0, "vcap", "Error allocating memory for client initial caps.\n");
        VG_(exit)(1);
    }
    VG_(memcpy)((void*)sr_Res(res), e->initial_caps(), (cnt+1) * sizeof(L4Re::Env::Cap_entry));

    return reinterpret_cast<L4Re::Env::Cap_entry*>(sr_Res(res));
}


/*
 * Modify client environment and make VRM be the handler for all
 * interesting events.
 */
EXTERN_C void *l4re_vcap_modify_env(struct ume_auxv *envp, Addr client_l4re_env_addr)
{
    L4Re::Env *e = new ((void*)client_l4re_env_addr) L4Re::Env(*L4Re::Env::env());
    VG_(debugLog)(0, "vcap", "  New env @ %p\n", e);
    VG_(debugLog)(0, "vcap", "  Orig env @ %p\n", L4Re::Env::env());

    e->parent(L4::cap_cast<L4Re::Parent>(client_obj.obj_cap()));
    e->rm(L4::cap_cast<L4Re::Rm>(client_obj.obj_cap()));
    e->log(L4::cap_cast<L4Re::Log>(client_obj.obj_cap()));
    e->first_free_cap(MAX_LOCAL_RM_CAPS + MAX_VG_CAPS);
    e->first_free_utcb(L4Re::Env::env()->first_free_utcb() + VG_UTCB_OFFSET);
    VG_(debugLog)(0, "vcap", "  new RM @ %lx\n", e->rm().cap());

    e->initial_caps(__copy_init_caps(e));

    client_env = (void *) e;
    client_env_size = l4re_env_env_size();

    return e;
}


/*
 * When parsing the early region list from RM, we need to add Node descriptors
 * for the segments found.  Therefore this callback exists for the aspacemgr
 * code to notify us about a found segment.
 */
void vrm_segment_notify(Addr start, Addr end, l4_cap_idx_t dscap, unsigned offset, unsigned flags)
{
    NSegment *seg = const_cast<NSegment*>(VG_(am_find_nsegment)((Addr)start));
    if (seg->start == start && seg->end == end) {
        vrm_update_segptr(seg, dscap, offset, flags);
    }
    else {
        VG_(printf)("Segment mismatch: args %08lx-%08lx; found %08lx-%08lx\n",
                    start, end, seg->start, seg->end);
    }
}


/*
 * This function actually exists for exactly one region: When starting the
 * dynamic memory manager, Valgrind allocates a malloc pool. This is still done
 * using the "old" RM in place, therefore at this point we need to go to RM and
 * request all information necessary to register a node pointer for this
 * region.
 */
void vrm_segment_fixup(NSegment *seg)
{
    if (seg->dsNodePtr)
        return;

    if (seg->kind == SkResvn)
        return;

    L4::Cap<L4Re::Rm> _rm;
    if (Vcap::Rm::real_rm.is_valid())
        _rm = Vcap::Rm::real_rm;
    else
        _rm = L4Re::Env::env()->rm();

    l4_addr_t addr     = seg->start;
    unsigned long size = seg->end - seg->start + 1;
    l4_addr_t offset   = 0;
    unsigned flags     = 0;
    L4::Cap<L4Re::Dataspace> ds;
    int i = _rm->find(&addr, &size, &offset, &flags, &ds);
    if (i && dbg_rm) VG_(debugLog)(2, "vcap", "vailed rm-find\n");

    if (dbg_rm) VG_(debugLog)(2, "vcap", "%s: addr %lx size %lx, cap %lx, offs %lx\n",
                  __func__, addr, size, ds.cap(), offset);

    if (ds.is_valid())
        vrm_update_segptr(seg, ds.cap(), offset, flags);
    if (dbg_rm) VG_(debugLog)(2, "vcap", "ds %lx node ptr @ %lx\n", ds.cap(), seg->dsNodePtr);
}


/*
 * Update node pointer information on an exisitng NSegment
 */
void vrm_update_segptr(NSegment *seg, l4_cap_idx_t dscap, unsigned offset, unsigned flags)
{
    typedef L4Re::Util::Region_handler<L4::Cap<L4Re::Dataspace>,
                                       Vcap::Rm::Region_ops>  Region_handler;

    L4::Cap<L4Re::Dataspace> dummy(dscap);
    /*
     * If vcap is not yet running, we may assume, that Valgrind's dynamic memory
     * manager is not running yet as well. Therefore, using new() to create a node
     * would not work. Instead, we use a placement allocator in the early stages.
     *
     * -> exists for calls from vrm_segment_fixup() that end up here
     */
    static unsigned _mbuf_index = 0;     //< next index into early placement buf
    static char _early_malloc_buf[8192]; //< early placement buf

    Vcap::Rm::node *n;

    if (vcap_running)
        n = new Vcap::Rm::node(L4Re::Util::Region(seg->start, seg->end),
                                           Region_handler(dummy, dummy.cap(),
                                                          offset, flags));
    else {
        n = new (_early_malloc_buf + _mbuf_index) 
                Vcap::Rm::node(L4Re::Util::Region(seg->start, seg->end),
                                                  Region_handler(dummy, dummy.cap(),
                                                                 offset, flags));
        _mbuf_index += sizeof(Vcap::Rm::node);
    }

    VG_(am_set_nodeptr)(seg, (Addr)n);
    if (dbg_rm) {
        VG_(debugLog)(4, "vcap", "\033[32mupdate_segment %lx (%lx): node %p\033[0m\n",
                      seg->start, seg, seg->dsNodePtr);
    }
}
