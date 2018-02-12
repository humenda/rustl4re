/*
 * memory.cc --
 *
 * Memory management implementation
 *
 * (c) 2011-2013 Björn Döbel <doebel@os.inf.tu-dresden.de>,
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include "memory"
#include "app_loading"
#include "locking.h"
#include <l4/sys/kdebug.h>
#include <pthread-l4.h>

#define MSG() DEBUGf(Romain::Log::Memory)

int Romain::Region_ops::map(Region_handler const *h, l4_addr_t addr,
                            L4Re::Util::Region const &r, bool writable,
                            l4_umword_t *result)
{
	(void)h;
	(void)addr;
	(void)r;
	(void)writable;
	(void)result;
	enter_kdebug("ops::map");
	return -1;
}

void Romain::Region_ops::unmap(Region_handler const *h, l4_addr_t vaddr,
                               l4_addr_t offs, l4_umword_t size)
{
	(void)h;
	(void)vaddr;
	(void)offs;
	(void)size;
	enter_kdebug("ops::unmap");
}


void Romain::Region_ops::free(Region_handler const *h, l4_addr_t start, l4_umword_t size)
{
	if ((h->flags() & L4Re::Rm::Reserved))
		return;

	if (h->flags() & L4Re::Rm::Pager)
		return;

	L4::Cap<L4Re::Dataspace> ds(h->client_cap_idx());
	ds->clear(h->offset() + start, size);
	
	//enter_kdebug("ops::free");
}


void Romain::Region_ops::take(Region_handler const* h)
{
	(void)h;
	enter_kdebug("ops::take");
}


void Romain::Region_ops::release(Region_handler const* h)
{
	(void)h;
	enter_kdebug("ops::release");
}


Romain::Region_map::Region_map()
	: Base(0,0), _active_instance(Invalid_inst)
{
	pthread_mutex_init(&_mtx, 0);

	l4_kernel_info_t *kip = l4re_kip();
	MSG() << "kip found at 0x" << std::hex << kip
	      << " having " << L4::Kip::Mem_desc::count(kip)
	      << " entries.";

	L4::Kip::Mem_desc *d = L4::Kip::Mem_desc::first(kip);
	l4_umword_t count  = L4::Kip::Mem_desc::count(kip);

	for (L4::Kip::Mem_desc *desc = d; desc < d + count; ++desc) {

		if (!desc->is_virtual())
			continue;

		l4_addr_t s = desc->start() == 0 ? 0x1000 : desc->start();
		l4_addr_t e = desc->end();
		MSG() << "virtual range: [" << std::hex << s << "-" << e << "] ";

		switch(desc->type()) {
			case L4::Kip::Mem_desc::Conventional:
				set_limits(s,e);
				break;
			default:
				WARN() << "type " << desc->type() << "???";
				break;
		}

	}
}


/*
 * Attach a replica region locally in the master AS.
 *
 * The Romain::Master manages replicas' region map. As a part of this, the master
 * attaches a copy of every region attached to one of the replicas to its own
 * address space. This local region is then used to service page faults caused by
 * the replicas.
 *
 * For read-only replica dataspaces, the master creates a writable copy of the
 * original region in a separate dataspace. This allows us to modify their content
 * if necessary (e.g., setting debug breakpoints on read-only code).
 *
 * For writable dataspaces, the master simply attaches the same region that has
 * already been attached to the replica AS into its own address space and maintains
 * a copy for every replica.
 *
 * The replica copies lead to another problem: to reduce replication overhead, we
 * would like to handle page faults using as few memory mappings as possible. To
 * be able to do so, we however need to make sure that all replica copies are
 * aligned to the same start address. The Romain::Master does this by ensuring
 * that the replica address as well as the master-local addresses of the copies
 * are aligned to the same offset within a minimally aligned region. The minimum
 * alignment is defined by the Region_handler()'s MIN_ALIGNMENT member (XXX and
 * should at some pol4_mword_t become a configurable feature).
 *
 * The layout looks like this:
 *
 *  REPLICA:
 *
 * (1)      (2)                         (3)
 *  +- - - - +---------------------------+
 *  |        |                           |
 *  +- - - - +---------------------------+
 *
 *  MASTER (per copy):
 *
 * (A)      (B)                         (C)
 *  +- - - - +---------------------------+
 *  |        |                           |
 *  +- - - - +---------------------------+ 
 *
 *   (1)  correct minimum alignment
 *   (2)  region.start()
 *   (3)  region.end()
 *
 *   (A)  address aligned similar to (1)
 *   (B)  local_region[inst]->start()
 *   (C)  local_region[inst]->end()
 *
 *   MIN_ALIGNMENT_MASK := (1 << MIN_ALIGNMENT) - 1
 *   (1) := (2) - [(2) & MIN_ALIGNMENT_MASK]
 *   (A) := (B) - [(B) & MIN_ALIGNMENT_MASK]
 *
 *  Attaching then works by first reserving an area with the RM
 *  that has the size (C) - (A). Then we attach the dataspace to
 *  (B) and free the reserved area so that the remaining memory
 *  can again be freely used.
 */
