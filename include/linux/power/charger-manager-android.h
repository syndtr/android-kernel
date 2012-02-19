/*
 * Copyright (C) 2011 Samsung Electronics Co., Ltd.
 * MyungJoo.Ham <myungjoo.ham@samsung.com>
 *
 * Charger Manager.
 * This framework enables to control and multiple chargers and to
 * monitor charging even in the context of suspend-to-RAM with
 * an interface combining the chargers.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
**/

#ifndef _CHARGER_MANAGER_ANDROID_H
#define _CHARGER_MANAGER_ANDROID_H

#include <linux/power_supply.h>

enum data_source {
	CM_FUEL_GAUGE,
	CM_CHARGER_STAT,
};

/**
 * struct charger_desc_android
 * @psy_name: the name of power-supply-class for charger manager
 * @fullbatt_uV: voltage in microvolt
 *	If it is not being charged and VBATT >= fullbatt_uV,
 *	it is assumed to be full.
 * @slow_polling_interval: slowest interval in second at which
 *	charger manager will monitor battery health
 * @fast_polling_interval: fastest interval in second at which
 *	charger manager will monitor battery health
 * @battery_present:
 *	Specify where information for existance of battery can be obtained
 * @psy_charger_stat: the names of power-supply for chargers
 * @num_charger_regulator: the number of entries in charger_regulators
 * @charger_regulators: array of regulator_bulk_data for chargers
 * @psy_fuel_gauge: the name of power-supply for fuel gauge
 * @temperature_out_of_range:
 *	Determine whether the status is overheat or cold or normal.
 *	return_value > 0: overheat
 *	return_value == 0: normal
 *	return_value < 0: cold
 * @measure_battery_temp:
 *	true: measure battery temperature
 *	false: measure ambient temperature
 */
struct charger_desc_android {
	char *psy_name;

	unsigned int slow_polling_interval;
	unsigned int fast_polling_interval;

	unsigned int fullbatt_uV;

	enum data_source battery_present;

	char **psy_charger_stat;

	int num_charger_regulators;
	struct regulator_bulk_data *charger_regulators;

	char *psy_fuel_gauge;

	int (*temperature_out_of_range)(int *mC);
	bool measure_battery_temp;
};

#endif /* _CHARGER_MANAGER_ANDROID_H */
