/*
 *  Switch class driver
 *
 * Copyright (C) 2008 Google, Inc.
 * Author: Mike Lockwood <lockwood@android.com>
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

#ifndef __LINUX_SWITCH_H__
#define __LINUX_SWITCH_H__

#include <linux/device.h>
#include <linux/list.h>
#include <linux/workqueue.h>

#define SWITCH_ATTR_READABLE	(1 << 0)
#define SWITCH_ATTR_WRITEABLE	(1 << 1)
#define SWITCH_ATTR_UEVENT	(1 << 2)
#define SWITCH_ATTR_IGNSTATE	(1 << 3)

#define DEFINE_SWITCH_HANDLER(_name, _dev, _attr_id, _func, _data)	\
	struct switch_handler _name = {					\
		.dev		= _dev,					\
		.attr_id	= _attr_id,				\
		.func		= _func,				\
		.data		= _data					\
	}

struct switch_dev;
struct switch_attr;
struct switch_handler;
struct switch_handler_list;

struct switch_attr {
	const char	*name;
	const char	**states;
	unsigned	states_nr;
	unsigned	state;

	int (*get_state)(struct switch_dev *sdev, struct switch_attr *attr);
	int (*set_state)(struct switch_dev *sdev, struct switch_attr *attr, unsigned state);

	int		flags;

	struct work_struct wq;
	struct device_attribute dev_attr;
	struct switch_handler_list *handlers;
	struct switch_dev *sdev;
};

struct switch_dev {
	const char	*name;
	struct device	*dev;
	int		index;

	struct switch_attr *attrs;
	unsigned	attrs_nr;

	ssize_t	(*print_name)(struct switch_dev *sdev, char *buf);
};

struct switch_handler {
	const char	*dev;
	unsigned	attr_id;
	void		*data;
	void (*func)(struct switch_handler *, unsigned);
	
	struct list_head list;
	struct switch_handler_list *top;
};

struct gpio_switch_platform_data {
	const char	*name;
	unsigned 	gpio;

	/* if NULL, switch_dev.name will be printed */
	const char	*name_on;
	const char	*name_off;
	/* if NULL, "0" or "1" will be printed */
	const char	*state_on;
	const char	*state_off;
};

extern int switch_dev_register(struct switch_dev *sdev);
extern void switch_dev_unregister(struct switch_dev *sdev);

extern int switch_get_state(struct switch_dev *sdev, unsigned id);
extern int switch_set_state(struct switch_dev *sdev, unsigned id, unsigned state);

extern int switch_handler_register(struct switch_handler *handler);
extern int switch_handler_remove(struct switch_handler *handler);
extern int switch_handler_set_state(struct switch_handler *handler, unsigned state);

#endif /* __LINUX_SWITCH_H__ */
