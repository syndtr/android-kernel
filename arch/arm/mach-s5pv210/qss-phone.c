/* linux/arch/arm/mach-s5pv210/qss-phone.c
 *
 * Copyright (C) 2010 Samsung Electronics. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/irq.h>

#include <mach/irqs.h>
#include <mach/gpio.h>
#include <mach/regs-gpio.h>

#include <plat/gpio-cfg.h>

#include <mach/qss.h>

#include "../../../drivers/misc/samsung_modemctl/modem_ctl.h"

/* Modem control */
static struct modemctl_data mdmctl_data = {
	.name = "xmm",
	.gpio_phone_active = GPIO_PHONE_ACTIVE,
	.gpio_pda_active = GPIO_PDA_ACTIVE,
	.gpio_cp_reset = GPIO_CP_RST,
	.gpio_phone_on = GPIO_PHONE_ON,
	.is_cdma_modem = 1,
};

static struct resource mdmctl_res[] = {
	[0] = {
		.name = "active",
		.flags = IORESOURCE_IRQ,
	},
	[1] = {
		.name = "onedram",
		.flags = IORESOURCE_IRQ,
	},
	[2] = {
		.name = "onedram",
		.start = (S5PV210_PA_SDRAM + 0x05000000),
		.end = (S5PV210_PA_SDRAM + 0x05000000 + SZ_16M - 1),
		.flags = IORESOURCE_MEM,
	},
};

static struct platform_device modemctl = {
	.name = "modemctl",
	.id = -1,
	.num_resources = ARRAY_SIZE(mdmctl_res),
	.resource = mdmctl_res,
	.dev = {
		.platform_data = &mdmctl_data,
	},
};

void __init qss_phone_init(void)
{
	int irq;

	s3c_gpio_cfgrange_nopull(GPIO_PDA_ACTIVE, 1, S3C_GPIO_OUTPUT);
	s3c_gpio_cfgrange_nopull(GPIO_PHONE_ON, 1, S3C_GPIO_OUTPUT);

	s3c_gpio_cfgrange_nopull(GPIO_PHONE_ACTIVE, 1, EINT_MODE);
	irq = gpio_to_irq(GPIO_PHONE_ACTIVE);
	if (irq < 0) {
		pr_err("Cannot get irq for phone active\n");
		return -1;
	}
	mdmctl_res[0].start = mdmctl_res[0].end = irq;

	s3c_gpio_cfgrange_nopull(GPIO_ONEDRAM_INT, 1, EINT_MODE);
	irq = gpio_to_irq(GPIO_ONEDRAM_INT);
	if (irq < 0) {
		pr_err("Cannot get irq for onedram\n");
		return -1;
	}
	mdmctl_res[1].start = mdmctl_res[1].end = irq;

	platform_device_register(&modemctl);
}
