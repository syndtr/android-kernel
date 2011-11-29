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

#define HAS_I2C(ID)							\
	defined(GPIO_I2C##ID##_SDA) && defined(GPIO_I2C##ID##_SCL)

#define DEFINE_I2C(ID)							\
	static struct i2c_gpio_platform_data i2c##ID##_pdata = {	\
		.sda_pin                = GPIO_I2C##ID##_SDA,		\
		.scl_pin                = GPIO_I2C##ID##_SCL,		\
		.udelay                 = 2,    /* 250KHz */		\
		.sda_is_open_drain      = 0,				\
		.scl_is_open_drain      = 0,				\
		.scl_is_output_only     = 0,				\
	};								\
	struct platform_device s3c_device_i2c##ID = {			\
		.name			= "i2c-gpio",			\
		.id			= ID,				\
		.dev.platform_data	= &i2c##ID##_pdata,		\
	}

	
#if HAS_I2C(3)
DEFINE_I2C(3);
#endif

#if HAS_I2C(4)
DEFINE_I2C(4);
#endif

#if HAS_I2C(5)
DEFINE_I2C(5);
#endif

#if HAS_I2C(6)
DEFINE_I2C(6);
#endif

#if HAS_I2C(7)
DEFINE_I2C(7);
#endif

#if HAS_I2C(8)
DEFINE_I2C(8);
#endif

#if HAS_I2C(9)
DEFINE_I2C(9);
#endif

#if HAS_I2C(10)
DEFINE_I2C(10);
#endif

#if HAS_I2C(11)
DEFINE_I2C(11);
#endif

#if HAS_I2C(12)
DEFINE_I2C(12);
#endif

#if HAS_I2C(13)
DEFINE_I2C(13);
#endif

#if HAS_I2C(14)
DEFINE_I2C(14);
#endif
