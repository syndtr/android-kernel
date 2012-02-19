/* linux/arch/arm/mach-s5pv210/qss-input.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/i2c/atmel_mxt_ts.h>
#include <linux/input.h>
#include <linux/input/cypress-touchkey.h>
#include <linux/input/gp2a_samsung.h>
#include <linux/input/sec_jack.h>

#include <mach/irqs.h>
#include <mach/gpio.h>
#include <mach/regs-gpio.h>

#include <plat/devs.h>
#include <plat/iic.h>
#include <plat/regs-iic.h>
#include <plat/gpio-cfg.h>
#include <plat/adc.h>

#include <mach/qss.h>

#define KEYPAD_BUTTONS_NR	3

#define S3C_ADC_JACK		3
#define S3C_ADC_GP2A		9

/* Keypad */

static struct gpio_keys_button keypad_buttons[] = {
	{
		.code 		= KEY_POWER,
		.gpio		= GPIO_KEY_POWER,
		.desc		= "keypad-power",
		.type		= EV_KEY,
		.active_low	= 1,
		.wakeup		= 1,
		.debounce_interval = 1,
	},
	{
		.code 		= KEY_VOLUMEUP,
		.gpio		= GPIO_KEY_VOLUMEUP,
		.desc		= "keypad-volumeup",
		.type		= EV_KEY,
		.active_low	= 1,
		.wakeup		= 1,
		.debounce_interval = 1,
	},
	{
		.code 		= KEY_VOLUMEDOWN,
		.gpio		= GPIO_KEY_VOLUMEDOWN,
		.desc		= "keypad-volumedown",
		.type		= EV_KEY,
		.active_low	= 1,
		.wakeup		= 1,
		.debounce_interval = 1,
	},
};

static struct gpio_keys_platform_data keypad_pdata = {
	.name		= "qss-keypad",
	.buttons	= keypad_buttons,
	.nbuttons	= ARRAY_SIZE(keypad_buttons),
};

static struct platform_device qss_keypad = {
	.name		= "gpio-keys",
	.id		= 0,
	.dev		= {
		.platform_data	= &keypad_pdata,
	},
};

static void __init qss_keypad_cfg_gpio(void)
{
	s3c_gpio_cfgrange_nopull(GPIO_KEY_POWER, 1, EINT_MODE);
	s3c_gpio_cfgrange_nopull(GPIO_KEY_VOLUMEUP, 1, EINT_MODE);
	s3c_gpio_cfgrange_nopull(GPIO_KEY_VOLUMEDOWN, 1, EINT_MODE);
}

/* Touch Screen */

static void tsp_onoff(int onoff)
{
	gpio_direction_output(GPIO_TSP_LDO_EN, onoff);

	if (onoff)
		msleep(75);
}

static struct mxt_platform_data qt602240_pdata = {
	.x_line		= 19,
	.y_line		= 11,
	.x_size		= 800,
	.y_size		= 480,
	.blen		= 0x21,
	.threshold	= 0x28,
	.voltage	= 2800000,              /* 2.8V */
	.orient		= MXT_DIAGONAL,
	.irqflags	= IRQF_TRIGGER_FALLING,
	.onoff		= tsp_onoff,
};

static struct i2c_gpio_platform_data i2c2_gpio_pdata = {
	.sda_pin                = S5PV210_GPD1(4),
	.scl_pin                = S5PV210_GPD1(5),
	.udelay                 = 2,
	.sda_is_open_drain      = 0,
	.scl_is_open_drain      = 0,
	.scl_is_output_only     = 0,
};

static struct platform_device s3c_device_i2c2_gpio = {
	.name			= "i2c-gpio",
	.id			= 2,
	.dev.platform_data	= &i2c2_gpio_pdata,
};

static struct i2c_board_info i2c2_devs[] __initdata = {
	{
		I2C_BOARD_INFO("qt602240_ts", 0x4a),
		.platform_data		= &qt602240_pdata,
	},
};

static void __init qss_tsp_cfg_gpio(void)
{
	int ret;

	/* i2c */
	s3c_gpio_cfgall_range(S5PV210_GPD1(4), 2,
			      S3C_GPIO_SFN(2), S3C_GPIO_PULL_UP);

	/* ldo enable */
	s3c_gpio_cfgrange_nopull(GPIO_TSP_LDO_EN, 1, S3C_GPIO_OUTPUT);
	gpio_request(GPIO_TSP_LDO_EN, "TSP_LDO_EN");
	tsp_onoff(1);

	/* gpio interrupt */
	s3c_gpio_cfgall_range(GPIO_TSP_INT, 1, EINT_MODE, S3C_GPIO_PULL_UP);
	ret = s5p_register_gpio_interrupt(GPIO_TSP_INT);
	if (ret < 0)
		printk(KERN_ERR "qss-input: unable to register tsp interrupt\n");
	else
		i2c2_devs[0].irq = ret;

}

/* kr3dh accelerometer */

