/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2014-2020 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 */

extern "C" {
#include <efi.h>
#include <efilib.h>
}

#include "support.h"
#include "startup.h"

extern "C" void armv8_disable_mmu(void);

extern "C" EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab);
EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
  ctor_init();

  // Bye bye UEFI
  UINTN num_entries;
  UINTN key;
  UINTN desc_size;
  uint32_t desc_ver;
  InitializeLib(image, systab);
  LibMemoryMap(&num_entries, &key, &desc_size, &desc_ver);
  uefi_call_wrapper(systab->BootServices->ExitBootServices, 2, image, key);

  // UEFI has interrupts enabled as per specification. Fiasco boot protocol
  // requires interrupts to be disabled, though.
  asm volatile("msr DAIFSet, #3");

  // UEFI did setup the MMU with caches. Disable everything because we move
  // code around and will jump to it at the end.
  armv8_disable_mmu();

  // Do the usual platform iteration as done by head.cc.
  Platform_base::iterate_platforms();
  init_modules_infos();
  startup(mod_info_mbi_cmdline(mod_header));

  return EFI_SUCCESS;
}
