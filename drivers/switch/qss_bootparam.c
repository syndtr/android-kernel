/*
 *  drivers/switch/qss_bootparam.c
 *
 * Copyright (C) 2011 Suryandaru Triandana <syndtr@gmail.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/switch.h>
#include <linux/io.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/param.h>

static const char *bootmodes[] = {
	"none",
	"charger",
	"recovery",
};

static const char *rebootcauses[] = {
	"none",
	"panic",
	"watchdog",
};

static struct switch_attr attrs[] = {
	[PARAM_SWITCH_BOOTMODE] = {
		.name		= "bootmode",
		.states		= bootmodes,
		.states_nr	= ARRAY_SIZE(bootmodes),
		.state		= 0,
		.flags		= SWITCH_ATTR_READABLE | SWITCH_ATTR_UEVENT,
	},
	[PARAM_SWITCH_REBOOTCAUSE] = {
		.name		= "rebootcause",
		.states		= rebootcauses,
		.states_nr	= ARRAY_SIZE(rebootcauses),
		.state		= 0,
		.flags		= SWITCH_ATTR_READABLE | SWITCH_ATTR_UEVENT,
	},
};

static struct switch_dev sdev = {
	.name		= "bootparam",
	.attrs		= attrs,
	.attrs_nr	= ARRAY_SIZE(attrs),
};

static int __init bootparam_init(void)
{
	/* set bootmode */
	attrs[PARAM_SWITCH_BOOTMODE].state =
		PARAM_INFORM_GET(readl(PARAM_INFORM), REBOOT_MODE);

	/* set reboot cause */
	attrs[PARAM_SWITCH_REBOOTCAUSE].state =
		PARAM_INFORM_GET(readl(PARAM_INFORM), REBOOT_LAST_CAUSE);

	return switch_dev_register(&sdev);
}

static void __exit bootparam_exit(void)
{
	switch_dev_unregister(&sdev);
}

module_init(bootparam_init);
module_exit(bootparam_exit);

MODULE_AUTHOR("Suryandaru Triandana <syndtr@gmail.com>");
MODULE_DESCRIPTION("QSS Boot Parameter driver");
MODULE_LICENSE("GPL");