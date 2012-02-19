/*
 * Copyright (C) 2011 Samsung Electronics Co., Ltd.
 * MyungJoo Ham <myungjoo.ham@samsung.com>
 *
 * This driver enables to monitor battery health and control charger
 * during suspend-to-mem.
 * Charger manager depends on other devices. register this later than
 * the depending devices.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
**/

#include <linux/io.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/rtc.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/power/charger-manager-android.h>
#include <linux/regulator/consumer.h>
#include <linux/wakelock.h>

#include "../staging/android/android_alarm.h"

#define UEVENT_BUF_SIZE	32
#define PSY_NAME_MAX	30

/**
 * struct charger_manager
 * @entry: entry for list
 * @dev: device pointer
 * @desc: instance of charger_desc
 * @fuel_gauge: power_supply for fuel gauge
 * @charger_stat: array of power_supply for chargers
 * @charger_enabled: the state of charger
 * @emergency_stop:
 *	When setting true, stop charging
 * @last_temp_mC: the measured temperature in milli-Celsius
 * @psy_name_buf: the name of power-supply-class for charger manager
 * @charger_psy: power_supply for charger manager
 * @status_save_ext_pwr_inserted:
 *	saved status of external power before entering suspend-to-RAM
 * @status_save_batt:
 *	saved status of battery before entering suspend-to-RAM
 */
struct charger_manager {
	struct list_head entry;
	struct device *dev;
	struct charger_desc_android *desc;

	struct work_struct work;
	struct wake_lock wakelock;
	struct android_alarm alarm;
	ktime_t last_poll;
	bool slow_poll;
	bool external_changed;

	struct power_supply *fuel_gauge;
	struct power_supply **charger_stat;

	bool charger_enabled;

	int emergency_stop;
	int last_temp_mC;

	char psy_name_buf[PSY_NAME_MAX + 1];
	char *psy_name_ptr[1];
	struct power_supply charger_psy;

	bool status_save_ext_pwr_inserted;
	bool status_save_batt;
};

/**
 * is_batt_present - See if the battery presents in place.
 * @cm: the Charger Manager representing the battery.
 */
static bool is_batt_present(struct charger_manager *cm)
{
	union power_supply_propval val;
	bool present = false;
	int i, ret;

	switch (cm->desc->battery_present) {
	case CM_FUEL_GAUGE:
		ret = cm->fuel_gauge->get_property(cm->fuel_gauge,
				POWER_SUPPLY_PROP_PRESENT, &val);
		if (ret == 0 && val.intval)
			present = true;
		break;
	case CM_CHARGER_STAT:
		for (i = 0; cm->charger_stat[i]; i++) {
			ret = cm->charger_stat[i]->get_property(
					cm->charger_stat[i],
					POWER_SUPPLY_PROP_PRESENT, &val);
			if (ret == 0 && val.intval) {
				present = true;
				break;
			}
		}
		break;
	}

	return present;
}

/**
 * is_ext_pwr_online - See if an external power source is attached to charge
 * @cm: the Charger Manager representing the battery.
 *
 * Returns true if at least one of the chargers of the battery has an external
 * power source attached to charge the battery regardless of whether it is
 * actually charging or not.
 */
static bool is_ext_pwr_online(struct charger_manager *cm)
{
	union power_supply_propval val;
	bool online = false;
	int i, ret;

	/* If at least one of them has one, it's yes. */
	for (i = 0; cm->charger_stat[i]; i++) {
		ret = cm->charger_stat[i]->get_property(
				cm->charger_stat[i],
				POWER_SUPPLY_PROP_ONLINE, &val);
		if (ret == 0 && val.intval) {
			online = true;
			break;
		}
	}

	return online;
}

/**
 * get_batt_uV - Get the voltage level of the battery
 * @cm: the Charger Manager representing the battery.
 * @uV: the voltage level returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_batt_uV(struct charger_manager *cm, int *uV)
{
	union power_supply_propval val;
	int ret;

	if (cm->fuel_gauge)
		ret = cm->fuel_gauge->get_property(cm->fuel_gauge,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
	else
		return -ENODEV;

	if (ret)
		return ret;

	*uV = val.intval;
	return 0;
}

/**
 * is_charging - Returns true if the battery is being charged.
 * @cm: the Charger Manager representing the battery.
 */
