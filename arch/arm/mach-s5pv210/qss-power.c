/* linux/arch/arm/mach-s5pv210/qss-mfd.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/max17040_battery.h>
#include <linux/power/charger-manager-android.h>

#include <mach/gpio.h>

#include <plat/devs.h>
#include <plat/iic.h>
#include <plat/adc.h>

#include <mach/qss.h>

#define ADC_NUM_SAMPLES		5
#define ADC_LIMIT_ERR_COUNT	5

#define S3C_ADC_TEMPERATURE	6

#define HIGH_BLOCK_TEMP		500
#define HIGH_RECOVER_TEMP	420
#define LOW_BLOCK_TEMP		0
#define LOW_RECOVER_TEMP	20

/**
** temp_adc_table_data
** @adc_value : thermistor adc value
** @temperature : temperature(C) * 10
**/
struct temp_adc_table_data {
	int adc_value;
	int temperature;
};

static struct temp_adc_table_data qss_temper[] = {
	/* ADC, Temperature (C/10) */
	{  222,		700	},
	{  230,		690	},
	{  238,		680	},
	{  245,		670	},
	{  253,		660	},
	{  261,		650	},
	{  274,		640	},
	{  287,		630	},
	{  300,		620	},
	{  314,		610	},
	{  327,		600	},
	{  339,		590	},
	{  350,		580	},
	{  362,		570	},
	{  374,		560	},
	{  386,		550	},
	{  401,		540	},
	{  415,		530	},
	{  430,		520	},
	{  444,		510	},
	{  459,		500	},
	{  477,		490	},
	{  495,		480	},
	{  513,		470	},
	{  526,		460	},
	{  539,		450	},
	{  559,		440	},
	{  580,		430	},
	{  600,		420	},
	{  618,		410	},
	{  642,		400	},
	{  649,		390	},
	{  674,		380	},
	{  695,		370	},
	{  717,		360	},
	{  739,		350	},
	{  760,		340	},
	{  782,		330	},
	{  803,		320	},
	{  825,		310	},
	{  847,		300	},
	{  870,		290	},
	{  894,		280	},
	{  918,		270	},
	{  942,		260	},
	{  966,		250	},
	{  990,		240	},
	{ 1014,		230	},
	{ 1038,		220	},
	{ 1062,		210	},
	{ 1086,		200	},
	{ 1110,		190	},
	{ 1134,		180	},
	{ 1158,		170	},
	{ 1182,		160	},
	{ 1206,		150	},
	{ 1228,		140	},
	{ 1251,		130	},
	{ 1274,		120	},
	{ 1297,		110	},
	{ 1320,		100	},
	{ 1341,		90	},
	{ 1362,		80	},
	{ 1384,		70	},
	{ 1405,		60	},
	{ 1427,		50	},
	{ 1450,		40	},
	{ 1474,		30	},
	{ 1498,		20	},
	{ 1514,		10	},
	{ 1533,		0	},
	{ 1544,		(-10)	},
	{ 1567,		(-20)	},
	{ 1585,		(-30)	},
	{ 1604,		(-40)	},
	{ 1623,		(-50)	},
	{ 1641,		(-60)	},
	{ 1659,		(-70)	},
	{ 1678,		(-80)	},
	{ 1697,		(-90)	},
	{ 1715,		(-100)	},
};

