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
#include <linux/sysdev.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/setup.h>
#include <asm/mach-types.h>

#include <mach/map.h>
#include <mach/regs-clock.h>

#include <plat/regs-serial.h>
#include <plat/s5pv210.h>
#include <plat/devs.h>
#include <plat/cpu.h>
#include <plat/pm.h>
#include <plat/s5p-time.h>

#include <mach/qss.h>

/* Following are default values for UCON, ULCON and UFCON UART registers */
#define SMDKC110_UCON_DEFAULT	(S3C2410_UCON_TXILEVEL |	\
				 S3C2410_UCON_RXILEVEL |	\
				 S3C2410_UCON_TXIRQMODE |	\
				 S3C2410_UCON_RXIRQMODE |	\
				 S3C2410_UCON_RXFIFO_TOI |	\
				 S3C2443_UCON_RXERR_IRQEN)

#define SMDKC110_ULCON_DEFAULT	S3C2410_LCON_CS8

#define SMDKC110_UFCON_DEFAULT	(S3C2410_UFCON_FIFOMODE |	\
				 S5PV210_UFCON_TXTRIG4 |	\
				 S5PV210_UFCON_RXTRIG4)

static struct s3c2410_uartcfg qss_uartcfgs[] __initdata = {
	{
		.hwport		= 0,
		.flags		= 0,
		.ucon		= SMDKC110_UCON_DEFAULT,
		.ulcon		= SMDKC110_ULCON_DEFAULT,
		.ufcon		= SMDKC110_UFCON_DEFAULT,
	},
	{
		.hwport		= 1,
		.flags		= 0,
		.ucon		= SMDKC110_UCON_DEFAULT,
		.ulcon		= SMDKC110_ULCON_DEFAULT,
		.ufcon		= SMDKC110_UFCON_DEFAULT,
	},
#ifndef CONFIG_FIQ_DEBUGGER
	{
		.hwport		= 2,
		.flags		= 0,
		.ucon		= SMDKC110_UCON_DEFAULT,
		.ulcon		= SMDKC110_ULCON_DEFAULT,
		.ufcon		= SMDKC110_UFCON_DEFAULT,
	},
#endif
	{
		.hwport		= 3,
		.flags		= 0,
		.ucon		= SMDKC110_UCON_DEFAULT,
		.ulcon		= SMDKC110_ULCON_DEFAULT,
		.ufcon		= SMDKC110_UFCON_DEFAULT,
	},
};

static struct platform_device *qss_devices[] __initdata = {
	&s3c_device_rtc,
	&s3c_device_wdt,
};

static void __init qss_fixup(struct tag *tag, char **cmdline,
		struct meminfo *mi)
{
	mi->bank[0].start = 0x30000000;
	mi->bank[0].size = 80 * SZ_1M;
	mi->bank[1].start = 0x40000000;
	mi->bank[1].size = 384 * SZ_1M;
	mi->nr_banks = 2;
}

static void __init qss_map_io(void)
{
	s5p_init_io(NULL, 0, S5P_VA_CHIPID);
	s3c24xx_init_clocks(24000000);
	s3c24xx_init_uarts(qss_uartcfgs, ARRAY_SIZE(qss_uartcfgs));
	s5p_set_timer_source(S5P_PWM3, S5P_PWM4);
}

static void __init qss_machine_init(void)
{
	/* initialize gpios */
	qss_gpio_init();

	s3c_pm_init();

	/* FIQ debugger */
#ifdef CONFIG_FIQ_DEBUGGER
	qss_fiqdbg_init();
#endif

	platform_add_devices(qss_devices, ARRAY_SIZE(qss_devices));
}

MACHINE_START(QSS, "QSS")
	.atag_offset	= 0x100,
	.fixup		= qss_fixup,
	.init_irq	= s5pv210_init_irq,
	.map_io		= qss_map_io,
	.init_machine	= qss_machine_init,
	.timer		= &s5p_timer,
MACHINE_END