static struct i2c_board_info i2c5_devs[] __initdata = {
	{
		I2C_BOARD_INFO("kr3dh", 0x19),
	},
};

static void __init qss_kr3dh_cfg_gpio(void)
{
	/* i2c gpio cfg */
	s3c_i2c5_cfg_gpio(&s3c_device_i2c5);
}

/* Touchkey */

static void touchkey_onoff(int onoff)
{
	gpio_direction_output(GPIO_TOUCHKEY_EN, onoff);

	if (onoff == TOUCHKEY_OFF)
		msleep(30);
	else
		msleep(50);
}

static const int touchkey_code[] = {
	KEY_MENU,
	KEY_HOME,
	KEY_BACK,
	KEY_SEARCH
};

static struct touchkey_platform_data touchkey_pdata = {
	.keycode_cnt	= ARRAY_SIZE(touchkey_code),
	.keycode	= touchkey_code,
	.touchkey_onoff	= touchkey_onoff,
};

static struct i2c_board_info i2c10_devs[] __initdata = {
	{
		I2C_BOARD_INFO("cypress-touchkey", 0x20),
		.platform_data 	= &touchkey_pdata,
	},
};

static void __init qss_touchkey_cfg_gpio(void)
{
	int ret;

	/* i2c gpio cfg */
	s3c_i2c10_cfg_gpio(&s3c_device_i2c10);

	s3c_gpio_cfgrange_nopull(GPIO_TOUCHKEY_EN, 1, S3C_GPIO_OUTPUT);
	gpio_request(GPIO_TOUCHKEY_EN, "TOUCHKEY_EN");

	/* gpio interrupt */
	s3c_gpio_cfgrange_nopull(GPIO_TOUCHKEY_INT, 1, EINT_MODE);
	ret = s5p_register_gpio_interrupt(GPIO_TOUCHKEY_INT);
	if (ret < 0)
		printk(KERN_ERR "qss-input: unable to register touckey interrupt\n");
	else
		i2c10_devs[0].irq = ret;
}

/* gp2a light sensor */

static int gp2a_power(bool on)
{
	static bool initialized;

	if (!initialized) {
		gpio_request(GPIO_GP2A_EN, "GP2A_EN");
		gpio_direction_output(GPIO_GP2A_EN, 0);
		initialized = true;
	}

	gpio_direction_output(GPIO_GP2A_EN, on);

	return 0;
}

static int gp2a_light_adc_value(void)
{
	static struct s3c_adc_client *client;

	if (IS_ERR(client))
		return PTR_ERR(client);

	if (!client) {
		client = s3c_adc_register(&s3c_device_i2c11, NULL, NULL, 0);
		if (IS_ERR(client)) {
			pr_err("qss-input: cannot register adc for gp2a\n");
			return PTR_ERR(client);
		}
	}

	return s3c_adc_read(client, S3C_ADC_GP2A);
}

static struct gp2a_platform_data gp2a_pdata = {
        .power = gp2a_power,
        .p_out = GPIO_GP2A_PROXIMITY_INT,
        .light_adc_value = gp2a_light_adc_value,
};

static struct i2c_board_info i2c11_devs[] __initdata = {
	{
		I2C_BOARD_INFO("gp2a", (0x88 >> 1)),
		.platform_data = &gp2a_pdata,
	},
};

static void __init qss_gp2a_cfg_gpio(void)
{
	/* i2c gpio cfg */
	s3c_i2c11_cfg_gpio(&s3c_device_i2c11);

	s3c_gpio_cfgrange_nopull(GPIO_GP2A_EN, 1, S3C_GPIO_OUTPUT);
	gpio_set_value(GPIO_GP2A_EN, 0);

	s3c_gpio_cfgrange_nopull(GPIO_GP2A_PROXIMITY_INT, 1, EINT_MODE);
}

/* yas529 geomagnetic sensor */

static struct i2c_board_info i2c12_devs[] __initdata = {
	{
		I2C_BOARD_INFO("yas529", 0x2e),
	},
};

static void __init qss_yas529_cfg_gpio(void)
{
	/* i2c gpio cfg */
	s3c_i2c12_cfg_gpio(&s3c_device_i2c12);

	s3c_gpio_cfgrange_nopull(GPIO_MSENSE_EN, 1, S3C_GPIO_OUTPUT);
	gpio_set_value(GPIO_MSENSE_EN, 1);
}

/* sec jack */

static struct platform_device qss_sec_jack;

static void sec_jack_set_micbias_state(bool on)
{
	qss_set_micbias_ear(on);
}

