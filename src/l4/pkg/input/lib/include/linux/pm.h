#ifndef _LINUX_PM_H
#define _LINUX_PM_H

/*
 * Power management requests
 */
enum
{
	PM_SUSPEND, /* enter D1-D3 */
	PM_RESUME,  /* enter D0 */

	PM_SAVE_STATE,  /* save device's state */

	/* enable wake-on */
	PM_SET_WAKEUP,

	/* bus resource management */
	PM_GET_RESOURCES,
	PM_SET_RESOURCES,

	/* base station management */
	PM_EJECT,
	PM_LOCK,
};

typedef int pm_request_t;

/*
 * Device types
 */
enum
{
	PM_UNKNOWN_DEV = 0, /* generic */
	PM_SYS_DEV,	    /* system device (fan, KB controller, ...) */
	PM_PCI_DEV,	    /* PCI device */
	PM_USB_DEV,	    /* USB device */
	PM_SCSI_DEV,	    /* SCSI device */
	PM_ISA_DEV,	    /* ISA device */
	PM_MTD_DEV,	    /* Memory Technology Device */
};

typedef int pm_dev_t;

/*
 * System device hardware ID (PnP) values
 */
enum
{
	PM_SYS_UNKNOWN = 0x00000000, /* generic */
	PM_SYS_KBC =	 0x41d00303, /* keyboard controller */
	PM_SYS_COM =	 0x41d00500, /* serial port */
	PM_SYS_IRDA =	 0x41d00510, /* IRDA controller */
	PM_SYS_FDC =	 0x41d00700, /* floppy controller */
	PM_SYS_VGA =	 0x41d00900, /* VGA controller */
	PM_SYS_PCMCIA =	 0x41d00e00, /* PCMCIA controller */
};

struct pm_dev {
	void *data;
};

typedef int (*pm_callback)(struct pm_dev *dev, pm_request_t rqst, void *data);

static inline struct pm_dev *pm_register(pm_dev_t type, unsigned long id,
                                         pm_callback callback)
{
	return 0;
}

static inline void pm_unregister(struct pm_dev *dev) {}
static inline void pm_access(struct pm_dev *dev) {}
static inline void pm_dev_idle(struct pm_dev *dev) {}

#endif

