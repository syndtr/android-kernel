/* linux/arch/arm/mach-s5pv210/qss-mfd.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <linux/mfd/max8998.h>
#include <linux/gpio.h>

#include <mach/irqs.h>
#include <mach/gpio.h>
#include <mach/regs-gpio.h>

#include <plat/devs.h>
#include <plat/iic.h>
#include <plat/gpio-cfg.h>

#include <mach/qss.h>

/* Consumers */

static struct regulator_consumer_supply ldo3_consumer[] = {
	REGULATOR_SUPPLY("vusb_a", "s3c-hsotg")
};

static struct regulator_consumer_supply ldo4_consumer[] = {
	{ .supply = "vadc", },
	REGULATOR_SUPPLY("vdd", "samsung-adc-v3")
};

static struct regulator_consumer_supply ldo5_consumer[] = {
	{ .supply = "vmmc", },
};

static struct regulator_consumer_supply ldo7_consumer[] = {
	{ .supply = "vlcd", },
};

static struct regulator_consumer_supply ldo8_consumer[] = {
	REGULATOR_SUPPLY("vusb_d", "s3c-hsotg")
};

static struct regulator_consumer_supply ldo11_consumer[] = {
	{ .supply = "cam_af", },
};

static struct regulator_consumer_supply ldo12_consumer[] = {
	{ .supply = "cam_sensor", },
};

static struct regulator_consumer_supply ldo13_consumer[] = {
	{ .supply = "vga_vddio", },
};

static struct regulator_consumer_supply ldo14_consumer[] = {
	{ .supply = "vga_dvdd", },
};

static struct regulator_consumer_supply ldo15_consumer[] = {
	{ .supply = "cam_isp_host", },
};

static struct regulator_consumer_supply ldo16_consumer[] = {
	{ .supply = "vga_avdd", },
};

static struct regulator_consumer_supply ldo17_consumer[] = {
	{ .supply = "vcc_lcd", },
};

static struct regulator_consumer_supply buck1_consumer[] = {
	{ .supply = "vddarm", },
};

static struct regulator_consumer_supply buck2_consumer[] = {
	{ .supply = "vddint", },
};

static struct regulator_consumer_supply buck4_consumer[] = {
	{ .supply = "cam_isp_core", },
};

static struct regulator_consumer_supply esafeout1_consumer[] = {
	{ .supply = "esafeout1", },
};

static struct regulator_consumer_supply charger_consumer[] = {
	{ .supply = "charger", },
};

/* Regulators */

static struct regulator_init_data qss_ldo2_data = {
	.constraints	= {
		.name		= "VALIVE_1.2V",
		.min_uV		= 1200000,
		.max_uV		= 1200000,
		.apply_uV	= true,
		.always_on	= true,
		.state_mem	= {
			.enabled = 1,
		},
	},
};

static struct regulator_init_data qss_ldo3_data = {
	.constraints	= {
		.name		= "VUSB_1.1V",
		.min_uV		= 1100000,
		.max_uV		= 1100000,
		.apply_uV	= true,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = true,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo3_consumer),
	.consumer_supplies	= ldo3_consumer,
};

static struct regulator_init_data qss_ldo4_data = {
	.constraints	= {
		.name		= "VADC_3.3V",
		.min_uV		= 3300000,
		.max_uV		= 3300000,
		.apply_uV	= true,
		.always_on	= true,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = true,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo4_consumer),
	.consumer_supplies	= ldo4_consumer,
};

static struct regulator_init_data qss_ldo5_data = {
	.constraints	= {
		.name		= "VTF_2.8V",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= true,
		.always_on	= true,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = true,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo5_consumer),
	.consumer_supplies	= ldo5_consumer,
};

static struct regulator_init_data qss_ldo7_data = {
	.constraints	= {
		.name		= "VLCD_1.8V",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= true,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = true,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo7_consumer),
	.consumer_supplies	= ldo7_consumer,
};

