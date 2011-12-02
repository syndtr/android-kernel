/* linux/arch/arm/mach-s5pv210/qss-sdhci.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/irq.h>

#include <mach/irqs.h>
#include <mach/gpio.h>
#include <mach/regs-gpio.h>

#include <plat/devs.h>
#include <plat/sdhci.h>
#include <plat/gpio-cfg.h>

#include <mach/qss.h>

void qss_setup_sdhci0_cfg_gpio(struct platform_device *dev, int width)
{
	s5pv210_setup_sdhci0_cfg_gpio(dev, width);

	s3c_gpio_cfgrange_nopull(GPIO_MASSMEMORY_EN, 1, S3C_GPIO_OUTPUT);
	gpio_set_value(GPIO_MASSMEMORY_EN, 1);
}

struct s3c_sdhci_platdata hsmmc0_platdata = {
	.cd_type		= S3C_SDHCI_CD_PERMANENT,
	.cfg_gpio		= qss_setup_sdhci0_cfg_gpio,
	.card_is_builtin	= true,
};

struct s3c_sdhci_platdata hsmmc1_platdata = {
	.cd_type		= S3C_SDHCI_CD_INTERNAL,
};

void qss_setup_sdhci2_cfg_gpio(struct platform_device *dev, int width)
{
	s5pv210_setup_sdhci2_cfg_gpio(dev, width);

	s3c_gpio_cfgrange_nopull(GPIO_SDHCI2_CD_INT, 1, EINT_MODE);
}

struct s3c_sdhci_platdata hsmmc2_platdata = {
	.cd_type		= S3C_SDHCI_CD_GPIO,
	.ext_cd_gpio		= GPIO_SDHCI2_CD_INT,
	.ext_cd_gpio_invert	= true,
	.cfg_gpio		= qss_setup_sdhci2_cfg_gpio,
};

static struct platform_device *qss_devices[] __initdata = {
	&s3c_device_hsmmc0,
	&s3c_device_hsmmc1,
	&s3c_device_hsmmc2,
};

void __init qss_sdhci_init(void)
{
	/* hsmmc0 */
	s5pv210_default_sdhci0();
	s3c_sdhci0_set_platdata(&hsmmc0_platdata);

	/* hsmmc1 */
	s5pv210_default_sdhci1();
	s3c_sdhci1_set_platdata(&hsmmc1_platdata);

	/* hsmmc2 */
	s5pv210_default_sdhci2();
	s3c_sdhci2_set_platdata(&hsmmc2_platdata);

	/* register devices */
	platform_add_devices(qss_devices, ARRAY_SIZE(qss_devices));
}
