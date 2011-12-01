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
#include <linux/input.h>

#include <mach/irqs.h>
#include <mach/gpio.h>
#include <mach/regs-gpio.h>

#include <plat/devs.h>
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

static struct platform_device *qss_devices[] __initdata = {
	&qss_keypad,
};

void __init qss_input_init(void)
{
	/* gpio cfg */
	qss_keypad_cfg_gpio();

	/* register devices */
	platform_add_devices(qss_devices, ARRAY_SIZE(qss_devices));
}