static struct regulator_init_data qss_ldo8_data = {
	.constraints	= {
		.name		= "VUSB_3.3V",
		.min_uV		= 3300000,
		.max_uV		= 3300000,
		.apply_uV	= true,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = true,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo8_consumer),
	.consumer_supplies	= ldo8_consumer,
};

static struct regulator_init_data qss_ldo9_data = {
	.constraints	= {
		.name		= "VCC_2.8V_PDA",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= true,
		.always_on	= true,
	},
};

static struct regulator_init_data qss_ldo11_data = {
	.constraints	= {
		.name		= "CAM_AF_3.0V",
		.min_uV		= 3000000,
		.max_uV		= 3000000,
		.apply_uV	= true,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = true,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo11_consumer),
	.consumer_supplies	= ldo11_consumer,
};

static struct regulator_init_data qss_ldo12_data = {
	.constraints	= {
		.name		= "CAM_SENSOR_CORE_1.2V",
		.min_uV		= 1200000,
		.max_uV		= 1200000,
		.apply_uV	= true,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = true,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo12_consumer),
	.consumer_supplies	= ldo12_consumer,
};

static struct regulator_init_data qss_ldo13_data = {
	.constraints	= {
		.name		= "VGA_VDDIO_2.8V",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= true,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = true,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo13_consumer),
	.consumer_supplies	= ldo13_consumer,
};

static struct regulator_init_data qss_ldo14_data = {
	.constraints	= {
		.name		= "VGA_DVDD_1.8V",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= true,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = true,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo14_consumer),
	.consumer_supplies	= ldo14_consumer,
};

static struct regulator_init_data qss_ldo15_data = {
	.constraints	= {
		.name		= "CAM_ISP_HOST_2.8V",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= true,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = true,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo15_consumer),
	.consumer_supplies	= ldo15_consumer,
};

static struct regulator_init_data qss_ldo16_data = {
	.constraints	= {
		.name		= "VGA_AVDD_2.8V",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= true,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = true,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo16_consumer),
	.consumer_supplies	= ldo16_consumer,
};

static struct regulator_init_data qss_ldo17_data = {
	.constraints	= {
		.name		= "VCC_3.0V_LCD",
		.min_uV		= 3000000,
		.max_uV		= 3000000,
		.apply_uV	= true,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = true,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo17_consumer),
	.consumer_supplies	= ldo17_consumer,
};

