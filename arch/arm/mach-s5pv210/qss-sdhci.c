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
#include <linux/delay.h>
#include <linux/switch.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdhci.h>
#include <linux/mmc/pm.h>

#include <mach/irqs.h>
#include <mach/gpio.h>
#include <mach/regs-gpio.h>

#include <plat/devs.h>
#include <plat/sdhci.h>
#include <plat/gpio-cfg.h>
#include <plat/regs-sdhci.h>

#include <mach/qss.h>

#define S3C_SDHCI_CTRL3_FCSELTX_INVERT  (0)
#define S3C_SDHCI_CTRL3_FCSELTX_BASIC \
	(S3C_SDHCI_CTRL3_FCSEL3 | S3C_SDHCI_CTRL3_FCSEL2)
#define S3C_SDHCI_CTRL3_FCSELRX_INVERT  (0)
#define S3C_SDHCI_CTRL3_FCSELRX_BASIC \
	(S3C_SDHCI_CTRL3_FCSEL1 | S3C_SDHCI_CTRL3_FCSEL0)

void qss_setup_sdhci_cfg_card(struct platform_device *dev,
				    void __iomem *r,
				    struct mmc_ios *ios,
				    struct mmc_card *card)
{
	u32 ctrl2;
	u32 ctrl3;

	ctrl2 = readl(r + S3C_SDHCI_CONTROL2);
	ctrl2 &= S3C_SDHCI_CTRL2_SELBASECLK_MASK;
	ctrl2 |= (S3C64XX_SDHCI_CTRL2_ENSTAASYNCCLR |
		  S3C64XX_SDHCI_CTRL2_ENCMDCNFMSK |
		  S3C_SDHCI_CTRL2_DFCNT_NONE |
		  S3C_SDHCI_CTRL2_ENCLKOUTHOLD);

	if (ios->clock <= (400 * 1000)) {
		ctrl2 &= ~(S3C_SDHCI_CTRL2_ENFBCLKTX |
			   S3C_SDHCI_CTRL2_ENFBCLKRX);
		ctrl3 = 0;
	} else {
		u32 range_start;
		u32 range_end;

		ctrl2 |= S3C_SDHCI_CTRL2_ENFBCLKTX |
			 S3C_SDHCI_CTRL2_ENFBCLKRX;

		if (card->type == MMC_TYPE_MMC)  /* MMC */
			range_start = 20 * 1000 * 1000;
		else    /* SD, SDIO */
			range_start = 25 * 1000 * 1000;

		range_end = 37 * 1000 * 1000;

		if ((ios->clock > range_start) && (ios->clock < range_end))
			ctrl3 = S3C_SDHCI_CTRL3_FCSELTX_BASIC |
				S3C_SDHCI_CTRL3_FCSELRX_BASIC;
		else
			ctrl3 = S3C_SDHCI_CTRL3_FCSELTX_BASIC |
				S3C_SDHCI_CTRL3_FCSELRX_INVERT;
	}


	writel(ctrl2, r + S3C_SDHCI_CONTROL2);
	writel(ctrl3, r + S3C_SDHCI_CONTROL3);
}

void qss_setup_sdhci0_cfg_gpio(struct platform_device *dev, int width)
{
	s5pv210_setup_sdhci0_cfg_gpio(dev, width);

	s3c_gpio_cfgrange_nopull(GPIO_MASSMEMORY_EN, 1, S3C_GPIO_OUTPUT);
	gpio_set_value(GPIO_MASSMEMORY_EN, 1);
}

struct s3c_sdhci_platdata hsmmc0_platdata = {
	.cd_type		= S3C_SDHCI_CD_PERMANENT,
	.cfg_gpio		= qss_setup_sdhci0_cfg_gpio,
	.cfg_card		= qss_setup_sdhci_cfg_card,
	.host_quirks		= SDHCI_QUIRK_BROKEN_TIMEOUT_VAL,
};

void (*qss_sdhci1_notify_func)(struct platform_device *, int);

int qss_sdhci1_ext_cd_init(void (*notify_func)(struct platform_device *, int))
{
	qss_sdhci1_notify_func = notify_func;
	return 0;
}

struct s3c_sdhci_platdata hsmmc1_platdata = {
	.cd_type		= S3C_SDHCI_CD_EXTERNAL,
	.ext_cd_init		= qss_sdhci1_ext_cd_init,
	.cfg_card		= qss_setup_sdhci_cfg_card,
	.host_quirks		= SDHCI_QUIRK_BROKEN_TIMEOUT_VAL,
	.pm_caps		= MMC_PM_KEEP_POWER | MMC_PM_IGNORE_PM_NOTIFY |
				  MMC_PM_SDHCI_SKIP_PM,
	.pm_flags		= MMC_PM_KEEP_POWER | MMC_PM_IGNORE_PM_NOTIFY |
				  MMC_PM_SDHCI_SKIP_PM,
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
	.cfg_card		= qss_setup_sdhci_cfg_card,
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
