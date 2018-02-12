#include "common.h"
#include "nand.h"

/*
*	Chip ID list
*
*	Id, name, page size, spare size, chip size in MegaByte, erase block size, options
*/
struct Dev_desc _dev_ids[] =
{
    {0x6e, "NAND 1MiB 5V 8-bit",	256, 0, 1, 0x1000, 0},
    {0x64, "NAND 2MiB 5V 8-bit",	256, 0, 2, 0x1000, 0},
    {0x6b, "NAND 4MiB 5V 8-bit",	512, 0, 4, 0x2000, 0},
    {0xe8, "NAND 1MiB 3,3V 8-bit",	256, 0, 1, 0x1000, 0},
    {0xec, "NAND 1MiB 3,3V 8-bit",	256, 0, 1, 0x1000, 0},
    {0xea, "NAND 2MiB 3,3V 8-bit",	256, 0, 2, 0x1000, 0},
    {0xd5, "NAND 4MiB 3,3V 8-bit", 	512, 0, 4, 0x2000, 0},
    {0xe3, "NAND 4MiB 3,3V 8-bit",	512, 0, 4, 0x2000, 0},
    {0xe5, "NAND 4MiB 3,3V 8-bit",	512, 0, 4, 0x2000, 0},
    {0xd6, "NAND 8MiB 3,3V 8-bit",	512, 0, 8, 0x2000, 0},

    {0x39, "NAND 8MiB 1,8V 8-bit",	512, 0, 8, 0x2000, 0},
    {0xe6, "NAND 8MiB 3,3V 8-bit",	512, 0, 8, 0x2000, 0},
    {0x49, "NAND 8MiB 1,8V 16-bit",	512, 0, 8, 0x2000, Opt_buswidth_16},
    {0x59, "NAND 8MiB 3,3V 16-bit",	512, 0, 8, 0x2000, Opt_buswidth_16},

    {0x33, "NAND 16MiB 1,8V 8-bit",	512, 0, 16, 0x4000, 0},
    {0x73, "NAND 16MiB 3,3V 8-bit",	512, 0, 16, 0x4000, 0},
    {0x43, "NAND 16MiB 1,8V 16-bit",	512, 0, 16, 0x4000, Opt_buswidth_16},
    {0x53, "NAND 16MiB 3,3V 16-bit",	512, 0, 16, 0x4000, Opt_buswidth_16},

    {0x35, "NAND 32MiB 1,8V 8-bit",	512, 0, 32, 0x4000, 0},
    {0x75, "NAND 32MiB 3,3V 8-bit",	512, 0, 32, 0x4000, 0},
    {0x45, "NAND 32MiB 1,8V 16-bit",	512, 0, 32, 0x4000, Opt_buswidth_16},
    {0x55, "NAND 32MiB 3,3V 16-bit",	512, 0, 32, 0x4000, Opt_buswidth_16},

    {0x36, "NAND 64MiB 1,8V 8-bit",	512, 0, 64, 0x4000, 0},
    {0x76, "NAND 64MiB 3,3V 8-bit",	512, 0, 64, 0x4000, 0},
    {0x46, "NAND 64MiB 1,8V 16-bit",	512, 0, 64, 0x4000, Opt_buswidth_16},
    {0x56, "NAND 64MiB 3,3V 16-bit",	512, 0, 64, 0x4000, Opt_buswidth_16},

    {0x78, "NAND 128MiB 1,8V 8-bit",	512, 0, 128, 0x4000, 0},
    {0x39, "NAND 128MiB 1,8V 8-bit",	512, 0, 128, 0x4000, 0},
    {0x79, "NAND 128MiB 3,3V 8-bit",	512, 0, 128, 0x4000, 0},
    {0x72, "NAND 128MiB 1,8V 16-bit",	512, 0, 128, 0x4000, Opt_buswidth_16},
    {0x49, "NAND 128MiB 1,8V 16-bit",	512, 0, 128, 0x4000, Opt_buswidth_16},
    {0x74, "NAND 128MiB 3,3V 16-bit",	512, 0, 128, 0x4000, Opt_buswidth_16},
    {0x59, "NAND 128MiB 3,3V 16-bit",	512, 0, 128, 0x4000, Opt_buswidth_16},

