/* linux/arch/arm/mach-s5pv210/include/mach/qss.h
 *
 * Copyright (c) 2011 Suryandaru Triandana <syndtr@gmail.com>
 *
 * S5PV210 - QSS specific definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __QSS_H
#define __QSS_H

/* inits */

void qss_pm_init(void) __init;
void qss_gpio_init(void) __init;
void qss_fiqdbg_init(void) __init;
void qss_input_init(void) __init;
void qss_mfd_init(void) __init;
void qss_power_init(void) __init;
void qss_switch_init(void) __init;
void qss_sdhci_init(void) __init;
void qss_onenand_init(void) __init;
void qss_display_init(void) __init;
void qss_media_init(void) __init;
void qss_wifi_init(void) __init;
void qss_sound_init(void) __init;

/* pm */
void qss_restart(char mode, const char *cmd);

/* hw rev */
extern int qss_hwrev(void);

#endif
