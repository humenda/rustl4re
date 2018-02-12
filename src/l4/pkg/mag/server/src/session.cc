
#include <l4/mag/server/session>

#include <algorithm>
#include <cstring>

void
Mag_server::Session::set_label_prop(Session *s, Property_handler const *, cxx::String const &v)
{
  if (s->_label)
    delete [] s->_label;

  if (v.empty())
    {
      s->_label = 0;
      return;
    }

  // limit label length to 256 letters
  unsigned l = std::min(v.len(), 256);
  s->_label = new char[l + 1];
  memcpy(s->_label, v.start(), l);
  s->_label[l] = 0;
}

void
Mag_server::Session::set_color_prop(Session *s, Property_handler const *, cxx::String const &v)
{
  typedef Mag_gfx::Rgb32::Color Color;

  if (v.empty())
    return;

  l4_uint32_t cval;
  if (v.eof(v.start() + v.from_hex(&cval)))
    {
      s->_color = Color((cval >> 16) & 0xff, (cval >> 8) & 0xff, cval & 0xff);
      return;
    }

  switch (v[0])
    {
    case 'r': s->_color = Color(130, 0, 0); break;
    case 'g': s->_color = Color(0, 130, 0); break;
    case 'b': s->_color = Color(0, 0, 130); break;
    case 'w': s->_color = Color(130, 130, 130); break;
    case 'y': s->_color = Color(130, 130, 0); break;
    case 'v': s->_color = Color(130, 0, 130); break;
    case 'R': s->_color = Color(255, 0, 0); break;
    case 'G': s->_color = Color(0, 255, 0); break;
    case 'B': s->_color = Color(0, 0, 255); break;
    case 'W': s->_color = Color(255, 255, 255); break;
    case 'Y': s->_color = Color(255, 255, 0); break;
    case 'V': s->_color = Color(255, 0, 255); break;
    default: s->_color = Color(20, 20, 20); break;
    // case 'b': _color = Color(20, 20, 20); break;
    }
}

Mag_server::Session::~Session()
{
  if (_label)
    delete [] _label;

  if (cxx::D_list_cyclic<Session>::in_list(this))
    cxx::D_list_cyclic<Session>::remove(this);

  if (_ntfy)
    _ntfy->notify();
}