static int s3c_get_adc_data(int ch)
{
	int adc_data;
	int adc_max = -1;
	int adc_min = 1 << 11;
	int adc_total = 0;
	int i, j;
	static struct s3c_adc_client *client;

	if (IS_ERR(client))
		return PTR_ERR(client);
	
	if (!client) {
		client = s3c_adc_register(&s3c_device_i2c9, NULL, NULL, 0);
		if (IS_ERR(client)) {
			pr_err("qss-power: cannot register adc\n");
			return PTR_ERR(client);
		}
	}
	
	for (i = 0; i < ADC_NUM_SAMPLES; i++) {
		adc_data = s3c_adc_read(client, ch);
		if (adc_data == -EAGAIN) {
			for (j = 0; j < ADC_LIMIT_ERR_COUNT; j++) {
				msleep(20);
				adc_data = s3c_adc_read(client, ch);
				if (adc_data > 0)
					break;
			}
			if (j >= ADC_LIMIT_ERR_COUNT) {
				pr_err("%s: Retry count exceeded[ch:%d]\n",
					__func__, ch);
				return adc_data;
			}
		} else if (adc_data < 0) {
			pr_err("%s: Failed read adc value : %d [ch:%d]\n",
				__func__, adc_data, ch);
			return adc_data;
		}

		if (adc_data > adc_max)
			adc_max = adc_data;
		if (adc_data < adc_min)
			adc_min = adc_data;

		adc_total += adc_data;
	}
	return (adc_total - adc_max - adc_min) / (ADC_NUM_SAMPLES - 2);
}

static int qss_battery_temp(void)
{
	int array_size = ARRAY_SIZE(qss_temper);
	int temp_adc = s3c_get_adc_data(S3C_ADC_TEMPERATURE);
	int mid;
	int left_side = 0;
	int right_side = array_size - 1;
	int temp = 0;

	if (temp_adc < 0) {
		pr_err("%s : Invalid temperature adc value [%d]\n",
			__func__, temp_adc);
		return temp_adc;
	}

	while (left_side <= right_side) {
		mid = (left_side + right_side) / 2;
		if (mid == 0 || mid == array_size - 1 ||
				(qss_temper[mid].adc_value <= temp_adc &&
				 qss_temper[mid+1].adc_value > temp_adc)) {
			temp = qss_temper[mid].temperature;
			break;
		} else if (temp_adc - qss_temper[mid].adc_value > 0) {
			left_side = mid + 1;
		} else {
			right_side = mid - 1;
		}
	}

	return temp;
}

static struct max17040_platform_data max17040_pdata = {
	.rcomp			= 0xD700,
	.skip_reset		= true,
};

static struct i2c_board_info i2c9_devs[] __initdata = {
	{
		I2C_BOARD_INFO("max17040", (0x6D >> 1)),
		.platform_data = &max17040_pdata,
	},
};

static char *charger_src[] = {
	"max8998_ac",
	"max8998_usb",
	NULL,
};

static struct regulator_bulk_data charger_regulators[] = {
	{ .supply = "charger", },
};

int cm_battery_temp(int *mC)
{
	int temp = qss_battery_temp();
	int ret = 0;

	if (temp >= HIGH_BLOCK_TEMP) {
		ret = 1;
	} else if (temp <= HIGH_RECOVER_TEMP && temp >= LOW_RECOVER_TEMP) {
		ret = 0;
	} else if (temp <= LOW_BLOCK_TEMP) {
		ret = -1;
	}

	*mC = temp*100;

	return ret;
}

static struct charger_desc_android charger_pdata = {
	.psy_name		= "battery",
	.slow_polling_interval	= 60 * 10,
	.fast_polling_interval	= 60 * 1,
	.fullbatt_uV		= 4150000,
	.battery_present	= CM_FUEL_GAUGE,
	.psy_charger_stat	= charger_src,
	.psy_fuel_gauge		= "max17040_battery",
	.charger_regulators	= charger_regulators,
	.num_charger_regulators	= ARRAY_SIZE(charger_regulators),
	.temperature_out_of_range = cm_battery_temp,
	.measure_battery_temp	= true,
	
	
};

static struct platform_device qss_charger = {
	.name		= "charger-manager",
	.id		= -1,
	.dev = {
		.platform_data = &charger_pdata,
	}
};

static struct platform_device *qss_devices[] __initdata = {
	&s3c_device_i2c9,
	&qss_charger,
};

void __init qss_power_init(void)
{
	/* i2c gpio cfg */
	s3c_i2c9_cfg_gpio(&s3c_device_i2c9);

	/* max17040 */
	i2c_register_board_info(9, i2c9_devs, ARRAY_SIZE(i2c9_devs));

	/* register devices */
	platform_add_devices(qss_devices, ARRAY_SIZE(qss_devices));
}