/*
 * fsa9480.c - FSA9480 micro USB switch device driver
 *
 * Copyright (C) 2010 Samsung Electronics
 * Minkyu Kang <mk7.kang@samsung.com>
 * Wonguk Jeong <wonguk.jeong@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/platform_data/fsa9480.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/switch.h>

/* FSA9480 I2C registers */
#define FSA9480_REG_DEVID		0x01
#define FSA9480_REG_CTRL		0x02
#define FSA9480_REG_INT1		0x03
#define FSA9480_REG_INT2		0x04
#define FSA9480_REG_INT1_MASK		0x05
#define FSA9480_REG_INT2_MASK		0x06
#define FSA9480_REG_ADC			0x07
#define FSA9480_REG_TIMING1		0x08
#define FSA9480_REG_TIMING2		0x09
#define FSA9480_REG_DEV_T1		0x0a
#define FSA9480_REG_DEV_T2		0x0b
#define FSA9480_REG_BTN1		0x0c
#define FSA9480_REG_BTN2		0x0d
#define FSA9480_REG_CK			0x0e
#define FSA9480_REG_CK_INT1		0x0f
#define FSA9480_REG_CK_INT2		0x10
#define FSA9480_REG_CK_INTMASK1		0x11
#define FSA9480_REG_CK_INTMASK2		0x12
#define FSA9480_REG_MANSW1		0x13
#define FSA9480_REG_MANSW2		0x14

/* Control */
#define CON_SWITCH_OPEN		(1 << 4)
#define CON_RAW_DATA		(1 << 3)
#define CON_MANUAL_SW		(1 << 2)
#define CON_WAIT		(1 << 1)
#define CON_INT_MASK		(1 << 0)
#define CON_MASK		(CON_SWITCH_OPEN | CON_RAW_DATA | \
				CON_MANUAL_SW | CON_WAIT)

/* Device Type 1 */
#define DEV_USB_OTG		(1 << 7)
#define DEV_DEDICATED_CHG	(1 << 6)
#define DEV_USB_CHG		(1 << 5)
#define DEV_CAR_KIT		(1 << 4)
#define DEV_UART		(1 << 3)
#define DEV_USB			(1 << 2)
#define DEV_AUDIO_2		(1 << 1)
#define DEV_AUDIO_1		(1 << 0)

#define DEV_T1_USB_MASK		(DEV_USB_OTG | DEV_USB)
#define DEV_T1_UART_MASK	(DEV_UART)
#define DEV_T1_CHARGER_MASK	(DEV_DEDICATED_CHG | DEV_USB_CHG)

/* Device Type 2 */
#define DEV_AV			(1 << 6)
#define DEV_TTY			(1 << 5)
#define DEV_PPD			(1 << 4)
#define DEV_JIG_UART_OFF	(1 << 3)
#define DEV_JIG_UART_ON		(1 << 2)
#define DEV_JIG_USB_OFF		(1 << 1)
#define DEV_JIG_USB_ON		(1 << 0)

#define DEV_T2_USB_MASK		(DEV_JIG_USB_OFF | DEV_JIG_USB_ON)
#define DEV_T2_UART_MASK	(DEV_JIG_UART_OFF | DEV_JIG_UART_ON)
#define DEV_T2_JIG_MASK		(DEV_JIG_USB_OFF | DEV_JIG_USB_ON | \
				DEV_JIG_UART_OFF | DEV_JIG_UART_ON)

/*
 * Manual Switch
 * D- [7:5] / D+ [4:2]
 * 000: Open all / 001: USB / 010: AUDIO / 011: UART / 100: V_AUDIO
 */
#define SW_VAUDIO		((4 << 5) | (4 << 2))
#define SW_UART			((3 << 5) | (3 << 2))
#define SW_AUDIO		((2 << 5) | (2 << 2))
#define SW_DHOST		((1 << 5) | (1 << 2))
#define SW_AUTO			((0 << 5) | (0 << 2))

/* Interrupt 1 */
#define INT_DETACH		(1 << 1)
#define INT_ATTACH		(1 << 0)

struct fsa9480_usbsw {
	struct i2c_client		*client;
	struct switch_dev		sdev;
	struct fsa9480_platform_data	*pdata;
	int				dev1;
	int				dev2;
	int				mansw;
};

static struct fsa9480_usbsw *chip;