void *Romain::Region_map::attach_locally(void* addr, l4_umword_t size,
                                         Romain::Region_handler *hdlr,
                                         l4_umword_t flags, l4_uint8_t align)
{
	(void)addr;
	Romain::Region_handler::Dataspace const *ds = &hdlr->local_memory(0);
	L4Re::Dataspace::Stats dsstat;
	(*ds)->info(&dsstat);

#if 1
	MSG() << PURPLE
		  << "attaching DS " << std::hex << ds->cap()
	      << ", addr = "     << addr
	      << ", flags = "    << flags << " (" << dsstat.flags << ")"
	      << ", size = "     << size
	      << ", align = "    << (l4_umword_t)align
	      << NOCOLOR;
#endif

	if (!ds->is_valid()) {
		enter_kdebug("attaching invalid cap");
	}

	/*
	 * Here's what the variables below mean in the original DS. If the DS is
	 * read-only, we still want a writable version, because some users, such as
	 * GDB, want to modify ro data (e.g., GDB breakpoints). Therefore, we then
	 * create a copy of the original DS and adapt the variables afterwards.
	 *
	 * 1      2    3    4                      5
	 * +------+----+----+----------------------+
	 * |      |'..'|..'.|                      |       Original DS
	 * |      |.''.|''.'|                      |
	 * +------+----+----+----------------------+
	 *        \         \
	 *         \         \
	 *          \         \
	 *           \         \
	 *           +---------+
	 *           |''.''.''.|                           New DS
	 *           |..'..'..'|
	 *           +---------+
	 *           6         7
	 *
	 * 1 .. original DS start
	 * 2 .. page base of region (page_base)
	 * 3 .. offset within DS    (hdlr()->offset)
	 * 4 .. region end          (hdlr()->offset + size)
	 * 5 .. original DS end
	 * 6 .. new DS start
	 * 7 .. new DS end
	 *
	 * offset_in_page = $3 - $2
	 * eff_size       = size + offset_in_page rounded to page size
	 *
	 * After copy:
	 *    page_base      = 0
	 *    hdlr()->offset = 0
	 *
	 */

	l4_addr_t page_base      = l4_trunc_page(hdlr->offset());
	l4_addr_t offset_in_page = hdlr->offset() - page_base;
	l4_umword_t eff_size        = l4_round_page(size + offset_in_page);

#if 1
	MSG() << "  page_base " << (void*)page_base << ", offset " << std::hex << offset_in_page
	      << ", eff_size " << eff_size << ", hdlr->offset " << hdlr->offset();
#endif

	hdlr->offset(page_base /*hdlr->offset() - offset_in_page*/);

	/*
	 * If the dataspace we attach was originally not mapped writable,
	 * make a writable copy here, so that fault handlers such as GDB
	 * can instrument this memory even if the client should not get
	 * it writable.
	 */
	if (!(dsstat.flags & L4Re::Dataspace::Map_rw)) {
		//MSG() << "Copying to rw dataspace.";
		L4::Cap<L4Re::Dataspace> mem;

		Romain::Region_map::allocate_ds(&mem, eff_size);

		mem->copy_in(0, *ds, page_base, hdlr->offset() + size);
		ds        = &mem; // taking pointer to stack?? XXX
		flags    &= ~L4Re::Rm::Read_only;
		page_base = 0;
		/*
		 * XXX: If we copied the dataspace above, what happens to the
		 *      original DS that was passed in? Shouldn't it be released?
		 *
		 *      No. It might still be attached elsewhere. Perhaps decrement
		 *      some refcnt? XXX check
		 */
	}

	/*
	 * flags contains the replica's flags setting. If the replica asks to attach into
	 * a previously reserved region, then this is only valid for the replica's
	 * rm::attach() call, but not for attaching our local version/copy of the DS.
	 */
	flags &= ~L4Re::Rm::In_area;

	l4_addr_t a = Romain::Region_map::attach_aligned(ds, eff_size,
	                                                 page_base, flags,
	                                                 hdlr->alignment(),
	                                                 hdlr->align_diff());

	Romain::Region reg(a, a+eff_size - 1);

	MSG() << "set_local_region(" << _active_instance << ", "
		  << std::hex << reg.start() << ")";
	hdlr->set_local_region(_active_instance, reg);
	return (void*)(a + offset_in_page);
}

