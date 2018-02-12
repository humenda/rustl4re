#include <stdint.h>
#include <efi.h>
#include <efilib.h>

#ifdef __cplusplus
extern "C"
{
  EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab);
}
#endif

EFI_STATUS
efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
  EFI_STATUS efi_status = EFI_SUCCESS;

  InitializeLib(image, systab);

  CHAR16 s[] = L"Hello World from L4\n";
  Print(s);

  return efi_status;
}