static bool is_charging(struct charger_manager *cm)
{
	int i, ret;
	bool charging = false;
	union power_supply_propval val;

	if (!is_batt_present(cm) || cm->emergency_stop || !cm->charger_enabled)
		return false;

	/* If at least one of the charger is charging, return yes */
	for (i = 0; cm->charger_stat[i]; i++) {
		/* The charger should be online (ext-power) */
		ret = cm->charger_stat[i]->get_property(
				cm->charger_stat[i],
				POWER_SUPPLY_PROP_ONLINE, &val);
		if (ret) {
			dev_warn(cm->dev, "Cannot read ONLINE value from %s.\n",
					cm->desc->psy_charger_stat[i]);
			continue;
		}
		if (val.intval == 0)
			continue;

		/*
		 * Check charging type if available
		 */
		ret = cm->charger_stat[i]->get_property(
				cm->charger_stat[i],
				POWER_SUPPLY_PROP_CHARGE_TYPE, &val);
		if (!ret) {
			if (!(val.intval == POWER_SUPPLY_CHARGE_TYPE_FAST ||
			     val.intval == POWER_SUPPLY_CHARGE_TYPE_TRICKLE))
				continue;
		}

		/* Then, this is charging. */
		charging = true;
		break;
	}

	return charging;
}

/**
 * try_charger_enable - Enable/Disable chargers altogether
 * @cm: the Charger Manager representing the battery.
 * @enable: true: enable / false: disable
 *
 * Note that Charger Manager keeps the charger enabled regardless whether
 * the charger is charging or not (because battery is full or no external
 * power source exists) except when CM needs to disable chargers forcibly
 * bacause of emergency causes; when the battery is overheated or too cold.
 */
static int try_charger_enable(struct charger_manager *cm, bool enable)
{
	int err = 0, i;
	struct charger_desc_android *desc = cm->desc;

	/* Ignore if it's redundent command */
	if (enable && cm->charger_enabled)
		return 0;
	if (!enable && !cm->charger_enabled)
		return 0;

	if (enable) {
		if (cm->emergency_stop)
			return -EAGAIN;
		err = regulator_bulk_enable(desc->num_charger_regulators,
					desc->charger_regulators);
	} else {
		/*
		 * Abnormal battery state - Stop charging forcibly,
		 * even if charger was enabled at the other places
		 */
		err = regulator_bulk_disable(desc->num_charger_regulators,
					desc->charger_regulators);

		for (i = 0; i < desc->num_charger_regulators; i++) {
			if (regulator_is_enabled(
				    desc->charger_regulators[i].consumer)) {
				regulator_force_disable(
					desc->charger_regulators[i].consumer);
				dev_warn(cm->dev,
					"Disable regulator(%s) forcibly.\n",
					desc->charger_regulators[i].supply);
			}
		}
	}

	if (!err)
		cm->charger_enabled = enable;

	return err;
}

/**
 * uevent_notify - Let users know something has changed.
 * @cm: the Charger Manager representing the battery.
 * @event: the event string.
 *
 * If @event is null, it implies that uevent_notify is called
 * by resume function. When called in the resume function, cm_suspended
 * should be already reset to false in order to let uevent_notify
 * notify the recent event during the suspend to users. While
 * suspended, uevent_notify does not notify users, but tracks
 * events so that uevent_notify can notify users later after resumed.
 */
static void uevent_notify(struct charger_manager *cm, const char *event)
{
	static char env_str[UEVENT_BUF_SIZE + 1] = "";
	static char env_str_save[UEVENT_BUF_SIZE + 1] = "";

	if (event == NULL) {
		/* No messages pending */
		if (!env_str_save[0])
			return;

		strncpy(env_str, env_str_save, UEVENT_BUF_SIZE);
		kobject_uevent(&cm->dev->kobj, KOBJ_CHANGE);
		env_str_save[0] = 0;

		return;
	}

	/* status not changed */
	if (!strncmp(env_str, event, UEVENT_BUF_SIZE))
		return;

	/* save the status and notify the update */
	strncpy(env_str, event, UEVENT_BUF_SIZE);
	kobject_uevent(&cm->dev->kobj, KOBJ_CHANGE);

	dev_info(cm->dev, event);
}

/**
 * cm_monitor - Monitor the temperature and return true for exceptions.
 * @cm: the Charger Manager representing the battery.
 *
 * Returns true if there is an event to notify for the battery.
 * (True if the status of "emergency_stop" changes)
 */