void *Romain::Region_map::attach(void* addr, l4_umword_t size,
                                 Romain::Region_handler const &hdlr,
                                 l4_umword_t flags, l4_uint8_t align, bool shared)
{
	void *ret = 0;

	MSG() << std::hex << addr << " " << size << " " << (int)align << " " << flags;
	MSG() << "cap " << std::hex << hdlr.client_cap_idx();

	/*
	 * XXX: the only reason for this Region_handler to exist is to copy
	 *      the shared parameter into the handler. Shouldn't the caller
	 *      do this???
	 */
	Romain::Region_handler _handler(hdlr);
	_handler.shared(shared);

	/*
	 * First, do the attach() in the remote address space. Thereby we get
	 * the remote address and can then during local attaching make sure
	 * that the memory alignment of the local mapping matches the remote
	 * alignment. This allows us to play mapping tricks later.
	 * 
	 * Note, we can only do this if the Search_addr flag is set, otherwise
	 * we might hurt the client's expectations about the mem region.
	 */
	if ((flags & L4Re::Rm::Search_addr) and 
	    !(flags & L4Re::Rm::In_area) and
	    (size > L4_SUPERPAGESIZE) and
	    (align < L4_LOG2_SUPERPAGESIZE)) {
		MSG() << "Forcing attach() with size " << std::hex << size
		      << " to use superpage mappings.";
		align = L4_LOG2_SUPERPAGESIZE;
		size  = l4_round_size(size, L4_SUPERPAGESHIFT);
	}
	

	ret = Base::attach(addr, size, _handler, flags, align);
	MSG() << "Base::attach = " << std::hex << ret << " hdlr.offs: " << _handler.offset();
	if (ret == (void*)~0UL) {
		INFO() << std::hex << addr << " " << size << " " << (int)align << " " << flags;
		enter_kdebug("Attach error");
	}

	/*
	 * The node is now present in the region map. This means we now need
	 * to continue working with the node's handler member instead of our
	 * local copy (because the attach() call has made a copy of it).
	 */
	Romain::Region_map::Base::Node n = find((l4_addr_t)ret);
	// and the region handler is const by default ... *grrrml*
	Romain::Region_handler* theHandler = const_cast<Romain::Region_handler*>(&(n->second));
	
	/*
	 * Figure out the best possible alignment we can have between the local
	 * and the remote region. If the alignments match, we can later map more
	 * than one page at a time.
	 */
	theHandler->alignToAddressAndSize(reinterpret_cast<l4_addr_t>(ret), size);

	/* Only attach locally, if this hasn't been done beforehand yet. */
	if (!n->second.local_region(_active_instance).start()) {
		void* a = attach_locally(addr, size, theHandler,
		                         flags, theHandler->alignment());
		_check(a == 0, "Error in local attach");
	}

	MSG() << "new mapping (" << _active_instance << ") "
	      << "[" << std::hex << n->second.local_region(_active_instance).start()
	      << " - " << n->second.local_region(_active_instance).end()
	      << "] -> " << ret;
	//enter_kdebug("after attach");

	return ret;
}


enum {
	NUM_DETACH = 32
};
pthread_t detacher;
pthread_mutex_t det_mtx;
l4_umword_t det_read, det_write;
sem_t     detach_full, detach_empty;

struct DetachJob
{
	Romain::Region_handler rh;
	l4_umword_t offset;
	l4_umword_t size;
} ;

DetachJob handlers[NUM_DETACH];

static void detach_region(DetachJob *dj)
{
	for (l4_umword_t idx = 0; idx < dj->rh.local_region_count(); ++idx) {
		if (dj->rh.local_region(idx).start() == 0) {
			continue;
		}

		MSG() << "detaching [" << std::hex
			  << dj->rh.local_region(idx).start() + dj->offset << " - "
			  << dj->rh.local_region(idx).start() + dj->offset + dj->size
			  << "]";

		L4::Cap<L4Re::Dataspace> memcap;
		l4_mword_t r = L4Re::Env::env()->rm()->detach(dj->rh.local_region(idx).start() + dj->offset,
											   dj->size,
											   &memcap, L4Re::This_task);
		MSG() << "detached locally: " << r << " cap: " << std::hex << memcap.cap();
		if (((r & L4Re::Rm::Detach_result_mask) == L4Re::Rm::Detached_ds) and
			//(memcap.cap() != L4_INVALID_CAP) and
			((memcap.cap() >> L4_CAP_SHIFT) < Romain::FIRST_REPLICA_CAP)) {
                        L4Re::Util::cap_alloc.free(memcap, L4Re::This_task);
		}

		if (!dj->rh.writable()) {
			/* For read-only regions, we only need to detach once */
			break;
		}
	}
}