    {0x71, "NAND 256MiB 3,3V 8-bit",	512, 0, 256, 0x4000, 0},

    /*
     * These are the new chips with large page size. The pagesize and the
     * erasesize is determined from the extended id bytes
     */
#define LP_OPTIONS (Opt_no_readrdy | Opt_no_autoincr)
#define LP_OPTIONS16 (LP_OPTIONS | Opt_buswidth_16)

    /*512 Megabit */
    {0xA2, "NAND 64MiB 1,8V 8-bit",	0, 0,  64, 0, LP_OPTIONS},
    {0xF2, "NAND 64MiB 3,3V 8-bit",	0, 0,  64, 0, LP_OPTIONS},
    {0xB2, "NAND 64MiB 1,8V 16-bit",	0, 0,  64, 0, LP_OPTIONS16},
    {0xC2, "NAND 64MiB 3,3V 16-bit",	0, 0,  64, 0, LP_OPTIONS16},

    /* 1 Gigabit */
    {0xA1, "NAND 128MiB 1,8V 8-bit",	0, 0, 128, 0, LP_OPTIONS},
    {0xF1, "NAND 128MiB 3,3V 8-bit",	0, 0, 128, 0, LP_OPTIONS},
    {0xB1, "NAND 128MiB 1,8V 16-bit",	0, 0, 128, 0, LP_OPTIONS16},
    {0xC1, "NAND 128MiB 3,3V 16-bit",	0, 0, 128, 0, LP_OPTIONS16},

    /* 2 Gigabit */
    {0xAA, "NAND 256MiB 1,8V 8-bit",	0, 0, 256, 0, LP_OPTIONS},
    {0xDA, "NAND 256MiB 3,3V 8-bit",	0, 0, 256, 0, LP_OPTIONS},
    {0xBA, "NAND 256MiB 1,8V 16-bit",	0, 0, 256, 0, LP_OPTIONS16},
    {0xCA, "NAND 256MiB 3,3V 16-bit",	0, 0, 256, 0, LP_OPTIONS16},

    /* 4 Gigabit */
    {0xAC, "NAND 512MiB 1,8V 8-bit",	0, 0, 512, 0, LP_OPTIONS},
    {0xDC, "NAND 512MiB 3,3V 8-bit",	0, 0, 512, 0, LP_OPTIONS},
    {0xBC, "NAND 512MiB 1,8V 16-bit",	0, 0, 512, 0, LP_OPTIONS16},
    {0xCC, "NAND 512MiB 3,3V 16-bit",	0, 0, 512, 0, LP_OPTIONS16},

    /* 8 Gigabit */
    {0xA3, "NAND 1GiB 1,8V 8-bit",	0, 0, 1024, 0, LP_OPTIONS},
    {0xD3, "NAND 1GiB 3,3V 8-bit",	0, 0, 1024, 0, LP_OPTIONS},
    {0xB3, "NAND 1GiB 1,8V 16-bit",	0, 0, 1024, 0, LP_OPTIONS16},
    {0xC3, "NAND 1GiB 3,3V 16-bit",	0, 0, 1024, 0, LP_OPTIONS16},

    /* 16 Gigabit */
    {0xA5, "NAND 2GiB 1,8V 8-bit",	0, 0, 2048, 0, LP_OPTIONS},
    {0xD5, "NAND 2GiB 3,3V 8-bit",	0, 0, 2048, 0, LP_OPTIONS},
    {0xB5, "NAND 2GiB 1,8V 16-bit",	0, 0, 2048, 0, LP_OPTIONS16},
    {0xC5, "NAND 2GiB 3,3V 16-bit",	0, 0, 2048, 0, LP_OPTIONS16},

    {0xD7, "NAND 4GiB ?V 16-bit",	4096, 218, 4096, 0x80000, LP_OPTIONS16},

    {0, 0, 0, 0, 0, 0, 0}
};

/*
*	Manufacturer ID list
*/
struct Mfr_desc _mfr_ids[] =
{
    {0x01, "AMD"},
    {0x04, "Fujitsu"},
    {0x07, "Renesas"},
    {0x20, "ST Micro"},
    {0x2c, "Micron"},
    {0x84, "National"},
    {0x98, "Toshiba"},
    {0xad, "Hynix"},
    {0xec, "Samsung"},
    {0x00, "Unknown"},
};
