/*
 * (c) 2014 Steffen Liebergeld <steffen.liebergeld@kernkonzept.com>
 *
 * This file is licensed under the terms of the GNU General Public License 2.
 * See file COPYING-GPL-2 for details.
 */
#define L4_RPC_DISABLE_LEGACY_DISPATCH 1
#include <l4/re/util/video/goos_svr>
#include <l4/re/util/object_registry>
#include <l4/re/util/dataspace_svr>
#include <l4/sys/cxx/ipc_epiface>

#include <l4/log/log.h>

#include <asm/server/server.h>
#include <asm/server/fb-srv.h>

#define PREFIX "l4fb-srv: "

class Fb : public L4Re::Util::Video::Goos_svr,
           public L4Re::Util::Dataspace_svr,
           public l4x_srv_epiface_t<Fb, L4::Kobject_2t<void, L4Re::Video::Goos, L4Re::Dataspace> >
{
public:
	using L4Re::Util::Video::Goos_svr::op_info;
	using L4Re::Util::Dataspace_svr::op_info;

	Fb();
	~Fb() throw() {}
	int map_hook(l4_addr_t offs, unsigned long flags,
	             l4_addr_t min, l4_addr_t max);
	void set(struct l4fb_info *info, L4::Cap<L4::Thread> thread);
	int refresh(int x, int y, int w, int h);

	struct l4x_fb_srv_ops *ops;
private:
	bool _map_done, _active;
	l4_addr_t _vidmem_start, _vidmem_size;
};

Fb::Fb()
 : _map_done(false), _active(0), _vidmem_start(0), _vidmem_size(0)
{
}

int
Fb::map_hook(l4_addr_t, unsigned long flags, l4_addr_t, l4_addr_t)
{
	// map everything at once, a framebuffer will usually be used fully
	int err;
	unsigned long sz = 1;
	l4_addr_t off, a;
	unsigned fl;
	L4::Cap<L4Re::Dataspace> ds;

	if (_map_done)
		return 0;

	a = _vidmem_start;

	if ((err = L4Re::Env::env()->rm()->find(&a, &sz, &off, &fl, &ds)) < 0) {
		LOG_printf(PREFIX "failed to query video memory: %d\n", err);
		return err;
	}

	if ((err = ds->map_region(off + (_vidmem_start - a), flags,
	                          _vidmem_start, _vidmem_start +
	                          _vidmem_size)) < 0) {
		LOG_printf(PREFIX "failed to map video memory: %d\n", err);
		return err;
	}

	_map_done = 1;
	return 0;
}


void
Fb::set(struct l4fb_info *info, L4::Cap<L4::Thread> thread)
{
	_vidmem_start       = info->screen_base;
	_vidmem_size        = info->screen_size;
	_screen_info.width  = info->i.width;
	_screen_info.height = info->i.height;
	_screen_info.flags  = L4Re::Video::Goos::F_auto_refresh;
	_screen_info.pixel_info =
		L4Re::Video::Pixel_info(info->i.pixel_info.bytes_per_pixel,
		                        info->i.pixel_info.r.size,
		                        info->i.pixel_info.r.shift,
		                        info->i.pixel_info.g.size,
		                        info->i.pixel_info.g.shift,
		                        info->i.pixel_info.b.size,
		                        info->i.pixel_info.b.shift,
		                        info->i.pixel_info.a.size,
		                        info->i.pixel_info.a.shift);

	_view_info.buffer_offset  = 0;
	_view_info.bytes_per_line = info->bytes_per_scanline;
	_rw_flags                 = Writable;
	_cache_flags = L4::Ipc::Gen_fpage<L4::Ipc::Snd_item>::Buffered;

	init_infos();

	_ds_start = _vidmem_start;
	_ds_size  = _vidmem_size;

	L4::Cap<L4Re::Video::Goos> c;

	c = L4Re::Env::env()->get_cap<L4Re::Video::Goos>("fbsrv");
	if (!c) {
		LOG_printf(PREFIX "Error finding IPC gate \"fbsrv\".\n");
		return;
	}

	_fb_ds = L4::cap_reinterpret_cast<L4Re::Dataspace>(c);

	if (!l4x_srv_register_name(this, thread, "fbsrv")) {
		LOG_printf(PREFIX "Could not establish fb server.\n");
		return;
	}

	_active = true;
}

int
Fb::refresh(int, int, int, int)
{
	/* No area-based redraw functionality in framebuffer infrastructure,
	 * so only use this on real hardware framebuffers */
	return L4_EOK;
}

static Fb _fb;

static Fb *fb_obj()
{
	return &_fb;
}

extern "C" void
l4x_fb_srv_set(struct l4fb_info *info, l4_cap_idx_t thread)
{
	fb_obj()->set(info, L4::Cap<L4::Thread>(thread));
}
