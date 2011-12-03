/* linux/arch/arm/mach-s5pv210/qss-display.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <linux/fb.h>
#include <linux/lcd.h>

#include <mach/gpio.h>

#include <plat/devs.h>
#include <plat/gpio-cfg.h>
#include <plat/fb.h>
#include <plat/regs-fb-v4.h>

#include <mach/qss.h>

#define LCD_BUS_NUM	3
#define LCD_WIDTH	480
#define LCD_HEIGHT	800

/* Framebuffer device */

static struct s3c_fb_pd_win fb_win0 = {
	.win_mode = {
		.left_margin	= 16,
		.right_margin	= 16,
		.upper_margin	= 2,
		.lower_margin	= 28,
		.hsync_len	= 2,
		.vsync_len	= 1,
		.xres		= LCD_WIDTH,
		.yres		= LCD_HEIGHT,
		.refresh	= 55,
	},
	.max_bpp	= 32,
	.default_bpp	= 32,
	.virtual_x	= LCD_WIDTH,
	.virtual_y	= 2 * LCD_HEIGHT,
};

static struct s3c_fb_platdata fb_pdata __initdata = {
	.win[0]		= &fb_win0,
	.vidcon0	= VIDCON0_VIDOUT_RGB | VIDCON0_PNRMODE_RGB |
			  VIDCON0_CLKSEL_LCD,
	.vidcon1	= VIDCON1_INV_VCLK | VIDCON1_INV_VDEN
			  | VIDCON1_INV_HSYNC | VIDCON1_INV_VSYNC,
	.setup_gpio	= s5pv210_fb_gpio_setup_24bpp,
};

/* SPI controller device */

static struct spi_gpio_platform_data spi_gpio_data = {
	.sck	= GPIO_DISPLAY_CLK,
	.mosi	= GPIO_DISPLAY_SI,
	.miso	= SPI_GPIO_NO_MISO,
	.num_chipselect = 1,
};

static struct platform_device qss_device_spi_gpio = {
	.name	= "spi_gpio",
	.id	= LCD_BUS_NUM,
	.dev	= {
		.parent		= &s3c_device_fb.dev,
		.platform_data	= &spi_gpio_data,
	},
};

static struct platform_device pvr_device_g3d = {
	.name		= "pvrsrvkm",
	.id		= -1,
};

static struct platform_device pvr_device_lcd = {
	.name		= "s3c_lcd",
	.id		= -1,
};

/* S6E63M0 SPI info */

static int lcd_reset(struct lcd_device *ld)
{
	static unsigned int first = 1;

	if (first) {
		gpio_request(GPIO_MLCD_RST, "MLCD_RST");
		first = 0;
	}

	gpio_direction_output(GPIO_MLCD_RST, 1);
	return 1;
}

static int lcd_power_on(struct lcd_device *ld, int enable)
{
	return 1;
}

static struct lcd_platform_data lcd_pdata = {
	.reset			= lcd_reset,
	.power_on		= lcd_power_on,
	.lcd_enabled		= 0,
	.reset_delay		= 120,	/* 120ms */
	.power_on_delay		= 25,	/* 25ms */
	.power_off_delay	= 200,	/* 200ms */
};

static struct spi_board_info s6e63m0_spi[] __initdata = {
	{
		.modalias	= "s6e63m0",
		.platform_data	= &lcd_pdata,
		.max_speed_hz	= 1200000,
		.bus_num	= LCD_BUS_NUM,
		.chip_select	= 0,
		.mode		= SPI_MODE_3,
		.controller_data = (void *)GPIO_DISPLAY_CS,
	},
};

static void __init qss_display_cfg_gpio(void)
{
	s3c_gpio_cfgrange_nopull(GPIO_DISPLAY_CS, 1, S3C_GPIO_OUTPUT);
	s3c_gpio_cfgrange_nopull(GPIO_DISPLAY_CLK, 1, S3C_GPIO_OUTPUT);
	s3c_gpio_cfgrange_nopull(GPIO_DISPLAY_SI, 1, S3C_GPIO_OUTPUT);
	s3c_gpio_cfgrange_nopull(GPIO_MLCD_RST, 1, S3C_GPIO_OUTPUT);
}

static struct platform_device *qss_devices[] __initdata = {
	&s3c_device_fb,
	&qss_device_spi_gpio,
	&pvr_device_g3d,
	&pvr_device_lcd,
};

void __init qss_display_init(void)
{
	/* display gpio config */
	qss_display_cfg_gpio();

	/* set fb pdata */
	s3c_fb_set_platdata(&fb_pdata);

	/* register devices */
	platform_add_devices(qss_devices, ARRAY_SIZE(qss_devices));

	/* register spi */
	spi_register_board_info(s6e63m0_spi, ARRAY_SIZE(s6e63m0_spi));
}
