// controller numbering is static ie depends on location
// else the device numbers are dynamically allocated.
// works only for hard disks
#define ATA_STATIC_ID

// not used in source code
//#define ATA_NOPCI

// support ata disk drives
#define DEV_ATADISK

// support atapi cdrom drives
#define DEV_ATAPICD

// support atapi tape drives
//#define DEV_ATAPIST

// support atapi floppy drives
#define DEV_ATAPIFD

// support atapi cam
//#define DEV_ATAPICAM

// support ata raid
#define DEV_ATARAID

