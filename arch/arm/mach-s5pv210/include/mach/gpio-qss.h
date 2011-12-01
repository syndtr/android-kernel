/* linux/arch/arm/mach-s5pv210/include/mach/gpio-qss.h
 *
 * Copyright (c) 2011 Suryandaru Triandana <syndtr@gmail.com>
 *
 * S5PV210 - GPIO table for QSS platform
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM_ARCH_GPIO_QSS_H
#define __ASM_ARCH_GPIO_QSS_H __FILE__

/* I2C */

#define GPIO_I2C4_SDA			S5PV210_MP05(3)
#define GPIO_I2C4_SCL			S5PV210_MP05(2)

#define GPIO_I2C5_SDA			S5PV210_GPJ3(6)
#define GPIO_I2C5_SCL			S5PV210_GPJ3(7)

#define GPIO_I2C6_SDA			S5PV210_GPJ4(0)
#define GPIO_I2C6_SCL			S5PV210_GPJ4(3)

#define GPIO_I2C7_SDA			S5PV210_GPJ3(4)
#define GPIO_I2C7_SCL			S5PV210_GPJ3(5)

#define GPIO_I2C8_SDA			S5PV210_GPD1(2)
#define GPIO_I2C8_SCL			S5PV210_GPD1(3)

#define GPIO_I2C9_SDA			S5PV210_MP05(1)
#define GPIO_I2C9_SCL			S5PV210_MP05(0)

#define GPIO_I2C10_SDA			S5PV210_GPJ3(0)
#define GPIO_I2C10_SCL			S5PV210_GPJ3(1)

#define GPIO_I2C11_SDA			S5PV210_GPG2(2)
#define GPIO_I2C11_SCL			S5PV210_GPG0(2)

#define GPIO_I2C12_SDA			S5PV210_GPJ0(1)
#define GPIO_I2C12_SCL			S5PV210_GPJ0(0)

#define GPIO_I2C14_SDA			S5PV210_MP04(5)
#define GPIO_I2C14_SCL			S5PV210_MP04(4)

#define GPIO_I2C15_SDA			S5PV210_GPC1(0)
#define GPIO_I2C15_SCL			S5PV210_GPC1(2)

/* Switch */

#define GPIO_USB_SEL			S5PV210_MP04(0)
#define GPIO_UART_SEL			S5PV210_MP05(7)
#define GPIO_SWITCH_INT			S5PV210_GPH2(7)

/* MFD */

#define GPIO_BUCK_1_EN_A		S5PV210_GPH0(3)
#define GPIO_BUCK_1_EN_B		S5PV210_GPH0(4)
#define GPIO_BUCK_2_EN			S5PV210_GPH0(5)

#define GPIO_MFD_INT			S5PV210_GPH0(7)

/* SDHCI */

#define GPIO_SDHCI2_CD_INT		S5PV210_GPH3(4)

#define GPIO_MASSMEMORY_EN		S5PV210_GPJ2(7)

/* LCD Panel */

#define GPIO_DISPLAY_CS			S5PV210_MP01(1)
#define GPIO_DISPLAY_CLK		S5PV210_MP04(1)
#define GPIO_DISPLAY_SI			S5PV210_MP04(3)

#define GPIO_MLCD_RST			S5PV210_MP05(5)

/* Input */

#define GPIO_KEY_POWER			S5PV210_GPH2(6)
#define GPIO_KEY_VOLUMEDOWN		S5PV210_GPH3(1)
#define GPIO_KEY_VOLUMEUP		S5PV210_GPH3(3)

#define GPIO_TSP_LDO_EN			S5PV210_GPJ1(3)
#define GPIO_TSP_INT			S5PV210_GPJ0(5)

#define GPIO_TOUCHKEY_EN		S5PV210_GPJ3(2)
#define GPIO_TOUCHKEY_INT		S5PV210_GPJ4(1)

#endif