static int fsa9480_write_reg(struct i2c_client *client,
		int reg, int value)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int fsa9480_read_reg(struct i2c_client *client, int reg)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int fsa9480_read_irq(struct i2c_client *client, int *value)
{
	int ret;

	ret = i2c_smbus_read_i2c_block_data(client,
			FSA9480_REG_INT1, 2, (u8 *)value);
	*value &= 0xffff;

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static const char *states_bool[] = {
	"0",
	"1",
};

static const char *states_mansw[] = {
	"auto",
	"dhost",
	"audio",
	"uart",
	"vaudio",
};

static int fsa9480_mansw_get_state(struct switch_dev *sdev,
				 struct switch_attr *attr)
{
	struct fsa9480_usbsw *usbsw =
			container_of(sdev, struct fsa9480_usbsw, sdev);
	struct i2c_client *client = usbsw->client;
	unsigned ret, state;

	ret = fsa9480_read_reg(client, FSA9480_REG_MANSW1);
	if (ret < 0)
		return ret;

	switch(ret) {
	case SW_AUTO:
		state = FSA9480_SWITCH_SEL_AUTO;
		break;
	case SW_DHOST:
		state = FSA9480_SWITCH_SEL_DHOST;
		break;
	case SW_AUDIO:
		state = FSA9480_SWITCH_SEL_AUDIO;
		break;
	case SW_UART:
		state = FSA9480_SWITCH_SEL_UART;
		break;
	case SW_VAUDIO:
		state = FSA9480_SWITCH_SEL_VAUDIO;
		break;
	default:
		state = ret;
	}

	return state;
}

static int fsa9480_mansw_set_state(struct switch_dev *sdev,
				 struct switch_attr *attr, unsigned state)
{
	struct fsa9480_usbsw *usbsw =
			container_of(sdev, struct fsa9480_usbsw, sdev);
	struct i2c_client *client = usbsw->client;
	unsigned int value;
	unsigned int path = 0;

	value = fsa9480_read_reg(client, FSA9480_REG_CTRL);

	switch(state) {
	case FSA9480_SWITCH_SEL_AUTO:
		path = SW_AUTO;
		value |= CON_MANUAL_SW;
		break;
	case FSA9480_SWITCH_SEL_DHOST:
		path = SW_DHOST;
		value &= ~CON_MANUAL_SW;
		break;
	case FSA9480_SWITCH_SEL_AUDIO:
		path = SW_AUDIO;
		value &= ~CON_MANUAL_SW;
		break;
	case FSA9480_SWITCH_SEL_UART:
		path = SW_UART;
		value &= ~CON_MANUAL_SW;
		break;
	case FSA9480_SWITCH_SEL_VAUDIO:
		path = SW_VAUDIO;
		value &= ~CON_MANUAL_SW;
		break;
	default:
		return -EINVAL;
	}

	usbsw->mansw = path;
	fsa9480_write_reg(client, FSA9480_REG_MANSW1, path);
	fsa9480_write_reg(client, FSA9480_REG_CTRL, value);

	return 0;
}


static struct switch_attr attrs[] = {
	[FSA9480_SWITCH_SELECT] = {
		.name		= "select",
		.states		= states_mansw,
		.states_nr	= ARRAY_SIZE(states_mansw),
		.get_state	= fsa9480_mansw_get_state,
		.set_state	= fsa9480_mansw_set_state,
		.flags		= SWITCH_ATTR_READABLE | SWITCH_ATTR_WRITABLE |
				  SWITCH_ATTR_IGNSTATE,
	},
	[FSA9480_SWITCH_USB] = {
		.name		= "usb",
		.states		= states_bool,
		.states_nr	= ARRAY_SIZE(states_bool),
		.flags		= SWITCH_ATTR_READABLE | SWITCH_ATTR_UEVENT,
	},
	[FSA9480_SWITCH_UART] = {
		.name		= "uart",
		.states		= states_bool,
		.states_nr	= ARRAY_SIZE(states_bool),
		.flags		= SWITCH_ATTR_READABLE | SWITCH_ATTR_UEVENT,
	},
	[FSA9480_SWITCH_CHARGER] = {
		.name		= "charger",
		.states		= states_bool,
		.states_nr	= ARRAY_SIZE(states_bool),
		.flags		= SWITCH_ATTR_READABLE | SWITCH_ATTR_UEVENT,
	},
	[FSA9480_SWITCH_JIG] = {
		.name		= "jig",
		.states		= states_bool,
		.states_nr	= ARRAY_SIZE(states_bool),
		.flags		= SWITCH_ATTR_READABLE | SWITCH_ATTR_UEVENT,
	},
	[FSA9480_SWITCH_DESKDOCK] = {
		.name		= "deskdock",
		.states		= states_bool,
		.states_nr	= ARRAY_SIZE(states_bool),
		.flags		= SWITCH_ATTR_READABLE | SWITCH_ATTR_UEVENT,
	},
	[FSA9480_SWITCH_CARDOCK] = {
		.name		= "cardock",
		.states		= states_bool,
		.states_nr	= ARRAY_SIZE(states_bool),
		.flags		= SWITCH_ATTR_READABLE | SWITCH_ATTR_UEVENT,
	},
};

static ssize_t fsa9480_show_device(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct fsa9480_usbsw *usbsw = dev_get_drvdata(dev);
	struct i2c_client *client = usbsw->client;
	int dev1, dev2;

	dev1 = fsa9480_read_reg(client, FSA9480_REG_DEV_T1);
	dev2 = fsa9480_read_reg(client, FSA9480_REG_DEV_T2);

	if (!dev1 && !dev2)
		return sprintf(buf, "NONE\n");

	/* USB */
	if (dev1 & DEV_T1_USB_MASK || dev2 & DEV_T2_USB_MASK)
		return sprintf(buf, "USB\n");

	/* UART */
	if (dev1 & DEV_T1_UART_MASK || dev2 & DEV_T2_UART_MASK)
		return sprintf(buf, "UART\n");

	/* CHARGER */
	if (dev1 & DEV_T1_CHARGER_MASK)
		return sprintf(buf, "CHARGER\n");

	/* JIG */
	if (dev2 & DEV_T2_JIG_MASK)
		return sprintf(buf, "JIG\n");

	return sprintf(buf, "UNKNOWN\n");
}

static DEVICE_ATTR(device, S_IRUGO, fsa9480_show_device, NULL);

static struct attribute *fsa9480_attributes[] = {
	&dev_attr_device.attr,
	NULL
};

static const struct attribute_group fsa9480_group = {
	.attrs = fsa9480_attributes,
};

static void fsa9480_detect_dev(struct fsa9480_usbsw *usbsw, int intr)
{
	int val1, val2, ctrl, ret;
	struct i2c_client *client = usbsw->client;

	val1 = fsa9480_read_reg(client, FSA9480_REG_DEV_T1);
	val2 = fsa9480_read_reg(client, FSA9480_REG_DEV_T2);
	ctrl = fsa9480_read_reg(client, FSA9480_REG_CTRL);

	dev_info(&client->dev, "intr: 0x%x, dev1: 0x%x, dev2: 0x%x\n",
			intr, val1, val2);

	if (!intr)
		goto out;

	if (intr & INT_ATTACH) {	/* Attached */
		/* USB */
		if (val1 & DEV_T1_USB_MASK || val2 & DEV_T2_USB_MASK) {
			switch_set_state(&usbsw->sdev, FSA9480_SWITCH_USB,
					FSA9480_SWITCH_ATTACHED);

			if (usbsw->mansw) {
				fsa9480_write_reg(client,
					FSA9480_REG_MANSW1, usbsw->mansw);
			}
		}

		/* UART */
		if (val1 & DEV_T1_UART_MASK || val2 & DEV_T2_UART_MASK) {
			switch_set_state(&usbsw->sdev, FSA9480_SWITCH_UART,
					FSA9480_SWITCH_ATTACHED);

			if (!(ctrl & CON_MANUAL_SW)) {
				fsa9480_write_reg(client,
					FSA9480_REG_MANSW1, SW_UART);
			}
		}

		/* CHARGER */
		if (val1 & DEV_T1_CHARGER_MASK) {
			switch_set_state(&usbsw->sdev, FSA9480_SWITCH_CHARGER,
					FSA9480_SWITCH_ATTACHED);
		}

		/* JIG */
		if (val2 & DEV_T2_JIG_MASK) {
			switch_set_state(&usbsw->sdev, FSA9480_SWITCH_JIG,
					FSA9480_SWITCH_ATTACHED);
		}

		/* Desk Dock */
		if (val2 & DEV_AV) {
			switch_set_state(&usbsw->sdev, FSA9480_SWITCH_DESKDOCK,
					FSA9480_SWITCH_ATTACHED);

			fsa9480_write_reg(client, FSA9480_REG_MANSW1, SW_DHOST);
			ret = fsa9480_read_reg(client, FSA9480_REG_CTRL);
			fsa9480_write_reg(client, FSA9480_REG_CTRL,
					ret & ~CON_MANUAL_SW);
		}

		/* Car Dock */
		if (val2 & DEV_JIG_UART_ON) {
			switch_set_state(&usbsw->sdev, FSA9480_SWITCH_CARDOCK,
					FSA9480_SWITCH_ATTACHED);
		}
	} else if (intr & INT_DETACH) {	/* Detached */
		/* USB */
		if (usbsw->dev1 & DEV_T1_USB_MASK ||
			usbsw->dev2 & DEV_T2_USB_MASK) {
			switch_set_state(&usbsw->sdev, FSA9480_SWITCH_USB,
					FSA9480_SWITCH_DETACHED);
		}

		/* UART */
		if (usbsw->dev1 & DEV_T1_UART_MASK ||
			usbsw->dev2 & DEV_T2_UART_MASK) {
			switch_set_state(&usbsw->sdev, FSA9480_SWITCH_UART,
					FSA9480_SWITCH_DETACHED);
		}

		/* CHARGER */
		if (usbsw->dev1 & DEV_T1_CHARGER_MASK) {
			switch_set_state(&usbsw->sdev, FSA9480_SWITCH_CHARGER,
					FSA9480_SWITCH_DETACHED);
		}

		/* JIG */
		if (usbsw->dev2 & DEV_T2_JIG_MASK) {
			switch_set_state(&usbsw->sdev, FSA9480_SWITCH_JIG,
					FSA9480_SWITCH_DETACHED);
		}

		/* Desk Dock */
		if (usbsw->dev2 & DEV_AV) {
			switch_set_state(&usbsw->sdev, FSA9480_SWITCH_DESKDOCK,
					FSA9480_SWITCH_DETACHED);

			ret = fsa9480_read_reg(client, FSA9480_REG_CTRL);
			fsa9480_write_reg(client, FSA9480_REG_CTRL,
					ret | CON_MANUAL_SW);
		}

		/* Car Dock */
		if (usbsw->dev2 & DEV_JIG_UART_ON) {
			switch_set_state(&usbsw->sdev, FSA9480_SWITCH_CARDOCK,
					FSA9480_SWITCH_DETACHED);
		}
	}

	usbsw->dev1 = val1;
	usbsw->dev2 = val2;

out:
	ctrl &= ~CON_INT_MASK;
	fsa9480_write_reg(client, FSA9480_REG_CTRL, ctrl);
}

static irqreturn_t fsa9480_irq_handler(int irq, void *data)
{
	struct fsa9480_usbsw *usbsw = data;
	struct i2c_client *client = usbsw->client;
	int intr;

	/* clear interrupt */
	fsa9480_read_irq(client, &intr);

	/* device detection */
	fsa9480_detect_dev(usbsw, intr);

	return IRQ_HANDLED;
}

static int fsa9480_irq_init(struct fsa9480_usbsw *usbsw)
{
	struct fsa9480_platform_data *pdata = usbsw->pdata;
	struct i2c_client *client = usbsw->client;
	int ret;
	int intr;
	unsigned int ctrl = CON_MASK;

	/* clear interrupt */
	fsa9480_read_irq(client, &intr);

	/* unmask interrupt (attach/detach only) */
	fsa9480_write_reg(client, FSA9480_REG_INT1_MASK, 0xfc);
	fsa9480_write_reg(client, FSA9480_REG_INT2_MASK, 0x1f);

	usbsw->mansw = fsa9480_read_reg(client, FSA9480_REG_MANSW1);

	if (usbsw->mansw)
		ctrl &= ~CON_MANUAL_SW;	/* Manual Switching Mode */

	fsa9480_write_reg(client, FSA9480_REG_CTRL, ctrl);

	if (pdata && pdata->cfg_gpio)
		pdata->cfg_gpio();

	if (client->irq) {
		ret = request_threaded_irq(client->irq, NULL,
				fsa9480_irq_handler,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"fsa9480 micro USB", usbsw);
		if (ret) {
			dev_err(&client->dev, "failed to reqeust IRQ\n");
			return ret;
		}

		if (pdata)
			device_init_wakeup(&client->dev, pdata->wakeup);
	}

	return 0;
}

static int __devinit fsa9480_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct fsa9480_usbsw *usbsw;
	int ret = 0;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -EIO;

	usbsw = kzalloc(sizeof(struct fsa9480_usbsw), GFP_KERNEL);
	if (!usbsw) {
		dev_err(&client->dev, "failed to allocate driver data\n");
		return -ENOMEM;
	}

	usbsw->client = client;
	usbsw->pdata = client->dev.platform_data;

	chip = usbsw;

	i2c_set_clientdata(client, usbsw);

	usbsw->sdev.name = "fsa9480";
	usbsw->sdev.attrs = attrs;
	usbsw->sdev.attrs_nr = ARRAY_SIZE(attrs);
	
	ret = switch_dev_register(&usbsw->sdev);
	if (ret) {
		dev_err(&client->dev, "failed to create switch_dev\n");
		goto err_switch_dev;
	}

	ret = fsa9480_irq_init(usbsw);
	if (ret)
		goto err_irq_init;

	ret = sysfs_create_group(&client->dev.kobj, &fsa9480_group);
	if (ret) {
		dev_err(&client->dev,
				"failed to create fsa9480 attribute group\n");
		goto err_sysfs_create;
	}

	/* ADC Detect Time: 500ms */
	fsa9480_write_reg(client, FSA9480_REG_TIMING1, 0x6);

	if (chip->pdata->reset_cb)
		chip->pdata->reset_cb();

	/* device detection */
	fsa9480_detect_dev(usbsw, INT_ATTACH);

	pm_runtime_set_active(&client->dev);

	return 0;

err_sysfs_create:
	if (client->irq)
		free_irq(client->irq, usbsw);
err_irq_init:
	switch_dev_unregister(&usbsw->sdev);
err_switch_dev:
	kfree(usbsw);
	return ret;
}

