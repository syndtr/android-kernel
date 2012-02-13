/* linux/arch/arm/mach-s5pv210/qss-switch.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/device.h>
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

static struct gpio_switch_platform_data uart_switch_pdata = {
	.name		= "uart",
	.gpio		= GPIO_UART_SEL,
	.direction	= SWITCH_GPIO_OUTPUT,
	.value		= 1,
};

static struct platform_device qss_device_uart_switch = {
	.name		= "switch-gpio",
	.id		= -1,
	.dev	= {
		.platform_data	= &uart_switch_pdata,
	},
};

static void __init qss_switch_cfg_gpio(void)
{
	s3c_i2c7_cfg_gpio(&s3c_device_i2c7);

	s3c_gpio_cfgrange_nopull(GPIO_USB_SEL, 1, S3C_GPIO_OUTPUT);

	s3c_gpio_cfgrange_nopull(GPIO_UART_SEL, 1, S3C_GPIO_OUTPUT);
	gpio_set_value(GPIO_UART_SEL, 1);

	s3c_gpio_cfgrange_nopull(GPIO_SWITCH_INT, 1, EINT_MODE);
	i2c7_devs[0].irq = gpio_to_irq(GPIO_SWITCH_INT);
}

static struct platform_device *qss_devices[] __initdata = {
	&s3c_device_i2c7,
	&qss_device_uart_switch,
};

static void __init qss_switch_gpio_export(void)
{
	struct class *class;
	struct device *dev;
	int ret;

	class = class_create(THIS_MODULE, "sec");
	if (IS_ERR(class)) {
		pr_err("Failed to create class 'sec': %ld\n",
		       PTR_ERR(class));
		return;
	}

	dev = device_create(class, NULL, 0, NULL, "uart_switch");
	if (IS_ERR(dev)) {
		pr_err("Failed to create device 'uart_switch': %ld\n",
		       PTR_ERR(dev));
		return;
	}

	ret = gpio_request(GPIO_UART_SEL, "UART_SEL");
	if (ret < 0) {
		pr_err("Failed to request gpio %d: %d\n", GPIO_UART_SEL, ret);
		return;
	}
	gpio_direction_output(GPIO_UART_SEL, 1);
	gpio_export(GPIO_UART_SEL, 1);
	gpio_export_link(dev, "UART_SEL", GPIO_UART_SEL);
}

void __init qss_switch_init(void)
{
	/* switch gpio config */
	qss_switch_cfg_gpio();

	/* register devices */
	platform_add_devices(qss_devices, ARRAY_SIZE(qss_devices));

	/* register fsa9480 */
	i2c_register_board_info(7, i2c7_devs, ARRAY_SIZE(i2c7_devs));
	
	/* export gpio */
	qss_switch_gpio_export();
}