static struct regulator_init_data qss_buck1_data = {
	.constraints	= {
		.name		= "VDD_ARM",
		.min_uV		= 750000,
		.max_uV		= 1500000,
		.apply_uV	= true,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.uV	= 1250000,
			.mode	= REGULATOR_MODE_NORMAL,
			.disabled = true,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(buck1_consumer),
	.consumer_supplies	= buck1_consumer,
};

static struct regulator_init_data qss_buck2_data = {
	.constraints	= {
		.name		= "VDD_INT",
		.min_uV		= 750000,
		.max_uV		= 1500000,
		.apply_uV	= true,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.uV	= 1100000,
			.mode	= REGULATOR_MODE_NORMAL,
			.disabled = true,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(buck2_consumer),
	.consumer_supplies	= buck2_consumer,
};

static struct regulator_init_data qss_buck3_data = {
	.constraints	= {
		.name		= "VCC_1.8V",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= true,
		.always_on	= true,
	},
};

static struct regulator_init_data qss_buck4_data = {
	.constraints	= {
		.name		= "CAM_ISP_CORE_1.2V",
		.min_uV		= 1200000,
		.max_uV		= 1200000,
		.apply_uV	= true,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = true,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(buck4_consumer),
	.consumer_supplies	= buck4_consumer,
};

static struct regulator_init_data qss_esafeout1_data = {
	.constraints	= {
		.name		= "ESAFEOUT1",
		.always_on	= true,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(esafeout1_consumer),
	.consumer_supplies	= esafeout1_consumer,
};

static struct regulator_init_data qss_charger_data = {
	.constraints	= {
		.name		= "CHARGER",
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(charger_consumer),
	.consumer_supplies	= charger_consumer,
};

static struct max8998_regulator_data qss_regulators[] = {
	{ MAX8998_LDO2,  &qss_ldo2_data },
	{ MAX8998_LDO3,  &qss_ldo3_data },
	{ MAX8998_LDO4,  &qss_ldo4_data },
	{ MAX8998_LDO5,  &qss_ldo5_data },
	{ MAX8998_LDO7,  &qss_ldo7_data },
	{ MAX8998_LDO8,  &qss_ldo8_data },
	{ MAX8998_LDO9,  &qss_ldo9_data },
	{ MAX8998_LDO11, &qss_ldo11_data },
	{ MAX8998_LDO12, &qss_ldo12_data },
	{ MAX8998_LDO13, &qss_ldo13_data },
	{ MAX8998_LDO14, &qss_ldo14_data },
	{ MAX8998_LDO15, &qss_ldo15_data },
	{ MAX8998_LDO16, &qss_ldo16_data },
	{ MAX8998_LDO17, &qss_ldo17_data },
	{ MAX8998_BUCK1, &qss_buck1_data },
	{ MAX8998_BUCK2, &qss_buck2_data },
	{ MAX8998_BUCK3, &qss_buck3_data },
	{ MAX8998_BUCK4, &qss_buck4_data },
	{ MAX8998_ESAFEOUT1, &qss_esafeout1_data },
	{ MAX8998_CHARGER,   &qss_charger_data },
};

static struct max8998_platform_data max8998_pdata = {
	.regulators	= qss_regulators,
	.num_regulators	= ARRAY_SIZE(qss_regulators),
	.irq_base	= MAX8998_EINT_BASE,
	.buck1_voltage1	= 1275000,
	.buck1_voltage2	= 1200000,
	.buck1_voltage3	= 1050000,
	.buck1_voltage4	= 950000,
	.buck2_voltage1	= 1100000,
	.buck2_voltage2	= 1000000,
	.buck1_set1	= GPIO_BUCK_1_EN_A,
	.buck1_set2	= GPIO_BUCK_1_EN_B,
	.buck1_default_idx = 0,
	.buck2_set3	= GPIO_BUCK_2_EN,
	.buck2_default_idx = 0,
	.wakeup		= true,
	.eoc		= 0, /* ac=20 - usb=40 */
	.restart	= -1,
	.timeout	= 6,
	
};

static struct i2c_board_info i2c6_devs[] __initdata = {
	{
		/* The address is 0xCC used since SRAD = 0 */
		I2C_BOARD_INFO("max8998", (0xCC >> 1)),
		.platform_data	= &max8998_pdata,
	},
};

static void __init qss_mfd_cfg_gpio(void)
{
	s3c_gpio_cfgrange_nopull(GPIO_BUCK_1_EN_A, 1, S3C_GPIO_OUTPUT);
	gpio_set_value(GPIO_BUCK_1_EN_A, 0);

	s3c_gpio_cfgrange_nopull(GPIO_BUCK_1_EN_B, 1, S3C_GPIO_OUTPUT);
	gpio_set_value(GPIO_BUCK_1_EN_B, 0);

	s3c_gpio_cfgrange_nopull(GPIO_BUCK_2_EN, 1, S3C_GPIO_OUTPUT);
	gpio_set_value(GPIO_BUCK_2_EN, 0);

	s3c_gpio_cfgrange_nopull(GPIO_MFD_INT, 1, EINT_MODE);
	i2c6_devs[0].irq = gpio_to_irq(GPIO_MFD_INT);
}

static struct platform_device *qss_devices[] __initdata = {
	&s3c_device_i2c6,
};

void __init qss_mfd_init(void)
{
	/* i2c gpio cfg */
	s3c_i2c6_cfg_gpio(&s3c_device_i2c6);

	/* register devices */
	platform_add_devices(qss_devices, ARRAY_SIZE(qss_devices));

	/* max8998 */
	qss_mfd_cfg_gpio();
	i2c_register_board_info(6, i2c6_devs, ARRAY_SIZE(i2c6_devs));
	regulator_has_full_constraints();
}
