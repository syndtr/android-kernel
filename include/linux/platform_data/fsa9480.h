/*
 * Copyright (C) 2010 Samsung Electronics
 * Minkyu Kang <mk7.kang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _FSA9480_H_
#define _FSA9480_H_

enum {
	FSA9480_SWITCH_SELECT,
	FSA9480_SWITCH_USB,
	FSA9480_SWITCH_UART,
	FSA9480_SWITCH_CHARGER,
	FSA9480_SWITCH_JIG,
	FSA9480_SWITCH_DESKDOCK,
	FSA9480_SWITCH_CARDOCK
};

enum {
	FSA9480_SWITCH_DETACHED,
	FSA9480_SWITCH_ATTACHED
};

enum {
	FSA9480_SWITCH_SEL_AUTO,
	FSA9480_SWITCH_SEL_DHOST,
	FSA9480_SWITCH_SEL_AUDIO,
	FSA9480_SWITCH_SEL_UART,
	FSA9480_SWITCH_SEL_VAUDIO
};

struct fsa9480_platform_data {
	void (*cfg_gpio) (void);
	void (*reset_cb) (void);
	void (*usb_power) (u8 on);
	int wakeup;
};

#endif /* _FSA9480_H_ */
