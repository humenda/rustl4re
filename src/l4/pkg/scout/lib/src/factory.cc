#include "factory.h"

#include <l4/mag-gfx/mem_factory>
#include <l4/mag-gfx/gfx_colors>

#include <l4/scout-gfx/fade_icon>
#include <l4/scout-gfx/pt_factory>

#include "panel.h"


using Mag_gfx::Pixel_info;

template< typename PT >
class Csf : public ::Factory, public Scout_gfx::Pt::Factory<PT>
{
public:

  Scout_gfx::Parent_widget *create_panel(Scout_browser *b)
  { return new Panel<PT>(this, b); }
};


static Csf<Mag_gfx::Rgb15>  _csf_rgb15;
static Csf<Mag_gfx::Bgr15>  _csf_bgr15;
static Csf<Mag_gfx::Rgb16>  _csf_rgb16;
static Csf<Mag_gfx::Bgr16>  _csf_bgr16;
static Csf<Mag_gfx::Rgb24>  _csf_rgb24;
static Csf<Mag_gfx::Rgb32>  _csf_rgb32;


