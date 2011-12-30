/* linux/arch/arm/mach-s5pv210/qss-media.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>

#include <plat/devs.h>
#include <plat/camport.h>

#include <mach/qss.h>

#include <media/v4l2-mediabus.h>
#include <media/s5p_fimc.h>

struct s5p_platform_fimc qss_fimc_md_pdata __initdata = { 0 };

static struct platform_device *qss_devices[] __initdata = {
	&s5p_device_fimc0,
	&s5p_device_fimc1,
	&s5p_device_fimc2,
	&s5p_device_fimc_md,
};

void __init qss_media_init(void)
{
	/* fimc */
	s3c_set_platdata(&qss_fimc_md_pdata, sizeof(qss_fimc_md_pdata),
			 &s5p_device_fimc_md);

	/* register devices */
	platform_add_devices(qss_devices, ARRAY_SIZE(qss_devices));
}