static bool cm_monitor(struct charger_manager *cm)
{
	struct charger_desc_android *desc = cm->desc;
	int temp = desc->temperature_out_of_range(&cm->last_temp_mC);

	dev_dbg(cm->dev, "monitoring (%2.2d.%3.3dC)\n",
		cm->last_temp_mC / 1000, cm->last_temp_mC % 1000);

	/* It has been stopped or charging already */
	if (!!temp == !!cm->emergency_stop)
		return false;

	if (temp) {
		cm->emergency_stop = temp;
		if (!try_charger_enable(cm, false)) {
			if (temp > 0)
				uevent_notify(cm, "OVERHEAT");
			else
				uevent_notify(cm, "COLD");
		}
	} else {
		cm->emergency_stop = 0;
		if (!try_charger_enable(cm, true))
			uevent_notify(cm, "CHARGING");
	}

	return true;
}

static void cm_setup_alarm(struct charger_manager *cm, int seconds)
{
	ktime_t low_interval = ktime_set(seconds - 10, 0);
	ktime_t slack = ktime_set(20, 0);
	ktime_t next;

	next = ktime_add(cm->last_poll, low_interval);
	android_alarm_start_range(&cm->alarm, next, ktime_add(next, slack));
}

static void cm_monitor_work(struct work_struct *work)
{
	struct charger_manager *cm =
		container_of(work, struct charger_manager, work);

	if (cm_monitor(cm) || cm->external_changed)
		power_supply_changed(&cm->charger_psy);

	cm->external_changed = false;
	cm->last_poll = alarm_get_elapsed_realtime();

	wake_unlock(&cm->wakelock);
	cm_setup_alarm(cm, cm->desc->fast_polling_interval);
}

static void cm_monitor_alarm(struct android_alarm *alarm)
{
	struct charger_manager *cm =
		container_of(alarm, struct charger_manager, alarm);

	wake_lock(&cm->wakelock);
	schedule_work(&cm->work);
}

static void cm_external_power_changed(struct power_supply *psy)
{
	struct charger_manager *cm = container_of(psy,
			struct charger_manager, charger_psy);

	cm->external_changed = true;
	wake_lock(&cm->wakelock);
	schedule_work(&cm->work);
}

