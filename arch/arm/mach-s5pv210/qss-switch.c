/* linux/arch/arm/mach-s5pv210/qss-switch.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/platform_data/fsa9480.h>
#include <linux/switch.h>

#include <mach/irqs.h>
#include <mach/gpio.h>
#include <mach/regs-gpio.h>

#include <plat/devs.h>
#include <plat/gpio-cfg.h>
#include <plat/iic.h>

#include <mach/qss.h>

static struct fsa9480_platform_data fsa9480_pdata = {
	.wakeup	= true,
};

static struct i2c_board_info i2c7_devs[] __initdata = {
	{
		I2C_BOARD_INFO("fsa9480", (0x4A >> 1)),
		.platform_data = &fsa9480_pdata,
	},
};

static void __init qss_switch_cfg_gpio(void)
{
	s3c_gpio_cfgrange_nopull(GPIO_USB_SEL, 1, S3C_GPIO_OUTPUT);

	s3c_gpio_cfgrange_nopull(GPIO_UART_SEL, 1, S3C_GPIO_OUTPUT);
	gpio_set_value(GPIO_UART_SEL, 1);

	s3c_gpio_cfgrange_nopull(GPIO_SWITCH_INT, 1, EINT_MODE);
	i2c7_devs[0].irq = gpio_to_irq(GPIO_SWITCH_INT);
}

void __init qss_switch_init(void)
{
	/* i2c gpio cfg */
	s3c_i2c6_cfg_gpio(&s3c_device_i2c7);

	/* register device */
	platform_device_register(&s3c_device_i2c7);

	/* switch gpio config */
	qss_switch_cfg_gpio();

	/* register fsa9480 */
	i2c_register_board_info(7, i2c7_devs, ARRAY_SIZE(i2c7_devs));
}
