/* linux/arch/arm/mach-s5pv210/qss-sound.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/mfd/wm8994/pdata.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/fixed.h>
#include <linux/gpio.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <mach/gpio.h>
#include <mach/irqs.h>
#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/regs-gpio.h>

#include <plat/devs.h>
#include <plat/iic.h>
#include <plat/gpio-cfg.h>
#include <plat/clock.h>

#include <mach/qss.h>

static struct regulator_consumer_supply wm8994_fixed_voltage0_supplies[] = {
	REGULATOR_SUPPLY("DBVDD", "4-001a"),
	REGULATOR_SUPPLY("AVDD2", "4-001a"),
	REGULATOR_SUPPLY("CPVDD", "4-001a"),
};

static struct regulator_consumer_supply wm8994_fixed_voltage1_supplies[] = {
	REGULATOR_SUPPLY("SPKVDD1", "4-001a"),
	REGULATOR_SUPPLY("SPKVDD2", "4-001a"),
};

static struct regulator_init_data wm8994_fixed_voltage0_init_data = {
	.constraints	= {
		.always_on	= 1,
	},
	.num_consumer_supplies	= ARRAY_SIZE(wm8994_fixed_voltage0_supplies),
	.consumer_supplies	= wm8994_fixed_voltage0_supplies,
};

static struct regulator_init_data wm8994_fixed_voltage1_init_data = {
	.constraints	= {
		.always_on	= 1,
	},
	.num_consumer_supplies	= ARRAY_SIZE(wm8994_fixed_voltage1_supplies),
	.consumer_supplies	= wm8994_fixed_voltage1_supplies,
};

static struct fixed_voltage_config wm8994_fixed_voltage0_config = {
	.supply_name	= "VCC_1.8V_PDA",
	.microvolts	= 1800000,
	.gpio		= -EINVAL,
	.init_data	= &wm8994_fixed_voltage0_init_data,
};

static struct fixed_voltage_config wm8994_fixed_voltage1_config = {
	.supply_name	= "V_BAT",
	.microvolts	= 3700000,
	.gpio		= -EINVAL,
	.init_data	= &wm8994_fixed_voltage1_init_data,
};

static struct platform_device wm8994_fixed_voltage0 = {
	.name		= "reg-fixed-voltage",
	.id		= 0,
	.dev		= {
		.platform_data	= &wm8994_fixed_voltage0_config,
	},
};

static struct platform_device wm8994_fixed_voltage1 = {
	.name		= "reg-fixed-voltage",
	.id		= 1,
	.dev		= {
		.platform_data	= &wm8994_fixed_voltage1_config,
	},
};

static struct regulator_consumer_supply wm8994_avdd1_supply =
	REGULATOR_SUPPLY("AVDD1", "4-001a");

static struct regulator_consumer_supply wm8994_dcvdd_supply =
	REGULATOR_SUPPLY("DCVDD", "4-001a");

static struct regulator_init_data wm8994_ldo1_data = {
	.constraints	= {
		.name		= "AVDD1_3.0V",
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &wm8994_avdd1_supply,
};

static struct regulator_init_data wm8994_ldo2_data = {
	.constraints	= {
		.name		= "DCVDD_1.0V",
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &wm8994_dcvdd_supply,
};

static struct wm8994_pdata wm8994_pdata = {
	/* configure gpio1 function: 0x0001(Logic level input/output) */
	.gpio_defaults[0]	= 0x0001,
	/* configure gpio3/4/5/7 function for AIF2 voice */
	.gpio_defaults[2]	= 0x8100,
	.gpio_defaults[3]	= 0x8100,
	.gpio_defaults[4]	= 0x8100,
	.gpio_defaults[6]	= 0x0100,
	/* configure gpio8/9/10/11 function for AIF3 BT */
	.gpio_defaults[7]	= 0x8100,
	.gpio_defaults[8]	= 0x0100,
	.gpio_defaults[9]	= 0x0100,
	.gpio_defaults[10]	= 0x0100,
	.gpio_base		= WM8994_GPIO_BASE,
	.irq_base		= WM8994_EINT_BASE,
	.ldo[0]	= { S5PV210_GPF3(4),	NULL,	&wm8994_ldo1_data },	/* XM0FRNB_2 */
	.ldo[1]	= { 0,			NULL,	&wm8994_ldo2_data },
};

static struct i2c_board_info i2c4_devs[] __initdata = {
	{
		/* CS/ADDR = low 0x34 (FYI: high = 0x36) */
		I2C_BOARD_INFO("wm8994", 0x1a),
		.platform_data	= &wm8994_pdata,
	},
};

static struct platform_device qss_pcm = {
	.name		= "samsung-qss-pcm",
	.id		= -1,
};


static struct platform_device *qss_devices[] __initdata = {
	&s3c_device_i2c4,
	&s5pv210_device_iis0,
	&samsung_asoc_idma,
	&wm8994_fixed_voltage0,
	&wm8994_fixed_voltage1,
	&qss_pcm,
};


void __init qss_sound_init(void)
{
	s3c_gpio_cfgrange_nopull(S5PV210_GPH0(1), 1, EINT_MODE);
	i2c4_devs[0].irq = gpio_to_irq(S5PV210_GPH0(1));
	
	/* Ths main clock of WM8994 codec uses the output of CLKOUT pin.
	 * The CLKOUT[9:8] set to 0x3(XUSBXTI) of 0xE010E000(OTHERS)
	 * because it needs 24MHz clock to operate WM8994 codec.
	 */
	__raw_writel(__raw_readl(S5P_OTHERS) | (0x3 << 8), S5P_OTHERS);

	/* i2c gpio cfg */
	s3c_i2c4_cfg_gpio(&s3c_device_i2c4);

	/* wm8994 */
	i2c_register_board_info(4, i2c4_devs, ARRAY_SIZE(i2c4_devs));

	/* register devices */
	platform_add_devices(qss_devices, ARRAY_SIZE(qss_devices));
}
