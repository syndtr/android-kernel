/* arch/arm/mach-s5pv210/qss-pm.c
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

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/pm.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/setup.h>
#include <asm/mach-types.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/gpio.h>
#include <mach/param.h>

#include <plat/pm.h>
#include <plat/watchdog-reset.h>

#include <mach/qss.h>

static int qss_notifier_call(struct notifier_block *this,
					unsigned long code, void *_cmd)
{
	unsigned int tmp = REBOOT_MODE_NONE;

	if ((code == SYS_RESTART) && _cmd) {
		if (!strcmp((char *)_cmd, "recovery"))
			tmp = REBOOT_MODE_RECOVERY;
		else
			tmp = REBOOT_MODE_NONE;
	}

	/* set reboot mode */
	tmp = PARAM_INFORM_SET(tmp, __raw_readl(PARAM_INFORM), REBOOT_MODE);

	/* clear reboot cause */
	tmp = PARAM_INFORM_SET(REBOOT_CAUSE_NONE, tmp, REBOOT_CAUSE);

	__raw_writel(tmp, PARAM_INFORM);

	return NOTIFY_DONE;
}

static struct notifier_block qss_reboot_notifier = {
	.notifier_call = qss_notifier_call,
};

static void qss_power_off(void)
{
	while (1) {
		/* wait for power button to be released */
		if (gpio_get_value(GPIO_KEY_POWER)) {
			printk(KERN_INFO "%s: set PS_HOLD to low\n", __func__);

			__raw_writel(__raw_readl(S5P_PS_HOLD_CONTROL) & 0xfffffeff,
			       S5P_PS_HOLD_CONTROL);

			printk(KERN_CRIT "%s: not reached\n", __func__);
		}

		printk(KERN_INFO "%s: power button is not released.\n", __func__);
		mdelay(1000);
	}
}

void __init qss_pm_init(void)
{
	unsigned int tmp, cause;

	/* setup reboot cause */
	tmp = __raw_readl(PARAM_INFORM);
	cause = PARAM_INFORM_GET(tmp, REBOOT_CAUSE);
	tmp = PARAM_INFORM_SET(REBOOT_CAUSE_PANIC, tmp, REBOOT_CAUSE);
	tmp = PARAM_INFORM_SET(cause, tmp, REBOOT_LAST_CAUSE);
	__raw_writel(tmp, PARAM_INFORM);

	/* pm power hook */
	pm_power_off = qss_power_off;

	/* reboot notifier */
	register_reboot_notifier(&qss_reboot_notifier);

	/* initialize s3c pm */
	s3c_pm_init();
}

void qss_restart(char mode, const char *cmd)
{
	arch_wdt_reset();
}