static struct sec_jack_zone sec_jack_zones[] = {
	{
		/* adc < 50, unstable zone, default to 3pole if it stays
		* in this range for a half second (20ms delays, 25 samples)
		*/
		.adc_high = 50,
		.delay_ms = 20,
		.check_count = 25,
		.jack_type = SEC_HEADSET_3POLE,
	},
	{
		/* 50 < adc <= 490, unstable zone, default to 3pole if it stays
		* in this range for a second (10ms delays, 100 samples)
		*/
		.adc_high = 490,
		.delay_ms = 10,
		.check_count = 100,
		.jack_type = SEC_HEADSET_3POLE,
	},
	{
		/* 490 < adc <= 900, unstable zone, default to 4pole if it
		* stays in this range for a second (10ms delays, 100 samples)
		*/
		.adc_high = 900,
		.delay_ms = 10,
		.check_count = 100,
		.jack_type = SEC_HEADSET_4POLE,
	},
	{
		/* 900 < adc <= 1500, 4 pole zone, default to 4pole if it
		* stays in this range for 200ms (20ms delays, 10 samples)
		*/
		.adc_high = 1500,
		.delay_ms = 20,
		.check_count = 10,
		.jack_type = SEC_HEADSET_4POLE,
	},
	{
		/* adc > 1500, unstable zone, default to 3pole if it stays
		* in this range for a second (10ms delays, 100 samples)
		*/
		.adc_high = 0x7fffffff,
		.delay_ms = 10,
		.check_count = 100,
		.jack_type = SEC_HEADSET_3POLE,
	},
};

/* To support 3-buttons earjack */
static struct sec_jack_buttons_zone sec_jack_buttons_zones[] = {
	{
		/* 0 <= adc <= 93, stable zone */
		.code           = KEY_MEDIA,
		.adc_low        = 0,
		.adc_high       = 93,
	},
	{
		/* 94 <= adc <= 167, stable zone */
		.code           = KEY_PREVIOUSSONG,
		.adc_low        = 94,
		.adc_high       = 167,
	},
	{
		/* 168 <= adc <= 370, stable zone */
		.code           = KEY_NEXTSONG,
		.adc_low        = 168,
		.adc_high       = 370,
	},
};

static int sec_jack_get_adc_value(void)
{
	static struct s3c_adc_client *client;

	if (IS_ERR(client))
		return PTR_ERR(client);

	if (!client) {
		client = s3c_adc_register(&qss_sec_jack, NULL, NULL, 0);
		if (IS_ERR(client)) {
			pr_err("qss-input: cannot register adc for sec_jack\n");
			return PTR_ERR(client);
		}
	}

	return (1800 * s3c_adc_read(client, S3C_ADC_JACK)) / 1024;
}

struct sec_jack_platform_data sec_jack_pdata = {
	.set_micbias_state	= sec_jack_set_micbias_state,
	.get_adc_value		= sec_jack_get_adc_value,
	.zones			= sec_jack_zones,
	.num_zones		= ARRAY_SIZE(sec_jack_zones),
	.buttons_zones		= sec_jack_buttons_zones,
	.num_buttons_zones	= ARRAY_SIZE(sec_jack_buttons_zones),
	.det_gpio		= GPIO_JACK_INT,
	.send_end_gpio		= GPIO_SEND_END_INT,
};

static struct platform_device qss_jack = {
	.name	= "sec_jack",
	.id	= 1, /* will be used also for gpio_event id */
	.dev	= {
		.platform_data	= &sec_jack_pdata,
	},
};

static void __init qss_jack_cfg_gpio(void)
{
	/* gpio interrupt */
	s3c_gpio_cfgrange_nopull(GPIO_JACK_INT, 1, EINT_MODE);
	s3c_gpio_cfgrange_nopull(GPIO_SEND_END_INT, 1, EINT_MODE);
}


static struct platform_device *qss_devices[] __initdata = {
	&qss_keypad,
	&qss_jack,
	&s3c_device_i2c2_gpio,
	&s3c_device_i2c5,
	&s3c_device_i2c10,
	&s3c_device_i2c11,
	&s3c_device_i2c12,
};

void __init qss_input_init(void)
{
	/* gpio cfg */
	qss_keypad_cfg_gpio();
	qss_tsp_cfg_gpio();
	qss_kr3dh_cfg_gpio();
	qss_touchkey_cfg_gpio();
	qss_gp2a_cfg_gpio();
	qss_yas529_cfg_gpio();
	qss_jack_cfg_gpio();

	/* i2c platdata */
	//s3c_i2c2_set_platdata(&i2c2_pdata);

	/* register devices */
	platform_add_devices(qss_devices, ARRAY_SIZE(qss_devices));

	/* tsp */
	i2c_register_board_info(2, i2c2_devs, ARRAY_SIZE(i2c2_devs));

	/* kr3dh */
	i2c_register_board_info(5, i2c5_devs, ARRAY_SIZE(i2c5_devs));

	/* touchkey */
	i2c_register_board_info(10, i2c10_devs, ARRAY_SIZE(i2c10_devs));

	/* gp2a */
	i2c_register_board_info(11, i2c11_devs, ARRAY_SIZE(i2c11_devs));

	/* yas529 */
	i2c_register_board_info(12, i2c12_devs, ARRAY_SIZE(i2c12_devs));
}
