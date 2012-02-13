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

#define QSS_MEMBANK0_START	0x30000000
#define QSS_MEMBANK0_SIZE	(80 * SZ_1M)
#define QSS_MEMBANK1_START	0x40000000
#define QSS_MEMBANK1_SIZE	(383 * SZ_1M)

#define QSS_PA_RAMCONSOLE	(QSS_MEMBANK1_START + QSS_MEMBANK1_SIZE)

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
void qss_phone_init(void) __init;

/* pm */
void qss_restart(char mode, const char *cmd);

/* hw rev */
extern int qss_hwrev(void);

/* sound */
extern void qss_set_micbias_main(bool state);
extern void qss_set_micbias_ear(bool state);

#endif
