/* linux/arch/arm/plat-samsung/dev-i2c-gpio.c
 *
 * Copyright (c) 2011 Suryandaru Triandana <syndtr@gmail.com>
 * 
 * S5P series device definition for i2c-gpio devices
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/gfp.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/platform_device.h>

#include <mach/gpio.h>

#include <plat/regs-iic.h>
#include <plat/iic.h>
#include <plat/devs.h>
#include <plat/cpu.h>
#include <plat/gpio-cfg.h>

#define HAS_I2C(ID)							\
	defined(GPIO_I2C##ID##_SDA) && defined(GPIO_I2C##ID##_SCL)

#define DEFINE_I2C(ID)							\
	static struct i2c_gpio_platform_data i2c##ID##_pdata = {	\
		.sda_pin                = GPIO_I2C##ID##_SDA,		\
		.scl_pin                = GPIO_I2C##ID##_SCL,		\
		.udelay                 = GPIO_I2C##ID##_UDELAY,	\
		.sda_is_open_drain      = 0,				\
		.scl_is_open_drain      = 0,				\
		.scl_is_output_only     = 0,				\
	};								\
	struct platform_device s3c_device_i2c##ID = {			\
		.name			= "i2c-gpio",			\
		.id			= ID,				\
		.dev.platform_data	= &i2c##ID##_pdata,		\
	};								\
	void s3c_i2c##ID##_cfg_gpio(struct platform_device *dev)	\
	{								\
		s3c_gpio_cfgall_range(GPIO_I2C##ID##_SDA, 1,		\
			S3C_GPIO_INPUT, S3C_GPIO_PULL_NONE);		\
		s3c_gpio_cfgall_range(GPIO_I2C##ID##_SCL, 1,		\
			S3C_GPIO_INPUT, S3C_GPIO_PULL_NONE);		\
	}

#if HAS_I2C(3)
#ifndef GPIO_I2C3_UDELAY
#define GPIO_I2C3_UDELAY 2
#endif
DEFINE_I2C(3)
#endif

#if HAS_I2C(4)
#ifndef GPIO_I2C4_UDELAY
#define GPIO_I2C4_UDELAY 2
#endif
DEFINE_I2C(4)
#endif

#if HAS_I2C(5)
#ifndef GPIO_I2C5_UDELAY
#define GPIO_I2C5_UDELAY 2
#endif
DEFINE_I2C(5)
#endif

#if HAS_I2C(6)
#ifndef GPIO_I2C6_UDELAY
#define GPIO_I2C6_UDELAY 2
#endif
DEFINE_I2C(6)
#endif

#if HAS_I2C(7)
#ifndef GPIO_I2C7_UDELAY
#define GPIO_I2C7_UDELAY 2
#endif
DEFINE_I2C(7)
#endif

#if HAS_I2C(8)
#ifndef GPIO_I2C8_UDELAY
#define GPIO_I2C8_UDELAY 2
#endif
DEFINE_I2C(8)
#endif

#if HAS_I2C(9)
#ifndef GPIO_I2C9_UDELAY
#define GPIO_I2C9_UDELAY 2
#endif
DEFINE_I2C(9)
#endif

#if HAS_I2C(10)
#ifndef GPIO_I2C10_UDELAY
#define GPIO_I2C10_UDELAY 2
#endif
DEFINE_I2C(10)
#endif

#if HAS_I2C(11)
#ifndef GPIO_I2C11_UDELAY
#define GPIO_I2C11_UDELAY 2
#endif
DEFINE_I2C(11)
#endif

#if HAS_I2C(12)
#ifndef GPIO_I2C12_UDELAY
#define GPIO_I2C12_UDELAY 2
#endif
DEFINE_I2C(12)
#endif

#if HAS_I2C(13)
#ifndef GPIO_I2C13_UDELAY
#define GPIO_I2C13_UDELAY 2
#endif
DEFINE_I2C(13)
#endif

#if HAS_I2C(14)
#ifndef GPIO_I2C14_UDELAY
#define GPIO_I2C14_UDELAY 2
#endif
DEFINE_I2C(14)
#endif
