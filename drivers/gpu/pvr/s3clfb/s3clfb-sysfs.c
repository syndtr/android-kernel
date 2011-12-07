/*
 * s3clfb-sysfs.c
 *
 * Copyright (C) 2011 Texas Instruments.
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 *
 * Author: Gustavo Diaz (gusdp@ti.com)
 *
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/uaccess.h>

#include "img_defs.h"
#include "servicesext.h"
#include "kerneldisplay.h"
#include "s3clfb.h"

static ssize_t show_ignore_sync(S3CLFB_DEVINFO *display_info, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", display_info->ignore_sync);
}

static ssize_t store_ignore_sync(S3CLFB_DEVINFO *display_info,
	const char *buf, size_t count)
{
	unsigned long new_value;

	if (strict_strtoul(buf, 10, &new_value))
		return -EINVAL;

	if (new_value == 0 || new_value == 1) {
		display_info->ignore_sync = new_value;
		return count;
	}

	return -EINVAL;
}

struct s3clfb_attribute {
	struct attribute attr;
	ssize_t (*show)(S3CLFB_DEVINFO *, char *);
	ssize_t (*store)(S3CLFB_DEVINFO *, const char *, size_t);
};

static ssize_t s3clfb_attr_show(struct kobject *kobj, struct attribute *attr,
		char *buf)
{
	S3CLFB_DEVINFO *display_info;
	struct s3clfb_attribute *s3clfb_attr;

	display_info = container_of(kobj, S3CLFB_DEVINFO, kobj);
	s3clfb_attr = container_of(attr, struct s3clfb_attribute, attr);

	if (!s3clfb_attr->show)
		return -ENOENT;

	return s3clfb_attr->show(display_info, buf);
}

static ssize_t s3clfb_attr_store(struct kobject *kobj, struct attribute *attr,
		const char *buf, size_t size)
{
	S3CLFB_DEVINFO *display_info;
	struct s3clfb_attribute *s3clfb_attr;

	display_info = container_of(kobj, S3CLFB_DEVINFO, kobj);
	s3clfb_attr = container_of(attr, struct s3clfb_attribute, attr);

	if (!s3clfb_attr->store)
		return -ENOENT;

	return s3clfb_attr->store(display_info, buf, size);
}

#define S3CLFB_ATTR(_name, _mode, _show, _store) \
	struct s3clfb_attribute s3clfb_attr_##_name = \
	__ATTR(_name, _mode, _show, _store)

static S3CLFB_ATTR(ignore_sync, S_IRUGO|S_IWUSR, show_ignore_sync,
	store_ignore_sync);

#undef S3CLFB_ATTR

static struct attribute *s3clfb_sysfs_attrs[] = {
	&s3clfb_attr_ignore_sync.attr,
	NULL
};

static const struct sysfs_ops s3clfb_sysfs_ops = {
	.show = s3clfb_attr_show,
	.store = s3clfb_attr_store,
};

static struct kobj_type s3clfb_ktype = {
	.sysfs_ops = &s3clfb_sysfs_ops,
	.default_attrs = s3clfb_sysfs_attrs,
};

void s3clfb_create_sysfs(struct s3clfb_device *odev)
{
	int i, r;

	/* Create a sysfs entry for every display */
	for (i = 0; i < odev->display_count; i++) {
		S3CLFB_DEVINFO *display_info = &odev->display_info_list[i];
		r = kobject_init_and_add(&display_info->kobj, &s3clfb_ktype,
			&odev->dev->kobj, "display%d",
			display_info->uDeviceID);
		if (r)
			ERROR_PRINTK("failed to create sysfs file\n");
	}
}

void s3clfb_remove_sysfs(struct s3clfb_device *odev)
{
	int i;
	for (i = 0; i < odev->display_count; i++) {
		S3CLFB_DEVINFO *display_info = &odev->display_info_list[i];
		kobject_del(&display_info->kobj);
		kobject_put(&display_info->kobj);
	}
}
