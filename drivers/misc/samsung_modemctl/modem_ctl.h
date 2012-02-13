/*
 * Copyright (C) 2010 Google, Inc.
 * Copyright (C) 2010 Samsung Electronics.
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

#ifndef __MODEM_CONTROL_H__
#define __MODEM_CONTROL_H__

#define IOCTL_MODEM_MAGIC		0xf0
#define IOCTL_MODEM_ON			_IO(IOCTL_MODEM_MAGIC, 0xc0)
#define IOCTL_MODEM_OFF			_IO(IOCTL_MODEM_MAGIC, 0xc1)
#define IOCTL_MODEM_GETSTATUS		_IOR(IOCTL_MODEM_MAGIC, 0xc2, unsigned int)
#define IOCTL_MODEM_RESET		_IO(IOCTL_MODEM_MAGIC, 0xc5)
#define IOCTL_MODEM_RAMDUMP_ON		_IO(IOCTL_MODEM_MAGIC, 0xc6)
#define IOCTL_MODEM_RAMDUMP_OFF		_IO(IOCTL_MODEM_MAGIC, 0xc7)
#define IOCTL_MODEM_MEM_RW		_IOWR(IOCTL_MODEM_MAGIC, 0xc8, unsigned long)

struct modem_io {
	uint32_t raw;
	uint32_t size;
	uint32_t id;
	uint32_t cmd;
	void *data;
	int (*filter)(struct modem_io*);
};

struct modem_rw {
	unsigned int addr;
	unsigned char *data;
	unsigned int size;
	int write;
};

/* platform data */
struct modemctl_data {
	const char *name;
	unsigned gpio_phone_active;
	unsigned gpio_pda_active;
	unsigned gpio_cp_reset;
	unsigned gpio_phone_on;
	bool is_cdma_modem; /* 1:CDMA Modem */
};

#endif
