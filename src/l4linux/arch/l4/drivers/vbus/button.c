/*
 *  button.c - vbus Button Driver
 *
 *  Inspired by drivers/acpi/button.c
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <asm/generic/vbus.h>

#define PREFIX "L4vbus: "

enum {
	VBUS_BUTTON_NOTIFY_STATUS = 0x80
};

#define VBUS_BUTTON_HID_POWER		"PNP0C0C"
#define ACPI_BUTTON_DEVICE_NAME_POWER	"Power Button"

#define VBUS_BUTTON_HID_SLEEP		"PNP0C0E"

#define VBUS_BUTTON_HID_LID		"PNP0C0D"
#define ACPI_BUTTON_DEVICE_NAME_LID	"Lid Switch"
enum {
	VBUS_BUTTON_TYPE_POWER		= 0x01,
	VBUS_BUTTON_TYPE_SLEEP		= 0x03,
	VBUS_BUTTON_TYPE_LID		= 0x05
};

MODULE_AUTHOR("Alexander Warg");
MODULE_DESCRIPTION("l4vbus Button Driver");
MODULE_LICENSE("GPL");

static const struct l4x_vbus_device_id button_device_ids[] = {
	{ VBUS_BUTTON_HID_LID },
	{ VBUS_BUTTON_HID_SLEEP },
	{ VBUS_BUTTON_HID_POWER },
	{ NULL },
};
MODULE_DEVICE_TABLE(vbus, button_device_ids);

static int vbus_button_add(struct l4x_vbus_device *device);
static int vbus_button_remove(struct l4x_vbus_device *device);
static void vbus_button_notify(struct l4x_vbus_device *device, unsigned type,
                               unsigned code, unsigned value);

#ifdef CONFIG_PM_SLEEP
static int vbus_button_resume(struct device *dev);
#endif
static SIMPLE_DEV_PM_OPS(vbus_button_pm, NULL, vbus_button_resume);

static struct l4x_vbus_driver l4x_vbus_button_driver = {
	.driver.name = "button",
	.id_table = button_device_ids,
	.ops = {
		.add = vbus_button_add,
		.remove = vbus_button_remove,
		.notify = vbus_button_notify,
	},
	.driver.pm = &vbus_button_pm,
};

struct vbus_button {
	unsigned int type;
	struct input_dev *input;
	char phys[32];			/* for input device */
	unsigned long pushed;
	bool wakeup_enabled;
};

static BLOCKING_NOTIFIER_HEAD(vbus_lid_notifier);
static struct l4x_vbus_device *lid_device;


static int vbus_lid_send_state(struct l4x_vbus_device *device)
{
#if 0
	struct vbus_button *button = device->driver_data;
	unsigned long long state;
	int ret;

	status = acpi_evaluate_integer(device->handle, "_LID", NULL, &state);
	if (ACPI_FAILURE(status))
		return -ENODEV;

	/* input layer checks if event is redundant */
	input_report_switch(button->input, SW_LID, !state);
	input_sync(button->input);

	if (state)
		pm_wakeup_event(&device->dev, 0);

	ret = blocking_notifier_call_chain(&acpi_lid_notifier, state, device);
	if (ret == NOTIFY_DONE)
		ret = blocking_notifier_call_chain(&acpi_lid_notifier, state,
		                                   device);
	if (ret == NOTIFY_DONE || ret == NOTIFY_OK) {
		/*
		 * It is also regarded as success if the notifier_chain
		 * returns NOTIFY_OK or NOTIFY_DONE.
		 */
		ret = 0;
	}
	return ret;
#endif
	return 0;
}

static void vbus_button_notify(struct l4x_vbus_device *device, unsigned type,
                               unsigned code, unsigned value)
{
	struct vbus_button *button = device->driver_data;
	struct input_dev *input;

	switch (code) {
	case VBUS_BUTTON_NOTIFY_STATUS:
		input = button->input;
		if (button->type == VBUS_BUTTON_TYPE_LID) {
			vbus_lid_send_state(device);
		} else {
			int keycode = test_bit(KEY_SLEEP, input->keybit) ?
			                       KEY_SLEEP : KEY_POWER;

			input_report_key(input, keycode, 1);
			input_sync(input);
			input_report_key(input, keycode, 0);
			input_sync(input);

			pm_wakeup_event(&device->dev, 0);
		}
		break;
	default:
		pr_info("Unsupported event [0x%x]\n", code);
		break;
	}
}

