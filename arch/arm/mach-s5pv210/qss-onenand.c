/* linux/arch/arm/mach-s5pv210/qss-onenand.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/onenand.h>

#include <plat/devs.h>
#include <plat/onenand-core.h>

#include <mach/qss.h>

static struct mtd_partition qss_partition_info[] = {
	{
		.name		= "boot",
		.offset		= (72*SZ_256K),
		.size		= (30*SZ_256K),
	},
	{
		.name		= "recovery",
		.offset		= (102*SZ_256K),
		.size		= (30*SZ_256K),
	},
	{
		.name		= "system",
		.offset		= (132*SZ_256K),
		.size		= (1100*SZ_256K),
	},
	{
		.name		= "cache",
		.offset		= (1232*SZ_256K),
		.size		= (100*SZ_256K),
	},
	{
		.name		= "datadata",
		.offset		= (1332*SZ_256K),
		.size		= (672*SZ_256K),
	},
	{
		.name		= "efs",
		.offset		= (2*SZ_256K),
		.size		= (40*SZ_256K),
	},
	{
		.name		= "reservoir",
		.offset		= (2004*SZ_256K),
		.size		= (44*SZ_256K), //2047
	},
};

static struct onenand_platform_data onenand_pdata = {
	.parts		= qss_partition_info,
	.nr_parts	= ARRAY_SIZE(qss_partition_info),
	.options	= ONENAND_IS_BROKEN_FLEXONENAND,
};

void __init qss_onenand_init(void)
{
	s5p_onenand_set_platdata(&onenand_pdata);

	/* register onenand */
	platform_device_register(&s5p_device_onenand);
}
