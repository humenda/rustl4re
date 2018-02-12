#include "acpi.h"
#include "acpi_l4.h"

#include <stdio.h>
static char const *const acpi_sys_modes[] =
  {"UNKNOWN MODE", "ACPI MODE", "LEGACY MODE"};

void
acpi_print_system_info(void * sys_info_p)
{
  ACPI_SYSTEM_INFO *info = (ACPI_SYSTEM_INFO*)sys_info_p;

  printf("ACPICA-Version:%x, System in %s, %ibit timer\n",
         info->AcpiCaVersion,
         acpi_sys_modes[info->Flags],
         info->TimerResolution);
}
