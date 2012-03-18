/* linux/arch/arm/mach-s5pv210/mach-qss.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/serial_core.h>
#include <linux/gpio.h>

#include <asm/hardware/vic.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/setup.h>
#include <asm/mach-types.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/gpio.h>

#include <plat/regs-serial.h>
#include <plat/devs.h>
#include <plat/cpu.h>
#include <plat/s5p-time.h>
#include <plat/gpio-cfg.h>
#include <plat/udc-hs.h>

#include <mach/qss.h>

#include "common.h"

#include <plat/clock.h>

/* Following are default values for UCON, ULCON and UFCON UART registers */
#define GONI_UCON_DEFAULT	(S3C2410_UCON_TXILEVEL |	\
				 S3C2410_UCON_RXILEVEL |	\
				 S3C2410_UCON_TXIRQMODE |	\
				 S3C2410_UCON_RXIRQMODE |	\
				 S3C2410_UCON_RXFIFO_TOI |	\
				 S3C2443_UCON_RXERR_IRQEN)

#define GONI_ULCON_DEFAULT	S3C2410_LCON_CS8

#define GONI_UFCON_DEFAULT	S3C2410_UFCON_FIFOMODE

static struct s3c2410_uartcfg qss_uartcfgs[] __initdata = {
	{
		.hwport		= 0,
		.flags		= 0,
		.ucon		= GONI_UCON_DEFAULT,
		.ulcon		= GONI_ULCON_DEFAULT,
		.ufcon		= GONI_UFCON_DEFAULT |
			S5PV210_UFCON_TXTRIG256 | S5PV210_UFCON_RXTRIG256,
	},
	{
		.hwport		= 1,
		.flags		= 0,
		.ucon		= GONI_UCON_DEFAULT,
		.ulcon		= GONI_ULCON_DEFAULT,
		.ufcon		= GONI_UFCON_DEFAULT |
			S5PV210_UFCON_TXTRIG64 | S5PV210_UFCON_RXTRIG64,
	},
#ifndef CONFIG_FIQ_DEBUGGER
	{
		.hwport		= 2,
		.flags		= 0,
		.ucon		= GONI_UCON_DEFAULT,
		.ulcon		= GONI_ULCON_DEFAULT,
		.ufcon		= GONI_UFCON_DEFAULT |
			S5PV210_UFCON_TXTRIG16 | S5PV210_UFCON_RXTRIG16,
	},
#endif
	{
		.hwport		= 3,
		.flags		= 0,
		.ucon		= GONI_UCON_DEFAULT,
		.ulcon		= GONI_ULCON_DEFAULT,
		.ufcon		= GONI_UFCON_DEFAULT |
			S5PV210_UFCON_TXTRIG16 | S5PV210_UFCON_RXTRIG16,
	},
};

static struct resource ramconsole_resources[] = {
	[0] = DEFINE_RES_MEM(QSS_PA_RAMCONSOLE, SZ_1M),
};

static struct platform_device qss_ramconsole = {
	.name		= "ram_console",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(ramconsole_resources),
	.resource	= ramconsole_resources,
};

static struct platform_device *qss_devices[] __initdata = {
	&qss_ramconsole,
	&s3c_device_adc,
	&s3c_device_rtc,
	&s3c_device_wdt,
};

static void __init qss_fixup(struct tag *tag, char **cmdline,
		struct meminfo *mi)
{
	mi->bank[0].start = QSS_MEMBANK0_START;
	mi->bank[0].size = QSS_MEMBANK0_SIZE;
	mi->bank[1].start = QSS_MEMBANK1_START;
	mi->bank[1].size = QSS_MEMBANK1_SIZE;
	mi->nr_banks = 2;
}

static void __init qss_map_io(void)
{
	s5pv210_init_io(NULL, 0);
	s3c24xx_init_clocks(24000000);
	s3c24xx_init_uarts(qss_uartcfgs, ARRAY_SIZE(qss_uartcfgs));
	s5p_set_timer_source(S5P_PWM3, S5P_PWM4);
}

static int __hwrev = 0;
static void __init qss_hwrev_init(void)
{
	int i;

	s3c_gpio_cfgrange_nopull(GPIO_HWREV_MODE0, 3, S3C_GPIO_INPUT);
	for (i = 0; i < 3; i++) {
		__hwrev |= (gpio_get_value(GPIO_HWREV_MODE0 + i) << i);
	}

	s3c_gpio_cfgrange_nopull(GPIO_HWREV_MODE3, 1, S3C_GPIO_INPUT);
	__hwrev |= (gpio_get_value(GPIO_HWREV_MODE3) << i);

	printk("QSS HW Rev: %d\n", __hwrev);
}

int qss_hwrev(void)
{
	return __hwrev;
}

static struct s3c_hsotg_plat qss_hsotg_pdata;

static void __init qss_machine_init(void)
{
	/* get hw rev */
	qss_hwrev_init();

	/* initialize pm */
	qss_pm_init();

	/* initialize gpios */
	qss_gpio_init();

	/* FIQ debugger */
#ifdef CONFIG_FIQ_DEBUGGER
	qss_fiqdbg_init();
#endif

	/* MFD */
	qss_mfd_init();

	/* Power */
	qss_power_init();

	platform_add_devices(qss_devices, ARRAY_SIZE(qss_devices));

	/* Input */
	qss_input_init();

	/* Switch */
	qss_switch_init();

	/* SDHCI */
	qss_sdhci_init();
	
	/* OneNAND */
	qss_onenand_init();

	/* Display */
	qss_display_init();

	/* Media */
	qss_media_init();

	/* Wifi */
	qss_wifi_init();

	/* Sound */
	qss_sound_init();

	/* Phone */
	qss_phone_init();
	
	/* USB */
	clk_xusbxti.rate = 24000000;
	s3c_hsotg_set_platdata(&qss_hsotg_pdata);
	platform_device_register(&s3c_device_usb_hsotg);
}

MACHINE_START(QSS, "QSS")
	.atag_offset	= 0x100,
	.fixup		= qss_fixup,
	.init_irq	= s5pv210_init_irq,
	.handle_irq	= vic_handle_irq,
	.map_io		= qss_map_io,
	.init_machine	= qss_machine_init,
	.timer		= &s5p_timer,
	.restart	= qss_restart,
MACHINE_END
