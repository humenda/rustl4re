/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <l4/sys/compiler.h>

class Pci_survey_config;

class Acpi_config
{
public:
  virtual Pci_survey_config *pci_survey_config() = 0;
  virtual ~Acpi_config() = 0;
};

inline Acpi_config::~Acpi_config() {}

struct acpica_pci_irq
{
  unsigned int irq;
  unsigned char trigger;
  unsigned char polarity;
};

int acpica_init();
int acpi_ecdt_scan();