static int charger_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct charger_manager *cm = container_of(psy,
			struct charger_manager, charger_psy);
	struct charger_desc_android *desc = cm->desc;
	int i, ret = 0, uV;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (is_charging(cm))
			if (!cm->fuel_gauge->get_property(cm->fuel_gauge,
			    POWER_SUPPLY_PROP_CAPACITY, val) &&
			    val->intval == 100)
				val->intval = POWER_SUPPLY_STATUS_FULL;
			else
				val->intval = POWER_SUPPLY_STATUS_CHARGING;
		else if (is_ext_pwr_online(cm))
			val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
		else
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		if (cm->emergency_stop > 0)
			val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
		else if (cm->emergency_stop < 0)
			val->intval = POWER_SUPPLY_HEALTH_COLD;
		else
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		if (is_batt_present(cm))
			val->intval = 1;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = get_batt_uV(cm, &i);
		val->intval = i;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = cm->fuel_gauge->get_property(cm->fuel_gauge,
				POWER_SUPPLY_PROP_CURRENT_NOW, val);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		/* in thenth of centigrade */
		if (cm->last_temp_mC == INT_MIN)
			desc->temperature_out_of_range(&cm->last_temp_mC);
		val->intval = cm->last_temp_mC / 100;
		if (!desc->measure_battery_temp)
			ret = -ENODEV;
		break;
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		/* in thenth of centigrade */
		if (cm->last_temp_mC == INT_MIN)
			desc->temperature_out_of_range(&cm->last_temp_mC);
		val->intval = cm->last_temp_mC / 100;
		if (desc->measure_battery_temp)
			ret = -ENODEV;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (!cm->fuel_gauge) {
			ret = -ENODEV;
			break;
		}

		if (!is_batt_present(cm)) {
			/* There is no battery. Assume 100% */
			val->intval = 100;
			break;
		}

		ret = cm->fuel_gauge->get_property(cm->fuel_gauge,
					POWER_SUPPLY_PROP_CAPACITY, val);
		if (ret)
			break;

		if (val->intval > 100) {
			val->intval = 100;
			break;
		}
		if (val->intval < 0)
			val->intval = 0;

		if (desc->fullbatt_uV == 0)
			break;

		/* Do not adjust SOC when charging: voltage is overrated */
		if (is_charging(cm))
			break;

		/*
		 * If the capacity value is inconsistent, calibrate it base on
		 * the battery voltage values and the thresholds given as desc
		 */
		ret = get_batt_uV(cm, &uV);
		if (ret) {
			/* Voltage information not available. No calibration */
			ret = 0;
			break;
		}

		if (uV >= desc->fullbatt_uV) {
			val->intval = 100;
			break;
		}

		break;
	case POWER_SUPPLY_PROP_ONLINE:
		if (is_ext_pwr_online(cm))
			val->intval = 1;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		if (cm->fuel_gauge) {
			if (cm->fuel_gauge->get_property(cm->fuel_gauge,
			    POWER_SUPPLY_PROP_CHARGE_FULL, val) == 0)
				break;
		}

		if (is_ext_pwr_online(cm)) {
			/* Not full if it's charging. */
			if (is_charging(cm)) {
				val->intval = 0;
				break;
			}
			/*
			 * Full if it's powered but not charging and
			 * not forced stop by emergency
			 */
			if (!cm->emergency_stop) {
				val->intval = 1;
				break;
			}
		}

		/* Full if it's over the fullbatt voltage */
		ret = get_batt_uV(cm, &uV);
		if (!ret && desc->fullbatt_uV > 0 && uV >= desc->fullbatt_uV &&
		    !is_charging(cm)) {
			val->intval = 1;
			break;
		}

		/* Full if the cap is 100 */
		if (cm->fuel_gauge) {
			ret = cm->fuel_gauge->get_property(cm->fuel_gauge,
					POWER_SUPPLY_PROP_CAPACITY, val);
			if (!ret && val->intval >= 100 && !is_charging(cm)) {
				val->intval = 1;
				break;
			}
		}

		val->intval = 0;
		ret = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		if (is_charging(cm)) {
			ret = cm->fuel_gauge->get_property(cm->fuel_gauge,
						POWER_SUPPLY_PROP_CHARGE_NOW,
						val);
			if (ret) {
				val->intval = 1;
				ret = 0;
			} else {
				/* If CHARGE_NOW is supplied, use it */
				val->intval = (val->intval > 0) ?
						val->intval : 1;
			}
		} else {
			val->intval = 0;
		}
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

#define NUM_CHARGER_PSY_OPTIONAL	(4)
static enum power_supply_property default_charger_props[] = {
	/* Guaranteed to provide */
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	/*
	 * Optional properties are:
	 * POWER_SUPPLY_PROP_CHARGE_NOW,
	 * POWER_SUPPLY_PROP_CURRENT_NOW,
	 * POWER_SUPPLY_PROP_TEMP, and
	 * POWER_SUPPLY_PROP_TEMP_AMBIENT,
	 */
};

static struct power_supply psy_default = {
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = default_charger_props,
	.num_properties = ARRAY_SIZE(default_charger_props),
	.get_property = charger_get_property,
	.external_power_changed = cm_external_power_changed,
};

