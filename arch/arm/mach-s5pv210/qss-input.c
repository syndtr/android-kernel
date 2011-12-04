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

#include <mach/irqs.h>
#include <mach/gpio.h>
#include <mach/regs-gpio.h>

#include <plat/devs.h>
#include <plat/iic.h>
#include <plat/gpio-cfg.h>

#include <mach/qss.h>

#define KEYPAD_BUTTONS_NR	3

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
};

static struct s3c2410_platform_i2c i2c2_pdata = {
	.flags		= 0,
	.bus_num	= 2,
	.slave_addr	= 0x10,
	.frequency	= 400 * 1000,
	.sda_delay	= 100,
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

	/* i2c gpio cfg */
	s3c_i2c2_cfg_gpio(&s3c_device_i2c2);

	s3c_gpio_cfgrange_nopull(GPIO_TSP_LDO_EN, 1, S3C_GPIO_OUTPUT);
	gpio_set_value(GPIO_TSP_LDO_EN, 1);

	/* gpio interrupt */
	s3c_gpio_cfgall_range(GPIO_TSP_INT, 1, EINT_MODE, S3C_GPIO_PULL_UP);
	ret = s5p_register_gpio_interrupt(GPIO_TSP_INT);
	if (ret < 0)
		printk(KERN_ERR "qss-input: unable to register tsp interrupt\n");
	else
		i2c2_devs[0].irq = ret;

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

static struct platform_device *qss_devices[] __initdata = {
	&qss_keypad,
	&s3c_device_i2c2,
	&s3c_device_i2c10,
};

void __init qss_input_init(void)
{
	/* gpio cfg */
	qss_keypad_cfg_gpio();
	qss_tsp_cfg_gpio();
	qss_touchkey_cfg_gpio();

	/* i2c platdata */
	s3c_i2c2_set_platdata(&i2c2_pdata);

	/* register devices */
	platform_add_devices(qss_devices, ARRAY_SIZE(qss_devices));

	/* tsp */
	i2c_register_board_info(2, i2c2_devs, ARRAY_SIZE(i2c2_devs));

	/* touchkey */
	i2c_register_board_info(10, i2c10_devs, ARRAY_SIZE(i2c10_devs));
}
