#include <asm/generic/l4lib.h>

#include <l4/stpm/stpmif.h>
#include <l4/stpm/encap.h>
#include <l4/log/l4log.h>

#include "tpm.h"

#define TPM_DEVICE_MINOR  224
#define TPM_DEVICE_NAME   "tpm_emu"
#define TPM_MODULE_NAME   "stpm_dev"

#define STPM_BUF_SIZE 4096

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alexander Boettcher <boettcher@tudos.org>");
MODULE_DESCRIPTION("Device driver for the L4 Secure Trusted Platform Module (STPM) interface. Either virtual TPM service (tpmemu) or a hardware TPM service (stpm) can be accessed, depending on the module parameter (vtpm=...). Default is stpm.");
MODULE_SUPPORTED_DEVICE(TPM_DEVICE_NAME);

L4_EXTERNAL_FUNC(stpm_transmit);
L4_EXTERNAL_FUNC(stpm_check_server);

static char * l4x_vtpm_instance = NULL;

static struct stpm_data {
	char * read;
	int size;
} stpm_data;

static u8 tpm_emu_status(struct tpm_chip *chip)
{
	return 0;
}

static int tpm_emu_send(struct tpm_chip *chip, u8 *buf, size_t len)
{
	stpm_data.size = STPM_BUF_SIZE;
	return stpm_transmit(buf, len, &stpm_data.read, &stpm_data.size);
}

static int tpm_emu_recv(struct tpm_chip *chip, u8 *buf, size_t count)
{
	if (stpm_data.size < count) {
		int size = stpm_data.size;
		memcpy(buf, stpm_data.read, size);
		stpm_data.size = 0;
		return size;
	}

	return -EIO;
}

static void tpm_emu_ready(struct tpm_chip *chip)
{
	printk(KERN_WARNING "tpm_emu_ready not implemented\n");
	LOG_printf("tpm_emu_ready not implemented");
}

static struct platform_device *pdev;

static DEVICE_ATTR(pubek, S_IRUGO, tpm_show_pubek, NULL);
static DEVICE_ATTR(pcrs, S_IRUGO, tpm_show_pcrs, NULL);
static DEVICE_ATTR(enabled, S_IRUGO, tpm_show_enabled, NULL);
static DEVICE_ATTR(active, S_IRUGO, tpm_show_active, NULL);
static DEVICE_ATTR(owned, S_IRUGO, tpm_show_owned, NULL);
static DEVICE_ATTR(temp_deactivated, S_IRUGO, tpm_show_temp_deactivated,
		   NULL);
static DEVICE_ATTR(caps, S_IRUGO, tpm_show_caps_1_2, NULL);
static DEVICE_ATTR(cancel, S_IWUSR | S_IWGRP, NULL, tpm_store_cancel);

static struct attribute *emu_attrs[] = {
	&dev_attr_pubek.attr,
	&dev_attr_pcrs.attr,
	&dev_attr_enabled.attr,
	&dev_attr_active.attr,
	&dev_attr_owned.attr,
	&dev_attr_temp_deactivated.attr,
	&dev_attr_caps.attr,
	&dev_attr_cancel.attr, NULL,
};

static struct attribute_group emu_attr_grp = { .attrs = emu_attrs };

static const struct file_operations emu_ops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.open = tpm_open,
	.read = tpm_read,
	.write = tpm_write,
	.release = tpm_release,
};

static struct device_driver emu_drv = {
	.name = TPM_DEVICE_NAME,
	.bus = &platform_bus_type,
	.owner = THIS_MODULE,
	.suspend = 0, //tpm_pm_suspend,
	.resume = 0, //tpm_pm_resume,
};

static struct tpm_vendor_specific tpm_emu = {
	.status = tpm_emu_status,
	.recv = tpm_emu_recv,
	.send = tpm_emu_send,
	.cancel = tpm_emu_ready,
	.req_complete_mask = 0,
	.req_complete_val = 0,
	.req_canceled = 0,
	.attr_group = &emu_attr_grp,
	.miscdev = { .fops = &emu_ops,},
};

int __init init_tpm_module(void)
{
	int rc, error;
	struct tpm_chip *chip;

	/* only do anything if service name was specified */
	if (!l4x_vtpm_instance)
		return -ENODEV;

	rc = driver_register(&emu_drv);
	if (rc < 0)
		return rc;

	if (IS_ERR(pdev = platform_device_register_simple(TPM_DEVICE_NAME,
	                                                  -1, NULL, 0))) {
		rc = PTR_ERR(pdev);
		goto out1;
	}

	rc = -ENODEV;
	if (!(chip = tpm_register_hardware(&pdev->dev, &tpm_emu)))
		goto out2;

	rc = -ENOMEM;
	stpm_data.read = kmalloc(STPM_BUF_SIZE, GFP_KERNEL);
	if (!stpm_data.read)
		goto out3;

	stpm_data.size = STPM_BUF_SIZE;

	/* connect to service */
	error = stpm_check_server(l4x_vtpm_instance, 1);

	if (error) {
		printk(KERN_ERR "TPM %s not found\n", l4x_vtpm_instance);
		LOG_printf("TPM %s not found", l4x_vtpm_instance);
		rc = -ENODEV;
		goto out4;
	}

	return 0;

out4:
	kfree(stpm_data.read);
out3:
	tpm_remove_hardware(&pdev->dev);
out2:
	platform_device_unregister(pdev);
out1:
	driver_unregister(&emu_drv);
	return rc;
}

void __exit cleanup_tpm_module(void)
{
	kfree(stpm_data.read);
	stpm_data.read = NULL;
	stpm_data.size = 0;

	tpm_remove_hardware(&pdev->dev);
	platform_device_unregister(pdev);
	driver_unregister(&emu_drv);
}

module_param_named(vtpm, l4x_vtpm_instance, charp, 0);
MODULE_PARM_DESC(vtpm, "Name of vTPM to use");

module_init(init_tpm_module);
module_exit(cleanup_tpm_module);
