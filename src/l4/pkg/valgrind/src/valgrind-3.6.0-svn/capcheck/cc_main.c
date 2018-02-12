
/*--------------------------------------------------------------------*/
/*--- CapCheck - L4Re capability usage checker           cc_main.c ---*/
/*--------------------------------------------------------------------*/

/*
	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License as
	published by the Free Software Foundation; either version 2 of the
	License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful, but
	WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
	02111-1307, USA.

	The GNU General Public License is contained in the file COPYING.
*/

#include "pub_tool_basics.h"
#include "pub_tool_tooliface.h"
#include <pub_tool_stacktrace.h>   // VG_(get_StackTrace)()
#include <pub_tool_threadstate.h>  // VG_(get_running_tid)()
#include <pub_tool_libcprint.h>    // VG_(printf)
#include <pub_tool_libcassert.h>    // VG_(assert)
#include <valgrind.h>              // CALL_FN...
#include <pub_tool_redir.h>        // VG_WRAP...
#include <pub_tool_hashtable.h>    // VG_(HT_*)()

#include <pub_core_basics.h>
#include <pub_core_vki.h>
#include <pub_core_threadstate.h>

#include <l4/sys/consts.h>
#include <l4/sys/types.h>
#include <l4/sys/ipc.h>

#if defined(VGO_l4re)
HChar *VG_(toolname)="capchk";
#endif

// XXX: distinguish what we know about a mapping
//      1) well-known protocol wrappers - we exactly know which
//         protocols belong to this -> flag everything else as an
//         error
//      2) mappings from syscall wrap - track which protocols are
//         used and use heuristic to determine "wrong" invocations
//          (a) invalid protocol error code
//          (b) number of type X invocations vs. number of all
//              invocations

enum MapType {
	Mt_None,        // no mapping yet
	Mt_Known,       // mapped through known protocol
	Mt_Speculative, // found mapping in syscall wrapper
};

enum MapFlags {
	Mf_Empty        = 0x0, // No flags
	Mf_AllowOvermap = 0x1, // overmap is ok
};

typedef struct cc_hash_node {
	struct cc_hash_node *next;
	UWord key;
	ExeContext *where;  /* where the cap was allocated */
	ExeContext *mapped; /* where the cap was mapped ! */
	unsigned   maptype; /* how well do we know this object? */
	unsigned   flags;   /* MapFlags */
} CC_hash_node;

static VgHashTable cap_alloc_list = NULL;
static int _alloc_count,
		   _free_count,
		   _syscall_count,
		   _overmap_count;

/***********************
 * Utility functions   *
 ***********************/

static CC_hash_node *cc_create_hash_node(ExeContext *ec, long idx)
{
	CC_hash_node *node = VG_(malloc)("CC hash node", sizeof(CC_hash_node));
	tl_assert2(node, "VG_(malloc) failed");
	node->key    = idx;
	node->where  = ec;
	node->mapped = NULL;
	node->maptype = Mt_None;
	node->flags  = Mf_Empty;
	return node;
}


static void print_stacktrace(ThreadId tid)
{
	Addr ips[10];
	unsigned int e = VG_(get_StackTrace)(tid, ips, 10, 0, 0, 0);
	VG_(pp_StackTrace)(ips, e);
}

/*
 * Tracking cap slot allocation and release
 */

void cc_track_cap_alloc(ThreadId tid, long idx)
{
	idx >>= L4_CAP_SHIFT;

	// idx == 0 means there was an error, so don't track this
	if (idx == 0)
		return;

	// XXX: check if already allocated -- too paranoid?

	ExeContext *ec  = VG_(record_ExeContext)(tid, 0);
	CC_hash_node *n = cc_create_hash_node(ec, idx);
	VG_(HT_add_node)(cap_alloc_list, n);

	++_alloc_count;
}


void cc_track_cap_free(ThreadId tid, long idx)
{
	idx >>= L4_CAP_SHIFT;

	/* 
	 * Remove. If this fails, we saw a cap_free() without a
	 * previous allocation. This is an error.
	 */
	CC_hash_node *n = VG_(HT_remove)(cap_alloc_list, idx);
	if (!n) {
		ExeContext *ec  = VG_(record_ExeContext)(tid, 0);
		VG_(printf)("ERROR: cap_free(%lx) on index that hasn't been allocated!\n",
					idx);
		VG_(pp_ExeContext)(ec);
	}
	else {
		// XXX: check whether cap slots were allocated but never mapped
		VG_(free)(n);
	}

	++_free_count;
}

/***************************************************************
 * Syscall wrapping
 *
 * Stolen from coregrind/m_syswrap/priv_types_n_macros.h
 **************************************************************/
// XXX: per-thread!
l4_buf_regs_t buf_regs_pre;

void cc_pre_syscall(ThreadId tid, UInt syscallno)
{
	/* 
	 * The buf regs _may_ contain mapping descriptors. Therefore, we store this
	 * information here. After the syscall, if we detect a mapping having taken
	 * place, we use this to reconstruct which mapping was established.
	 */
	VG_(memcpy)(&buf_regs_pre, l4_utcb_br_u(VG_(threads)[tid].utcb), sizeof(l4_buf_regs_t));
}