static int charger_manager_probe(struct platform_device *pdev)
{
	struct charger_desc_android *desc = dev_get_platdata(&pdev->dev);
	struct charger_manager *cm;
	int ret = 0, i = 0;
	union power_supply_propval val;

	if (!desc) {
		dev_err(&pdev->dev, "No platform data (desc) found.\n");
		ret = -ENODEV;
		goto err_alloc;
	}

	cm = kzalloc(sizeof(struct charger_manager), GFP_KERNEL);
	if (!cm) {
		dev_err(&pdev->dev, "Cannot allocate memory.\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	/* Basic Values. Unspecified are Null or 0 */
	cm->dev = &pdev->dev;
	cm->desc = kzalloc(sizeof(struct charger_desc_android), GFP_KERNEL);
	if (!cm->desc) {
		dev_err(&pdev->dev, "Cannot allocate memory.\n");
		ret = -ENOMEM;
		goto err_alloc_desc;
	}
	memcpy(cm->desc, desc, sizeof(struct charger_desc_android));
	cm->last_temp_mC = INT_MIN; /* denotes "unmeasured, yet" */

	if (!desc->charger_regulators || desc->num_charger_regulators < 1) {
		ret = -EINVAL;
		dev_err(&pdev->dev, "charger_regulators undefined.\n");
		goto err_no_charger;
	}

	if (!desc->psy_charger_stat || !desc->psy_charger_stat[0]) {
		dev_err(&pdev->dev, "No power supply defined.\n");
		ret = -EINVAL;
		goto err_no_charger_stat;
	}

	/* Counting index only */
	while (desc->psy_charger_stat[i])
		i++;

	cm->charger_stat = kzalloc(sizeof(struct power_supply *) * (i + 1),
				   GFP_KERNEL);
	if (!cm->charger_stat) {
		ret = -ENOMEM;
		goto err_no_charger_stat;
	}

	memcpy(&cm->charger_psy, &psy_default,
				sizeof(psy_default));
	if (!desc->psy_name) {
		strncpy(cm->psy_name_buf, psy_default.name,
				PSY_NAME_MAX);
	} else {
		strncpy(cm->psy_name_buf, desc->psy_name, PSY_NAME_MAX);
	}
	cm->charger_psy.name = cm->psy_name_buf;
	cm->psy_name_ptr[0] = cm->psy_name_buf;

	for (i = 0; desc->psy_charger_stat[i]; i++) {
		struct power_supply *curpsy = power_supply_get_by_name(
					desc->psy_charger_stat[i]);
		if (!curpsy) {
			dev_err(&pdev->dev, "Cannot find power supply "
					"\"%s\"\n",
					desc->psy_charger_stat[i]);
			ret = -ENODEV;
			goto err_chg_stat;
		}
		curpsy->supplied_to = cm->psy_name_ptr;
		curpsy->num_supplicants = 1;
		cm->charger_stat[i] = curpsy;
	}

	cm->fuel_gauge = power_supply_get_by_name(desc->psy_fuel_gauge);
	if (!cm->fuel_gauge) {
		dev_err(&pdev->dev, "Cannot find power supply \"%s\"\n",
				desc->psy_fuel_gauge);
		ret = -ENODEV;
		goto err_chg_stat;
	}
	cm->fuel_gauge->supplied_to = cm->psy_name_ptr;
	cm->fuel_gauge->num_supplicants = 1;

	if (!desc->slow_polling_interval)
		desc->slow_polling_interval = 1;
	if (!desc->fast_polling_interval)
		desc->fast_polling_interval = 1;

	if (!desc->temperature_out_of_range) {
		dev_err(&pdev->dev, "there is no temperature_out_of_range\n");
		ret = -EINVAL;
		goto err_chg_stat;
	}

	platform_set_drvdata(pdev, cm);

	/* Allocate for psy properties because they may vary */
	cm->charger_psy.properties = kzalloc(sizeof(enum power_supply_property)
				* (ARRAY_SIZE(default_charger_props) +
				NUM_CHARGER_PSY_OPTIONAL),
				GFP_KERNEL);
	if (!cm->charger_psy.properties) {
		dev_err(&pdev->dev, "Cannot allocate for psy properties.\n");
		ret = -ENOMEM;
		goto err_chg_stat;
	}
	memcpy(cm->charger_psy.properties, default_charger_props,
		sizeof(enum power_supply_property) *
		ARRAY_SIZE(default_charger_props));
	cm->charger_psy.num_properties = psy_default.num_properties;

	/* Find which optional psy-properties are available */
	if (!cm->fuel_gauge->get_property(cm->fuel_gauge,
					  POWER_SUPPLY_PROP_CHARGE_NOW, &val)) {
		cm->charger_psy.properties[cm->charger_psy.num_properties] =
				POWER_SUPPLY_PROP_CHARGE_NOW;
		cm->charger_psy.num_properties++;
	}
	if (!cm->fuel_gauge->get_property(cm->fuel_gauge,
					  POWER_SUPPLY_PROP_CURRENT_NOW,
					  &val)) {
		cm->charger_psy.properties[cm->charger_psy.num_properties] =
				POWER_SUPPLY_PROP_CURRENT_NOW;
		cm->charger_psy.num_properties++;
	}
	if (!desc->measure_battery_temp) {
		cm->charger_psy.properties[cm->charger_psy.num_properties] =
				POWER_SUPPLY_PROP_TEMP_AMBIENT;
		cm->charger_psy.num_properties++;
	}
	if (desc->measure_battery_temp) {
		cm->charger_psy.properties[cm->charger_psy.num_properties] =
				POWER_SUPPLY_PROP_TEMP;
		cm->charger_psy.num_properties++;
	}

	ret = power_supply_register(NULL, &cm->charger_psy);
	if (ret) {
		dev_err(&pdev->dev, "Cannot register charger-manager with"
				" name \"%s\".\n", cm->charger_psy.name);
		goto err_register;
	}

	ret = regulator_bulk_get(&pdev->dev, desc->num_charger_regulators,
				 desc->charger_regulators);
	if (ret) {
		dev_err(&pdev->dev, "Cannot get charger regulators.\n");
		goto err_bulk_get;
	}

	wake_lock_init(&cm->wakelock, WAKE_LOCK_SUSPEND, "charger-manager");

	INIT_WORK(&cm->work, cm_monitor_work);

	cm->last_poll = alarm_get_elapsed_realtime();
	android_alarm_init(&cm->alarm, ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP,
			cm_monitor_alarm);

	ret = try_charger_enable(cm, true);
	if (ret) {
		dev_err(&pdev->dev, "Cannot enable charger regulators\n");
		goto err_chg_enable;
	}

	schedule_work(&cm->work);

	return 0;

err_chg_enable:
	if (desc->charger_regulators)
		regulator_bulk_free(desc->num_charger_regulators,
					desc->charger_regulators);
err_bulk_get:
	power_supply_unregister(&cm->charger_psy);
err_register:
	kfree(cm->charger_psy.properties);
err_chg_stat:
	kfree(cm->charger_stat);
err_no_charger_stat:
err_no_charger:
	kfree(cm->desc);
err_alloc_desc:
	kfree(cm);
err_alloc:
	return ret;
}

static int __devexit charger_manager_remove(struct platform_device *pdev)
{
	struct charger_manager *cm = platform_get_drvdata(pdev);
	struct charger_desc_android *desc = cm->desc;

	if (desc->charger_regulators)
		regulator_bulk_free(desc->num_charger_regulators,
					desc->charger_regulators);

	power_supply_unregister(&cm->charger_psy);
	kfree(cm->charger_psy.properties);
	kfree(cm->charger_stat);
	kfree(cm->desc);
	kfree(cm);

	return 0;
}

static const struct platform_device_id charger_manager_id[] = {
	{ "charger-manager", 0 },
	{ },
};
MODULE_DEVICE_TABLE(platform, charger_manager_id);

static int cm_suspend_prepare(struct device *dev)
{
	struct platform_device *pdev = container_of(dev, struct platform_device,
						    dev);
	struct charger_manager *cm = platform_get_drvdata(pdev);

	if (!is_ext_pwr_online(cm)) {
		cm_setup_alarm(cm, cm->desc->slow_polling_interval);
		cm->slow_poll = true;
	}

	return 0;
}

static void cm_suspend_complete(struct device *dev)
{
	struct platform_device *pdev = container_of(dev, struct platform_device,
						    dev);
	struct charger_manager *cm = platform_get_drvdata(pdev);

	if (cm->slow_poll) {
		cm_setup_alarm(cm, cm->desc->fast_polling_interval);
		cm->slow_poll = false;
	}
}

static const struct dev_pm_ops charger_manager_pm = {
	.prepare	= cm_suspend_prepare,
	.complete	= cm_suspend_complete,
};

static struct platform_driver charger_manager_driver = {
	.driver = {
		.name = "charger-manager",
		.owner = THIS_MODULE,
		.pm = &charger_manager_pm,
	},
	.probe = charger_manager_probe,
	.remove = __devexit_p(charger_manager_remove),
	.id_table = charger_manager_id,
};

static int __init charger_manager_init(void)
{
	return platform_driver_register(&charger_manager_driver);
}
late_initcall(charger_manager_init);

static void __exit charger_manager_cleanup(void)
{
	platform_driver_unregister(&charger_manager_driver);
}
module_exit(charger_manager_cleanup);

MODULE_AUTHOR("MyungJoo Ham <myungjoo.ham@samsung.com>");
MODULE_DESCRIPTION("Charger Manager NG");
MODULE_LICENSE("GPL");
