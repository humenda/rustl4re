#pragma once

#include "resource.h"

namespace Hw {
  class Msi_resource;
}

namespace Vi {
  class Msi_resource : public Resource
  {
  private:
    Hw::Msi_resource *_hw_msi;

  public:
    Msi_resource(Hw::Msi_resource *hr);
    Hw::Msi_resource *hw_msi() const { return _hw_msi; }
  };

}