void cc_post_syscall(ThreadId tid, UInt syscallno, SysRes res)
{
	++_syscall_count;

	ThreadState *t = VG_(get_ThreadState)(tid);
	l4_msgtag_t tag;
	int e;
	tag.raw = t->arch.vex.guest_EAX;
	unsigned cap_idx = t->arch.vex.guest_EDX >> L4_CAP_SHIFT;

	/* Notify about invocation errors. */
	if ((e = l4_error(tag))) {
		VG_(printf)("\033[33mError %lx invoking cap %lx\033[0m\n", e, cap_idx);
		print_stacktrace(tid);
	}

	/* Cap mapping received?
	 *
	 * Reply tag is the sender's msgtag, so the number of items now
	 * is the number of mappings we received. As we got here, these
	 * mappings were successful (otherwise we would have bailed out
	 * earlier).
	 *
	 * This means, the stored buffer registers from pre_syscall()
	 * contain the receive descriptors for the new mapping(s). Now
	 * we search those for cap mappings (RCV_ITEM_SINGLE_CAP set).
	 */
	int idx = 0;
	for (e = 0; e < l4_msgtag_items(tag); ++e, ++idx) {
		if (buf_regs_pre.br[idx] & L4_RCV_ITEM_SINGLE_CAP) {
			long cap_idx = buf_regs_pre.br[idx] >> L4_CAP_SHIFT;
			ExeContext *ec  = VG_(record_ExeContext)(tid, 0);
			CC_hash_node *n = VG_(HT_lookup)(cap_alloc_list, cap_idx);
			tl_assert2(n, "Node lookup for %lx failed\n", cap_idx);

			/*
			 * Report overmap. This may be a false positive as we cannot
			 * track asynchronous unmaps of Mt_Speculative mappings.
			 *
			 * XXX Use suppressions to avoid known false positives.
			 *
			 * XXX May a command line option be useful?
			 */
			if (n->mapped && !(n->flags & Mf_AllowOvermap)) {
				VG_(printf)("\033[33mWARNING! Overmap detected for cap index \033[0m%lx\n", cap_idx);
				VG_(printf)("Old map backtrace:\n");
				VG_(pp_ExeContext)(n->mapped);
				VG_(printf)("Overmapped at:\n");
				VG_(pp_ExeContext)(ec);
			}

			n->mapped  = ec;
			n->maptype = Mt_Speculative;
		}
		else // this is a real buf mapping, so the descriptor is
			 // 2 words large and we need to increment idx once more
			++idx;
	}
}


static void cc_post_clo_init(void)
{
}


static
IRSB* cc_instrument ( VgCallbackClosure* closure,
                      IRSB* bb,
                      VexGuestLayout* layout,
                      VexGuestExtents* vge,
                      IRType gWordTy, IRType hWordTy )
{
	return bb;
}


static void cc_fini(Int exitcode)
{
	VG_(printf)("Cap allocations: %d, Cap frees: %d, System calls %d, Overmaps %d\n",
				_alloc_count, _free_count, _syscall_count, _overmap_count);

	// XXX: check whether cap slots were allocated but never mapped
	if (VG_(HT_count_nodes)(cap_alloc_list)) {
		VG_(printf)("--- Still allocated caps: ---\n");
		VG_(HT_ResetIter)(cap_alloc_list);
		CC_hash_node *n = VG_(HT_Next)(cap_alloc_list);
		while (n) {
			VG_(printf)("\033[31mCap still allocated:\033[0m %lx\n", n->key);
			VG_(printf)("     Allocation trace:\n");
			VG_(pp_ExeContext)(n->where);
			VG_(printf)("     Last map trace (%s mapping):\n", n->maptype == Mt_Speculative ? "speculative" : "well-known");
			if (n->mapped)
				VG_(pp_ExeContext)(n->mapped);
			else
				VG_(printf)("      Never mapped!\n");
			n = VG_(HT_Next)(cap_alloc_list);
		}
		VG_(printf)("-------       END     -------\n");
	}
}


static void cc_pre_clo_init(void)
{
	VG_(details_name)              ("CapCheck");
	VG_(details_version)           (NULL);
	VG_(details_description)       ("Capability allocation tracker");
	VG_(details_copyright_author)  ("Copyright (C) 2009 Technische Universität Dresden");
	VG_(details_bug_reports_to)    ("doebel@tudos.org");

	VG_(printf)("Setting event handlers.\n");
	VG_(track_cap_alloc)(cc_track_cap_alloc);
	VG_(track_cap_free) (cc_track_cap_free);
	VG_(needs_syscall_wrapper)(cc_pre_syscall, cc_post_syscall);

	VG_(printf)("Creating hash table\n");
	cap_alloc_list = VG_(HT_construct)("cap alloc list");
	VG_(printf)("   ht @ %p\n", cap_alloc_list);

	VG_(printf)("Initializing stuff\n");
	_alloc_count   = 0;
	_free_count    = 0;
	_syscall_count = 0;
	_overmap_count = 0;


	VG_(basic_tool_funcs) (cc_post_clo_init,
	                       cc_instrument,
	                       cc_fini);

	/* No needs, no core events to track */
}

VG_DETERMINE_INTERFACE_VERSION(cc_pre_clo_init)

/*--------------------------------------------------------------------*/
/*--- end                                                          ---*/
/*--------------------------------------------------------------------*/
