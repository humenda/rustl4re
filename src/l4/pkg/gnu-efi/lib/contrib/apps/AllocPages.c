
/*
 * Copyright (C) 2013 Jerry Hoemann <jerry.hoemann@hp.com>
 *
 *
 * Application to allocate memory at EFI.  Syntax of command
 * mimics the EFI Boot Service "AllocatePages."
 *
 * See UEFI spec 2.3, Section 6.2.
 *
 *




FS1:\> memmap
Type      Start            End              #pages             Attributes
BS_Code   0000000000000000-0000000000000FFF 0000000000000001 000000000000000F
Available 0000000000001000-000000000008DFFF 000000000000008D 000000000000000F
Reserved  000000000008E000-000000000008FFFF 0000000000000002 000000000000000F
Available 0000000000090000-000000000009FFFF 0000000000000010 000000000000000F
Available 0000000000100000-000000000FFFFFFF 000000000000FF00 000000000000000F
BS_Code   0000000010000000-0000000010061FFF 0000000000000062 000000000000000F
Available 0000000010062000-000000005CDFFFFF 000000000004CD9E 000000000000000F
ACPI_NVS  000000005CE00000-000000005DDFFFFF 0000000000001000 000000000000000F
BS_Data   000000005DE00000-000000005DFFFFFF 0000000000000200 000000000000000F
Available 000000005E000000-000000005EF1CFFF 0000000000000F1D 000000000000000F
BS_Data   000000005EF1D000-00000000709FBFFF 0000000000011ADF 000000000000000F
Available 00000000709FC000-00000000710E3FFF 00000000000006E8 000000000000000F
LoaderCode 00000000710E4000-00000000711FEFFF 000000000000011B 000000000000000F
Available 00000000711FF000-0000000071901FFF 0000000000000703 000000000000000F
BS_Code   0000000071902000-00000000721FEFFF 00000000000008FD 000000000000000F


Example to allocat 5 pages type BootCode at address 20000000 (hex)


FS1:\> AllocPages.efi 2 3 5 20000000
AllocatePage: __AllocType__ __MemType__ __NumPages__ [__Addr__]
__AllocType__ {0,1,2} -- Any, MaxAddr, Addr
__MemType__   {0..13}, Reserved ==0, LCode==1, LData==2, BSCode==3, BSData==4, ...
__NumPages__  {0..F000000}
[__Addr__]     0... 3FFFFFFFFFFF
All numbers in hex no leading 0x

AllocatPage(2,3,5,20000000)


Example to allocat 5 pages type BootCode at address 30000000 (hex)


FS1:\> AllocPages.efi 2 3 5 30000000
AllocatePage: __AllocType__ __MemType__ __NumPages__ [__Addr__]
__AllocType__ {0,1,2} -- Any, MaxAddr, Addr
__MemType__   {0..13}, Reserved ==0, LCode==1, LData==2, BSCode==3, BSData==4, ...
__NumPages__  {0..F000000}
[__Addr__]     0... 3FFFFFFFFFFF
All numbers in hex no leading 0x



FS1:\> memmap
Type      Start            End              #pages             Attributes
BS_Code   0000000000000000-0000000000000FFF 0000000000000001 000000000000000F
Available 0000000000001000-000000000008DFFF 000000000000008D 000000000000000F
Reserved  000000000008E000-000000000008FFFF 0000000000000002 000000000000000F
Available 0000000000090000-000000000009FFFF 0000000000000010 000000000000000F
Available 0000000000100000-000000000FFFFFFF 000000000000FF00 000000000000000F
BS_Code   0000000010000000-0000000010061FFF 0000000000000062 000000000000000F
Available 0000000010062000-000000001FFFFFFF 000000000000FF9E 000000000000000F
BS_Code   0000000020000000-0000000020004FFF 0000000000000005 000000000000000F
Available 0000000020005000-000000002FFFFFFF 000000000000FFFB 000000000000000F
BS_Code   0000000030000000-0000000030004FFF 0000000000000005 000000000000000F
Available 0000000030005000-000000005CDFFFFF 000000000002CDFB 000000000000000F
ACPI_NVS  000000005CE00000-000000005DDFFFFF 0000000000001000 000000000000000F
BS_Data   000000005DE00000-000000005DFFFFFF 0000000000000200 000000000000000F
Available 000000005E000000-000000005EF1CFFF 0000000000000F1D 000000000000000F
BS_Data   000000005EF1D000-00000000709FBFFF 0000000000011ADF 000000000000000F
Available 00000000709FC000-00000000710E3FFF 00000000000006E8 000000000000000F
LoaderCode 00000000710E4000-00000000711FEFFF 000000000000011B 000000000000000F
Available 00000000711FF000-0000000071901FFF 0000000000000703 000000000000000F
BS_Code   0000000071902000-00000000721FEFFF 00000000000008FD 000000000000000F





 */

#include <efi.h>
#include <efilib.h>


