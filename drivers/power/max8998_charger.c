/*
 * max8998_charger.c - Power supply consumer driver for the Maxim 8998/LP3974
 *
 *  Copyright (C) 2009-2010 Samsung Electronics
 *  MyungJoo Ham <myungjoo.ham@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/module.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/mfd/max8998.h>
#include <linux/mfd/max8998-private.h>
#include <linux/switch.h>
#include <linux/platform_data/fsa9480.h>

struct max8998_battery_data {
	struct device *dev;
	struct max8998_dev *iodev;
	struct power_supply psy_ac;
	struct power_supply psy_usb;
	struct switch_handler sw_ac;
	struct switch_handler sw_usb;
	struct switch_handler sw_deskdock;
	unsigned state_ac, state_usb;
};

static enum power_supply_property max8998_psy_props[] = {
	POWER_SUPPLY_PROP_PRESENT, /* the presence of charger */
	POWER_SUPPLY_PROP_ONLINE, /* charger is active or not */
	POWER_SUPPLY_PROP_CHARGE_TYPE, /* the type of charger */
};

/* Note that the charger control is done by a current regulator "CHARGER" */
static int max8998_psy_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct max8998_battery_data *max8998;
	struct i2c_client *i2c;
	unsigned state;
	int ret;
	u8 reg;

	if (psy->type == POWER_SUPPLY_TYPE_MAINS) {
		max8998 = container_of(psy, struct max8998_battery_data, psy_ac);
		state = max8998->state_ac;
	} else {
		max8998 = container_of(psy, struct max8998_battery_data, psy_usb);
		state = max8998->state_usb;
	}

	i2c = max8998->iodev->i2c;

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		if (!state) {
			val->intval = 0;
			return 0;
		}
		ret = max8998_read_reg(i2c, MAX8998_REG_STATUS2, &reg);
		if (ret)
			return ret;
		val->intval = !!(reg & MAX8998_MASK_VDCIN);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		if (!state) {
			val->intval = 0;
			return 0;
		}
		ret = max8998_read_reg(i2c, MAX8998_REG_STATUS2, &reg);
		if (ret)
			return ret;
		val->intval = !!(reg & MAX8998_MASK_CHGON);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		if (!state) {
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
			return 0;
		}
		ret = max8998_read_reg(i2c, MAX8998_REG_STATUS2, &reg);
		if (ret)
			return ret;
		if (reg & MAX8998_MASK_CHGON) {
			if (reg & MAX8998_MASK_FCHG)
				val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
			else
				val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
		} else
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static void switch_handler_ac(struct switch_handler *handler, unsigned state)
{
	struct max8998_battery_data *max8998 =
			(struct max8998_battery_data *)handler->data;

	max8998->state_ac = state;
	power_supply_changed(&max8998->psy_ac);
}

static void switch_handler_usb(struct switch_handler *handler, unsigned state)
{
	struct max8998_battery_data *max8998 =
			(struct max8998_battery_data *)handler->data;

	max8998->state_usb = state;
	power_supply_changed(&max8998->psy_usb);
}