static int __devexit fsa9480_remove(struct i2c_client *client)
{
	struct fsa9480_usbsw *usbsw = i2c_get_clientdata(client);
	if (client->irq)
		free_irq(client->irq, usbsw);

	sysfs_remove_group(&client->dev.kobj, &fsa9480_group);
	device_init_wakeup(&client->dev, 0);
	kfree(usbsw);
	return 0;
}

#ifdef CONFIG_PM
static int fsa9480_suspend(struct device *dev)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct fsa9480_usbsw *usbsw = i2c_get_clientdata(client);
	struct fsa9480_platform_data *pdata = usbsw->pdata;

	if (device_may_wakeup(&client->dev) && client->irq)
		enable_irq_wake(client->irq);

	if (pdata->usb_power)
		pdata->usb_power(0);

	return 0;
}

static int fsa9480_resume(struct device *dev)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct fsa9480_usbsw *usbsw = i2c_get_clientdata(client);
	int dev1, dev2;

	if (device_may_wakeup(&client->dev) && client->irq)
		disable_irq_wake(client->irq);

	/*
	 * Clear Pending interrupt. Note that detect_dev does what
	 * the interrupt handler does. So, we don't miss pending and
	 * we reenable interrupt if there is one.
	 */
	fsa9480_read_reg(client, FSA9480_REG_INT1);
	fsa9480_read_reg(client, FSA9480_REG_INT2);

	dev1 = fsa9480_read_reg(client, FSA9480_REG_DEV_T1);
	dev2 = fsa9480_read_reg(client, FSA9480_REG_DEV_T2);

	/* device detection */
	fsa9480_detect_dev(usbsw, (dev1 || dev2) ? INT_ATTACH : INT_DETACH);

	return 0;
}

static const struct dev_pm_ops fsa9480_pm_ops = {
	.suspend		= fsa9480_suspend,
	.resume			= fsa9480_resume,
};
#endif /* CONFIG_PM */

static const struct i2c_device_id fsa9480_id[] = {
	{"fsa9480", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, fsa9480_id);

static struct i2c_driver fsa9480_i2c_driver = {
	.driver = {
		.name = "fsa9480",
#ifdef CONFIG_PM
		.pm	= &fsa9480_pm_ops,
#endif
	},
	.probe = fsa9480_probe,
	.remove = __devexit_p(fsa9480_remove),
	.id_table = fsa9480_id,
};

module_i2c_driver(fsa9480_i2c_driver);

MODULE_AUTHOR("Minkyu Kang <mk7.kang@samsung.com>");
MODULE_DESCRIPTION("FSA9480 USB Switch driver");
MODULE_LICENSE("GPL");