#ifdef CONFIG_PM_SLEEP
static int vbus_button_resume(struct device *dev)
{
	struct l4x_vbus_device *device = l4x_vbus_device_from_device(dev);
	struct vbus_button *button = device->driver_data;

	if (button->type == VBUS_BUTTON_TYPE_LID)
		return vbus_lid_send_state(device);
	return 0;
}
#endif

static int vbus_button_add(struct l4x_vbus_device *device)
{
	struct vbus_button *button;
	struct input_dev *input;
	const char *hid = device->hid;
	int error;

	button = kzalloc(sizeof(struct vbus_button), GFP_KERNEL);
	if (!button)
		return -ENOMEM;

	device->driver_data = button;

	button->input = input = input_allocate_device();
	if (!input) {
		error = -ENOMEM;
		goto err_free_button;
	}

	if (!strcmp(hid, VBUS_BUTTON_HID_POWER)) {
		button->type = VBUS_BUTTON_TYPE_POWER;
	} else if (!strcmp(hid, VBUS_BUTTON_HID_SLEEP)) {
		button->type = VBUS_BUTTON_TYPE_SLEEP;
	} else if (!strcmp(hid, VBUS_BUTTON_HID_LID)) {
		button->type = VBUS_BUTTON_TYPE_LID;
	} else {
		pr_err(PREFIX "Unsupported hid [%s]\n", hid);
		error = -ENODEV;
		goto err_free_input;
	}

	snprintf(button->phys, sizeof(button->phys), "%s/button/input0", hid);

	input->name = dev_name(&device->dev);
	input->phys = button->phys;
	input->id.bustype = BUS_HOST;
	input->id.product = button->type;
	input->dev.parent = &device->dev;

	switch (button->type) {
	case VBUS_BUTTON_TYPE_POWER:
		input->evbit[0] = BIT_MASK(EV_KEY);
		set_bit(KEY_POWER, input->keybit);
		break;

	case VBUS_BUTTON_TYPE_SLEEP:
		input->evbit[0] = BIT_MASK(EV_KEY);
		set_bit(KEY_SLEEP, input->keybit);
		break;

	case VBUS_BUTTON_TYPE_LID:
		input->evbit[0] = BIT_MASK(EV_SW);
		set_bit(SW_LID, input->swbit);
		break;
	}

	error = input_register_device(input);
	if (error)
		goto err_free_input;
	if (button->type == VBUS_BUTTON_TYPE_LID) {
		vbus_lid_send_state(device);
		/*
		 * This assumes there's only one lid device, or if there are
		 * more we only care about the last one...
		 */
		lid_device = device;
	}
#if 0
	if (device->wakeup.flags.valid) {
		/* Button's GPE is run-wake GPE */
		acpi_enable_gpe(device->wakeup.gpe_device,
				device->wakeup.gpe_number);
		if (!device_may_wakeup(&device->dev)) {
			device_set_wakeup_enable(&device->dev, true);
			button->wakeup_enabled = true;
		}
	}
#endif
	pr_info(PREFIX "%s\n", dev_name(&device->dev));
	return 0;

 err_free_input:
	input_free_device(input);
 err_free_button:
	kfree(button);
	return error;
}

static int vbus_button_remove(struct l4x_vbus_device *device)
{
	struct vbus_button *button = device->driver_data;
#if 0
	if (device->wakeup.flags.valid) {
		acpi_disable_gpe(device->wakeup.gpe_device,
				device->wakeup.gpe_number);
		if (button->wakeup_enabled)
			device_set_wakeup_enable(&device->dev, false);
	}
#endif
	input_unregister_device(button->input);
	kfree(button);
	return 0;
}

module_l4x_vbus_driver(l4x_vbus_button_driver);
