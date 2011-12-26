/* linux/arch/arm/mach-s5pv210/include/mach/param.h
 * 
 * Copyright (C) 2011 Suryandaru Triandana <syndtr@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __MACH_PARAM_H
#define __MACH_PARAM_H

#include <mach/regs-clock.h>

#define PARAM_INFORM		S5P_INFORM6

#define REBOOT_MODE_MASK	0xf
#define REBOOT_MODE_SHIFT	0

enum {
	REBOOT_MODE_NONE,
	REBOOT_MODE_LPM,
	REBOOT_MODE_RECOVERY
};

#define REBOOT_CAUSE_MASK	0xf
#define REBOOT_CAUSE_SHIFT	4

#define REBOOT_LAST_CAUSE_MASK	0xf
#define REBOOT_LAST_CAUSE_SHIFT	8

enum {
	REBOOT_CAUSE_NONE,
	REBOOT_CAUSE_PANIC,
	REBOOT_CAUSE_WATCHDOG,
};

#define PARAM_INFORM_GET(val, type)	\
	((val & type##_MASK << type##_SHIFT) >> type##_SHIFT)

#define PARAM_INFORM_SET(val, to, type)	\
	((to & ~(type##_MASK << type##_SHIFT)) | (val << type##_SHIFT))

enum {
	PARAM_SWITCH_BOOTMODE,
	PARAM_SWITCH_REBOOTCAUSE
};

#endif
