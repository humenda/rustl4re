#pragma once

#include <l4/scout-gfx/factory>
#include <l4/mag-gfx/geometry>

#include "browser.h"

namespace Scout_gfx {
class Parent_widget;
}

class Platform;

class Factory : public virtual Scout_gfx::Factory
{
public:
  virtual Scout_gfx::Parent_widget *create_panel(Scout_browser *) = 0;
};