static void *detach_thread(void *arg)
{
	l4_debugger_set_object_name(pthread_l4_cap(pthread_self()), "romain::detacher");

	while (1) {
		sem_wait(&detach_full);

		detach_region(&handlers[det_read]);

		det_read++;
		det_read %= NUM_DETACH;

		sem_post(&detach_empty);
	}

	return 0;
}


int Romain::Region_map::detach(void* /* IN */ addr, l4_umword_t /* IN */ size,
                               l4_umword_t /* IN */ flags,
                               L4Re::Util::Region* /* OUT */ reg,
                               Romain::Region_handler* /* OUT */ h)
{
	static bool thread_running = false;

	if (!thread_running) {
		det_read  = 0;
		det_write = 0;
		pthread_mutex_init(&det_mtx, 0);
		sem_init(&detach_full, 0, 0);
		sem_init(&detach_empty, 0, NUM_DETACH);

		l4_mword_t r = pthread_create(&detacher, 0, detach_thread, 0);
		_check(r != 0, "error creating detacher");

		thread_running = true;
	}
	
	//sem_wait(&detach_empty);
		/* First, do a lookup for the handler, because Base::Detach() will not always
		 * return the handler node, but we need it for detach later.
		 */
		l4_addr_t srch = (l4_addr_t)addr;
		Node n = Base::find(Region(srch, srch + size - 1));

		MSG() << "detaching... " << Region_map::print_mapping(n, 0);

		handlers[det_write].rh = n->second;
		if (handlers[det_write].rh.client_cap_idx() == L4_INVALID_CAP) {
			enter_kdebug("invalid cap!");
		}
		handlers[det_write].offset = srch - n->first.start();
		if (size <= n->first.size())
			handlers[det_write].size   = size;
		else
			handlers[det_write].size   = n->first.size();
		detach_region(&handlers[det_write]);
	//det_write++;
	//det_write %= NUM_DETACH;
	//sem_post(&detach_full);

	MSG() << std::hex << "Base::detach(" << (l4_addr_t)addr << " " << size << " " << flags << ")";
	MSG() << std::hex << find((l4_addr_t)addr)->second.client_cap_idx();
	l4_mword_t ret = Base::detach(addr, size, flags, reg, h);
	MSG() << std::hex << "Base::detach(): " << ret << " r.start: " << reg->start() << " " << reg->size();

	return ret;
}


bool Romain::Region_map::lazy_map_region(Romain::Region_map::Base::Node &n, l4_umword_t inst, bool iswritepf)
{
	/* already mapped? */
	if (n->second.local_region(inst).start()) {
		/* If this mapping already exists, we don't need to
		 * create a copy here. However, before serving the
		 * very first page fault in this region, we have to
		 * make sure that this memory is really paged into
		 * the master's address space. Hence, we touch the
		 * region here.
		 */
		if (n->second.is_ro()) {
			l4_touch_ro((void*)n->second.local_region(inst).start(),
			            n->second.local_region(inst).size());
		} else {
			l4_touch_rw((void*)n->second.local_region(inst).start(),
			            n->second.local_region(inst).size());
		}


		return false;
	}

	MSG() << "start " <<  n->second.local_region(inst).start();
	MSG() << "replica without yet established mapping.";
	MSG() << "ro: " << n->second.is_ro();
	MSG() << "shared: " << (n->second.shared() ? "true" : "false");

	/*
	 * As we found a node, we know there exists at least one replica
	 * that already has this mapping. Therefore, we go and look for
	 * one of those mappings to start from.
	 */
	l4_mword_t existing = find_existing_region(n);
	_check(existing < 0, "no mapping???");
	_check(existing > Romain::MAX_REPLICAS, "no mapping???");

	Romain::Rm_guard(this, inst);
	Romain::Region_handler *rh = const_cast<Romain::Region_handler*>(&n->second);

	/*
	 * Case 1: region is read-only -> we share the mapping from
	 *         the first node, because it was already established.
	 */
	if (n->second.is_ro() or rh->shared()) {
		/*
		 * XXX: Why is setting local_region and memory split up?
		 */
		rh->set_local_region(inst, n->second.local_region(existing));
		rh->local_memory(inst, n->second.local_memory(existing));
	} else {
			MSG() << "Copying existing mapping.";
			bool b = copy_existing_mapping(*rh, existing, inst, iswritepf);
			_check(!b, "error creating rw copy");
	}
	return true;
}