static __devinit int max8998_battery_probe(struct platform_device *pdev)
{
	struct max8998_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct max8998_platform_data *pdata = dev_get_platdata(iodev->dev);
	struct max8998_battery_data *max8998;
	struct i2c_client *i2c;
	int ret = 0;

	if (!pdata) {
		dev_err(pdev->dev.parent, "No platform init data supplied\n");
		return -ENODEV;
	}

	max8998 = kzalloc(sizeof(struct max8998_battery_data), GFP_KERNEL);
	if (!max8998)
		return -ENOMEM;

	max8998->dev = &pdev->dev;
	max8998->iodev = iodev;
	platform_set_drvdata(pdev, max8998);
	i2c = max8998->iodev->i2c;

	/* Setup "End of Charge" */
	/* If EOC value equals 0,
	 * remain value set from bootloader or default value */
	if (pdata->eoc >= 10 && pdata->eoc <= 45) {
		max8998_update_reg(i2c, MAX8998_REG_CHGR1,
				(pdata->eoc / 5 - 2) << 5, 0x7 << 5);
	} else if (pdata->eoc == 0) {
		dev_dbg(max8998->dev,
			"EOC value not set: leave it unchanged.\n");
	} else {
		dev_err(max8998->dev, "Invalid EOC value\n");
		ret = -EINVAL;
		goto err;
	}

	/* Setup Charge Restart Level */
	switch (pdata->restart) {
	case 100:
		max8998_update_reg(i2c, MAX8998_REG_CHGR1, 0x1 << 3, 0x3 << 3);
		break;
	case 150:
		max8998_update_reg(i2c, MAX8998_REG_CHGR1, 0x0 << 3, 0x3 << 3);
		break;
	case 200:
		max8998_update_reg(i2c, MAX8998_REG_CHGR1, 0x2 << 3, 0x3 << 3);
		break;
	case -1:
		max8998_update_reg(i2c, MAX8998_REG_CHGR1, 0x3 << 3, 0x3 << 3);
		break;
	case 0:
		dev_dbg(max8998->dev,
			"Restart Level not set: leave it unchanged.\n");
		break;
	default:
		dev_err(max8998->dev, "Invalid Restart Level\n");
		ret = -EINVAL;
		goto err;
	}

	/* Setup Charge Full Timeout */
	switch (pdata->timeout) {
	case 5:
		max8998_update_reg(i2c, MAX8998_REG_CHGR2, 0x0 << 4, 0x3 << 4);
		break;
	case 6:
		max8998_update_reg(i2c, MAX8998_REG_CHGR2, 0x1 << 4, 0x3 << 4);
		break;
	case 7:
		max8998_update_reg(i2c, MAX8998_REG_CHGR2, 0x2 << 4, 0x3 << 4);
		break;
	case -1:
		max8998_update_reg(i2c, MAX8998_REG_CHGR2, 0x3 << 4, 0x3 << 4);
		break;
	case 0:
		dev_dbg(max8998->dev,
			"Full Timeout not set: leave it unchanged.\n");
		break;
	default:
		dev_err(max8998->dev, "Invalid Full Timeout value\n");
		ret = -EINVAL;
		goto err;
	}

	max8998->psy_ac.name = "max8998_ac";
	max8998->psy_ac.type = POWER_SUPPLY_TYPE_MAINS;
	max8998->psy_ac.get_property = max8998_psy_get_property;
	max8998->psy_ac.properties = max8998_psy_props;
	max8998->psy_ac.num_properties = ARRAY_SIZE(max8998_psy_props);

	ret = power_supply_register(max8998->dev, &max8998->psy_ac);
	if (ret) {
		dev_err(max8998->dev, "failed: power supply register: ac\n");
		goto err;
	}

	max8998->psy_usb.name = "max8998_usb";
	max8998->psy_usb.type = POWER_SUPPLY_TYPE_USB;
	max8998->psy_usb.get_property = max8998_psy_get_property;
	max8998->psy_usb.properties = max8998_psy_props;
	max8998->psy_usb.num_properties = ARRAY_SIZE(max8998_psy_props);

	ret = power_supply_register(max8998->dev, &max8998->psy_usb);
	if (ret) {
		dev_err(max8998->dev, "failed: power supply register: usb\n");
		goto err_psy;
	}
	
	max8998->sw_ac.dev = "fsa9480";
	max8998->sw_ac.attr_id = FSA9480_SWITCH_CHARGER;
	max8998->sw_ac.data = max8998;
	max8998->sw_ac.func = switch_handler_ac;
	if (switch_handler_register(&max8998->sw_ac)) {
		dev_err(max8998->dev, "failed: switch handler: ac\n");
		goto err_psy;
	}
	
	max8998->sw_usb.dev = "fsa9480";
	max8998->sw_usb.attr_id = FSA9480_SWITCH_USB;
	max8998->sw_usb.data = max8998;
	max8998->sw_usb.func = switch_handler_usb;
	if (switch_handler_register(&max8998->sw_usb)) {
		dev_err(max8998->dev, "failed: switch handler: usb\n");
		goto err_sw1;
	}

	max8998->sw_deskdock.dev = "fsa9480";
	max8998->sw_deskdock.attr_id = FSA9480_SWITCH_USB;
	max8998->sw_deskdock.data = max8998;
	max8998->sw_deskdock.func = switch_handler_usb;
	if (switch_handler_register(&max8998->sw_deskdock)) {
		dev_err(max8998->dev, "failed: switch handler: deskdock\n");
		goto err_sw2;
	}

	return 0;
err_sw2:
	switch_handler_remove(&max8998->sw_usb);
err_sw1:
	switch_handler_remove(&max8998->sw_ac);
err_psy:
	power_supply_unregister(&max8998->psy_ac);
err:
	kfree(max8998);
	return ret;
}

static int __devexit max8998_battery_remove(struct platform_device *pdev)
{
	struct max8998_battery_data *max8998 = platform_get_drvdata(pdev);

	switch_handler_remove(&max8998->sw_ac);
	switch_handler_remove(&max8998->sw_usb);
	switch_handler_remove(&max8998->sw_deskdock);
	power_supply_unregister(&max8998->psy_ac);
	power_supply_unregister(&max8998->psy_usb);
	kfree(max8998);

	return 0;
}

static const struct platform_device_id max8998_battery_id[] = {
	{ "max8998-battery", TYPE_MAX8998 },
	{ }
};

static struct platform_driver max8998_battery_driver = {
	.driver = {
		.name = "max8998-battery",
		.owner = THIS_MODULE,
	},
	.probe = max8998_battery_probe,
	.remove = __devexit_p(max8998_battery_remove),
	.id_table = max8998_battery_id,
};

module_platform_driver(max8998_battery_driver);

MODULE_DESCRIPTION("MAXIM 8998 battery control driver");
MODULE_AUTHOR("MyungJoo Ham <myungjoo.ham@samsung.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:max8998-battery");