#define MAX_NUM_PAGES 0x000000000F000000
#define MAX_ADDR ((1ULL << 46) - 1)

#define	MAX_ARGS 256

#define CHAR_SPACE L' '

#define DEBUG 0

INTN
argify(CHAR16 *buf, UINTN len, CHAR16 **argv)   
{

        UINTN     i=0, j=0;
        CHAR16   *p = buf;
	
        if (buf == 0) { 
		argv[0] = NULL;
		return 0;
	}
	/* len represents the number of bytes, not the number of 16 bytes chars */
	len = len >> 1;

	/*
	 * Here we use CHAR_NULL as the terminator rather than the length
	 * because it seems like the EFI shell return rather bogus values for it.
	 * Apparently, we are guaranteed to find the '\0' character in the buffer
	 * where the real input arguments stop, so we use it instead.
	 */
	for(;;) {
		while (buf[i] == CHAR_SPACE && buf[i] != CHAR_NULL && i < len) i++;

		if (buf[i] == CHAR_NULL || i == len) goto end;

		p = buf+i;
		i++;

		while (buf[i] != CHAR_SPACE && buf[i] != CHAR_NULL && i < len) i++;

		argv[j++] = p;

		if (buf[i] == CHAR_NULL) goto end;

		buf[i]  = CHAR_NULL;

		if (i == len)  goto end;

		i++;

		if (j == MAX_ARGS-1) {
			Print(L"too many arguments (%d) truncating\n", j);
			goto end;
		}
	}
end:
        argv[j] = NULL;
	return j;
}


EFI_STATUS
efi_main (EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{

	EFI_STATUS efi_status;
	EFI_GUID LoadedImageProtocol = LOADED_IMAGE_PROTOCOL;
	EFI_LOADED_IMAGE *info;

	CHAR16 arglist[MAX_ARGS+1] = {0};
	CHAR16 *argv[MAX_ARGS];
	INTN argc = 0;
	INTN err = 0;

	INTN AllocType = -1;
	INTN MemType = -1;
	INTN NumPages = -1;
	UINTN Addr = 0;

	InitializeLib(image, systab);

        efi_status = uefi_call_wrapper( BS->HandleProtocol, 3, image,
                &LoadedImageProtocol, &info);


	Print(L"AllocatePage: __AllocType__ __MemType__ __NumPages__ [__Addr__]\n");
	Print(L"__AllocType__ {0,1,2} -- Any, MaxAddr, Addr\n"); 
	Print(L"__MemType__   {0..13}, Reserved ==0, LCode==1, LData==2, BSCode==3, BSData==4, ...\n");
	Print(L"__NumPages__  {0..%x}\n", MAX_NUM_PAGES);
	Print(L"[__Addr__]     0... %llx\n", MAX_ADDR);
	Print(L"All numbers in hex no leading 0x\n");
	Print(L"\n");

#if DEBUG
	Print(L"%s\n", info->LoadOptions);
#endif


#if DEBUG
	Print(L"Set up arglist\n");
#endif
	CopyMem(arglist, info->LoadOptions, info->LoadOptionsSize);
#if DEBUG
	Print(L"arglist = <%s>\n", arglist);
#endif
	
#if DEBUG
	Print(L"Now try argify\n");
#endif
	argc = argify(arglist, info->LoadOptionsSize, argv);
#if DEBUG
	Print(L"argc = %d\n", argc);
#endif

#if DEBUG
	for (c = 0;  c < argc;  c++ ) {
		Print(L"argv[%d] = <%s>\n", c, argv[c]);
	}
#endif
	if ( (argc < 3) || (argc > 5) ) {
		Print(L"Wrong argument count\n");
		return EFI_SUCCESS;
	}

	AllocType = xtoi(argv[1]);
	MemType   = xtoi(argv[2]);
	NumPages  = xtoi(argv[3]);
	if ( argc == 5 ) Addr = xtoi(argv[4]);

	if ( (AllocType < 0) || (AllocType > 2)) {
		Print(L"Invalid AllocType\n");
		err++;
	}
	if ( (MemType < 0) || (MemType > 13) ) {
		Print(L"Invalid MemType\n");
		err++;
	}
	if ( (NumPages < 0) || (NumPages > MAX_NUM_PAGES) ) {
		Print(L"Inavlid NumPages\n");
		err++;
	}
	if ( Addr > MAX_ADDR ) {
		Print(L"Inavlid Address\n");
		err++;
	}
	if ( err ) {
		return EFI_INVALID_PARAMETER;
	}

	Print(L"AllocatPage(%d,%d,%d,%lx)\n", AllocType, MemType, NumPages, Addr);

	efi_status = uefi_call_wrapper(BS->AllocatePages, 4, AllocType, MemType, NumPages, &Addr);

	if ( EFI_ERROR(efi_status) ) {
		Print(L"Allocate Pages Failed: %d\n", efi_status);
		return efi_status;
	}

	return EFI_SUCCESS;
}
