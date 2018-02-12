#pragma once

#include "irqs.h"

namespace Hw { namespace Irqs {
Io_irq_pin *real_irq(unsigned irqnum);
}}
