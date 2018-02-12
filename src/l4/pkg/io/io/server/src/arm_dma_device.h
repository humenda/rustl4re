#pragma once

#include "arm_dma_domain.h"
#include "hw_device.h"
#include "main.h"

namespace Hw {
  /**
   * Hw::Device with specific properties for ARM IOMMUs.
   *
   * This class offers the following new properties for devices:
   *   - iommu   ID of the IOMMU
   *   - sidreg  register space for stream ID configuration (SMMU-400 only)
   *   - offset  offset into `sidreg` (SMMU-400 only)
   *   - utlb    micro-TLB the device is connected to (IPMMU only)
   *
   * FIXME: Using an extra class for this seems like an unsatisfactory solution.
   */
  class Arm_dma_device : public Device
  {
  public:
    Arm_dma_device(l4_umword_t uid, l4_uint32_t adr) : Device(uid, adr)
    { setup(); }

    Arm_dma_device(l4_uint32_t adr) : Device(adr)
    { setup(); }

    Arm_dma_device() : Device()
    { setup(); }

  private:
    void setup()
    {
      register_arm_specific_properties();
      property("flags")->set(-1, DF_dma_supported);
    }

    void register_arm_specific_properties()
    {
      register_property("iommu", &_iommu);
      register_property("sidreg", &_sidreg);
      register_property("offset", &_offset);
      register_property("utlb", &_utlb);
    }

    Device_property<Hw::Device> _sidreg;
    Int_property _offset;
    Int_property _iommu;
    Int_property _utlb;
  };
}
