/*!
 * \file
 * \brief  A terminal using an L4Re::Framebuffer via L4::Con
 *
 * \date
 * \author Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 */
/*
 * (c) 2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/re/console>
#include <l4/re/env>
#include <l4/re/event>
#include <l4/re/event_enums.h>
#include <l4/re/namespace>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/video/goos_fb>
#include <l4/re/util/object_registry>
#include <l4/sys/err.h>
#include <l4/sys/kdebug.h>
#include <l4/libgfxbitmap/font.h>
#include <l4/libgfxbitmap/support>
#include <l4/lib_vt100/vt100.h>
#include <l4/event/event>
#include <l4/cxx/ipc_stream>
#include <l4/cxx/ipc_server>
#include <l4/cxx/exceptions>
#include <l4/util/util.h>
#include <l4/re/util/icu_svr>
#include <l4/re/util/vcon_svr>
#include <l4/re/util/br_manager>
#include <l4/sys/typeinfo_svr>
#include <l4/sys/semaphore>

#include <pthread-l4.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <new>

static L4Re::Util::Video::Goos_fb fb;
static void *fb_addr;
static L4::Cap<L4Re::Dataspace> ev_ds;
static L4::Cap<L4::Semaphore> ev_irq;

static L4Re::Event_buffer ev_buffer;

static L4Re::Video::View::Info fbi;
static l4_uint32_t fn_x, fn_y;

// vt100 interpreter of ouput stream from client
termstate_t *term;

class Terminal : public L4::Server_object_t<L4::Vcon>,
                 public L4Re::Util::Icu_cap_array_svr<Terminal>,
                 public L4Re::Util::Vcon_svr<Terminal>
{
public:
  typedef L4Re::Util::Icu_cap_array_svr<Terminal> Icu_svr;
  typedef L4Re::Util::Vcon_svr<Terminal>          Vcon_svr;

  explicit Terminal();

  L4_RPC_LEGACY_DISPATCH(L4::Vcon);
  int dispatch(l4_umword_t obj, L4::Ipc::Iostream &ios)
  { return dispatch<L4::Ipc::Iostream>(obj, ios); }

  void trigger() { _irq.trigger(); }

  unsigned vcon_read(char *buf, unsigned size) throw();
  void vcon_write(const char *buf, unsigned size) throw();
  int vcon_set_attr(l4_vcon_attr_t const *attr) throw();
  int vcon_get_attr(l4_vcon_attr_t *attr) throw();

private:
  Icu_svr::Irq _irq;
};

Terminal *terminal;


// vt100 interpreter backend
static void con_puts(termstate_t *term, int x, int y, l4_int8_t *s, int len,
                      unsigned fg, unsigned bg)
{
  (void)term;
  gfxbitmap_font_text(fb_addr, (l4re_video_view_info_t *)&fbi,
                      0, (char*)s, len, fn_x * x, fn_y * y, fg, bg);

  fb.refresh(fn_x * x, fn_y * y, fn_x * len, fn_y);
}

static termstate_t *term_init(int cols, int rows, int hist)
{
  // get new termstate
  termstate_t *term = (termstate_t *)malloc(sizeof(termstate_t));
  if (!term)
    {
      printf("malloc for new term failed");
      return 0;
    }

  // explizitely init. these, as they may be freed in init_termstate
  term->text = 0;
  term->attrib = 0;
  term->newline = 1;

  // init termstate
  if ((vt100_init(term, cols, rows, hist)))
    {
      free(term);
      return 0;
    }

  return term;
}

static const char* init()
{
  // initialize cons frontend

  fb.setup("fb");

  fb_addr = fb.attach_buffer();

  if (fb.view_info(&fbi))
    return "Cannot get framebuffer info\n";

  gfxbitmap_font_init();
  fn_x = gfxbitmap_font_width(0);
  fn_y = gfxbitmap_font_height(0);

  // convert colors to the format used by our framebuffer
  libterm_init_colors(&fbi);

  // initialize terminal library
  term = term_init(fbi.width / fn_x, fbi.height / fn_y, 50);

  // initialize input event handling

  ev_ds = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
  if (!ev_ds.is_valid())
    return "Cannot allocate cap\n";

  ev_irq = L4Re::Util::cap_alloc.alloc<L4::Semaphore>();
  if (!ev_irq.is_valid())
    return "Cannot allocate cap\n";

  if (l4_error(L4Re::Env::env()->factory()->create(ev_irq)))
    return "Could not create event IRQ\n";

  if (l4_error(L4::cap_cast<L4Re::Console>(fb.goos())->bind(0, ev_irq)))
    return "Could not bind event IRQ\n";

  if (L4::cap_cast<L4Re::Console>(fb.goos())->get_buffer(ev_ds))
    return "Cannot get event dataspace and irq\n";

  l4_addr_t sz = ev_ds->size();
  void* _buf = 0;

  if (L4Re::Env::env()->rm()->attach(&_buf, sz, L4Re::Rm::Search_addr, ev_ds))
    return "Cannot attach event dataspace\n";

  ev_buffer = L4Re::Event_buffer(_buf, sz);

  return 0;
}

static void
touch_repeat(termstate_t * term, unsigned code, unsigned repeat)
{
 (void)term; (void)code; (void)repeat;
}

static void handle_key(L4Re::Event_buffer::Event *e)
{
  switch (e->payload.code)
    {
    case L4RE_KEY_RIGHTSHIFT:        // modifiers
    case L4RE_KEY_LEFTSHIFT:
      if (e->payload.value)
        term->__shift = 1;
      else
        term->__shift = 0;
      touch_repeat(term, e->payload.code, 0);
      break;
    case L4RE_KEY_LEFTCTRL:
    case L4RE_KEY_RIGHTCTRL:
      if (e->payload.value)
        term->__ctrl  = 1;
      else
        term->__ctrl  = 0;
      touch_repeat(term, e->payload.code, 0);
      break;
    case L4RE_KEY_LEFTALT:
      if (e->payload.value)
        term->__alt  = 1;
      else
        term->__alt  = 0;
      touch_repeat(term, e->payload.code, 0);
      break;
    case L4RE_KEY_RIGHTALT:
      if (e->payload.value)
        term->__altgr  = 1;
      else
        term->__altgr  = 0;
      touch_repeat(term, e->payload.code, 0);
      break;
    case L4RE_KEY_PAGEUP:            // special terminal movement chars
      if (e->payload.value && term->__shift)
        {
          vt100_scroll_up(term, term->phys_h / 2); // scroll for half screen
          vt100_redraw(term);
          touch_repeat(term, e->payload.code, e->payload.value);
        }
      break;
    case L4RE_KEY_PAGEDOWN:
      if (e->payload.value && term->__shift)
        {
          vt100_scroll_down(term, term->phys_h / 2); // scroll for half screen
          vt100_redraw(term);
          touch_repeat(term, e->payload.code, e->payload.value);
        }
      break;
    }

  if (e->payload.value)         // regular chars
    vt100_add_key(term, e->payload.code);
  touch_repeat(term, e->payload.code, e->payload.value);
}

namespace {

struct Ev_loop : public Event::Event_loop
{
  Ev_loop(L4::Cap<L4::Semaphore> irq, int prio) : Event::Event_loop(irq, prio) {}
  void handle()
  {
    L4Re::Event_buffer::Event *e;
    bool fire = false;
    while ((e = ev_buffer.next()))
      {
        if (e->payload.type == L4RE_EV_KEY)
          {
            handle_key(e);
            fire = true;
          }

        e->free();
      }

    if (fire)
      terminal->trigger();

    fire = false;
  }
};
}

// convert attributes to corresponding gfxbitmap_color_pix_t
static void attribs_to_colors(l4_int8_t a, gfxbitmap_color_pix_t *fg,
                              gfxbitmap_color_pix_t *bg)
{
    int fg_i, bg_i, in;

    unpack_attribs(a, &fg_i, &bg_i, &in);
    *fg = libterm_get_color(in, fg_i);
    *bg = libterm_get_color(in, bg_i);
}

// redraw whole screen
void vt100_redraw(termstate_t *term)
{
  int x, y;
  int old_x = 0, old_y = 0;
  l4_int8_t *s = NULL;
  l4_int8_t old_attrib = 0;
  int old_index = 0;
  gfxbitmap_color_pix_t fg, bg;

  //    LOGl("term = %p, text = %p, color = %p", term, term->text, term->color);
  if (term == NULL)
    return;
  // correct y for vis offset
  for (y = 0 - term->vis_off; y < term->phys_h - term->vis_off; y++)
    {
      for (x = 0; x < term->w; x++)
        {
          //vt100_redraw_xy(term, x, y);

          // if we observe a change in attributes, send the
          // accumulated string
          if (s != NULL && old_attrib != term->attrib[xy2index(term, x, y)])
            {
              attribs_to_colors(old_attrib, &fg, &bg);
              // correct y for vis offset
              con_puts(term, old_x, old_y + term->vis_off, s,
                       xy2index(term, x, y) - old_index, fg, bg);
              s = NULL;
            }
          // start a new string
          if (s == NULL)
            {
              old_index = xy2index(term, x, y);
              s = term->text + old_index;
              old_attrib = term->attrib[old_index];
              old_x = x;
              old_y = y;
            }
        }
      // care for end of line remainder strings
      if (s != NULL)
        {
          attribs_to_colors(old_attrib, &fg, &bg);
          // correct y for vis offset
          con_puts(term, old_x, old_y + term->vis_off, s,
              xy2index(term, x - 1, y) - old_index + 1, fg, bg);
          s = NULL;
        }
    }
}

void vt100_redraw_xy(termstate_t *term, int x, int y)
{
  gfxbitmap_color_pix_t fg, bg;
  l4_int8_t a;
  l4_int8_t *c;

  // if out of bound, do nothing
  if (y + term->vis_off >= term->phys_h)
    {
      return;
    }

  c = term->text + xy2index(term, x, y);
  a = term->attrib[xy2index(term, x, y)];
  attribs_to_colors(a, &fg, &bg);
  // correct for moved vis
  con_puts(term, x, y + term->vis_off, c, 1, fg, bg);
}

// vt100 backends, currently not implemented
void vt100_hide_cursor(termstate_t *term)
{
  if (term->cursor_vis)
    {
      // if the cursor is in visible area...
      if ( (term->cur_y + term->vis_off) <= term->phys_h )
        {
          int bg, fg, intensity;
          int index = xy2index( term, term->cur_x, term->cur_y );

          unpack_attribs( term->attrib[index], &fg, &bg, &intensity );
          term->attrib[index] = pack_attribs( bg, fg, intensity );

          // only redraw if cursor is visible
          vt100_redraw_xy( term, term->cur_x, term->cur_y );
        }
    }
}

void vt100_show_cursor(termstate_t *term)
{
  if (term->cursor_vis)
    {
      if ( (term->cur_y + term->vis_off) <= term->phys_h )
        {
          int bg, fg, intensity;
          int index = xy2index( term, term->cur_x, term->cur_y );

          unpack_attribs( term->attrib[index], &fg, &bg, &intensity );
          term->attrib[index] = pack_attribs( bg, fg, intensity );

          // only redraw if cursor is visible
          vt100_redraw_xy( term, term->cur_x, term->cur_y );
        }
    }

}

static L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks> server;

Terminal::Terminal()
  : Icu_svr(1, &_irq)
{
}

void
Terminal::vcon_write(const char *buf, unsigned len) throw()
{
  char mbuf[len];
  memcpy(mbuf, buf, len);

  //printf("%d: %.*s\n", len, (int)len, mbuf);

  if (term)
    vt100_write(term, mbuf, len);
  else
    L4Re::Env::env()->log()->printn(mbuf, len);
}

unsigned
Terminal::vcon_read(char *buf, unsigned len) throw()
{
  int c = 0;
  char *b = buf;
  while (len && (c = vt100_trygetchar(term)) != -1)
    {
      *b = c;
      ++b;
      --len;
    }
  return b - buf;
}

int
Terminal::vcon_set_attr(l4_vcon_attr_t const *attr) throw()
{
  term->echo    = attr->l_flags & L4_VCON_ECHO;
  term->newline = attr->o_flags & L4_VCON_ONLCR;
  return -L4_EOK;
}

int
Terminal::vcon_get_attr(l4_vcon_attr_t *attr) throw()
{
  attr->l_flags = term->echo ? L4_VCON_ECHO : 0;
  attr->o_flags = term->newline ? L4_VCON_ONLCR : 0;
  attr->i_flags = 0;
  return -L4_EOK;
}

class Controller : public L4::Server_object_t<L4::Factory>
{
public:
  int dispatch(l4_umword_t obj, L4::Ipc::Iostream &ios);
};

int
Controller::dispatch(l4_umword_t, L4::Ipc::Iostream &ios)
{
  l4_msgtag_t tag;
  ios >> tag;

  switch (tag.label())
    {
    case L4::Meta::Protocol:
      return L4::Util::handle_meta_request<L4::Factory>(ios);
    case L4::Factory::Protocol:
      break;
    default:
      return -L4_EBADPROTO;
    }

  L4::Factory::Proto op;
  ios >> op;

  switch (op)
  {
  case L4::Vcon::Protocol:
      {
        try
          {
            Terminal *t = new Terminal;
            ios << server.registry()->register_obj(t);
          }
        catch (L4::Runtime_error const &e)
          {
            return e.err_no();
          }
        catch (std::bad_alloc const &)
          {
            return -L4_ENOMEM;
          }
      }
      return L4_EOK;
    default:
      return -L4_ENOSYS;
  }
}


int main()
{
  const char* error = init();
  if (error)
    {
      printf("%s", error);
      exit(1);
    }

  Ev_loop event(ev_irq, 0xff);
  if (!event.attached())
    return 1;

  event.start();

  Terminal _terminal;
  terminal = &_terminal;
  if (!server.registry()->register_obj(&_terminal, "term"))
    {
      printf("Terminal registration failed.\n");
      return 1;
    }

  if (0)
    {
      Controller ctrl;
      if (!server.registry()->register_obj(&ctrl, "terminal"))
        {
          printf("Terminal ctrl registration failed.\n");
          return 1;
        }
    }

  server.loop();
}

