/*
 *  drivers/switch/switch_toggle.c
 *
 * Copyright (C) 2012 Suryandaru Triandana <syndtr@gmail.com>
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
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/switch.h>

struct toggle_switch_data {
	struct switch_dev sdev;
	struct toggle_switch_platform_data *pdata;
	const char *states[2];
	struct switch_attr attrs[1];
};

static int toggle_set_state(struct switch_dev *sdev, struct switch_attr *attr, unsigned state)
{
	struct toggle_switch_data *switch_data =
		container_of(sdev, struct toggle_switch_data, sdev);

	switch_data->pdata->func(state);
	return 0;
}

static ssize_t print_name(struct switch_dev *sdev, char *buf)
{
	struct toggle_switch_data *switch_data =
		container_of(sdev, struct toggle_switch_data, sdev);
	int ret = switch_get_state(sdev, 0);

	if (ret < 0)
		return -EINVAL;

	if (ret)
		if (switch_data->pdata->name_on)
			return sprintf(buf, "%s", switch_data->pdata->name_on);
		else
			return -1;
	else
		if (switch_data->pdata->name_off)
			return sprintf(buf, "%s", switch_data->pdata->name_off);
		else
			return -1;
}

static int toggle_switch_probe(struct platform_device *pdev)
{
	struct toggle_switch_platform_data *pdata = pdev->dev.platform_data;
	struct toggle_switch_data *switch_data;
	struct switch_attr *attr;
	int ret = 0;

	if (!pdata || !pdata->func)
		return -EBUSY;

	switch_data = kzalloc(sizeof(struct toggle_switch_data), GFP_KERNEL);
	if (!switch_data)
		return -ENOMEM;

	attr = &(switch_data->attrs[0]);
	attr->name = "state";
	attr->states = switch_data->states;
	attr->states_nr = ARRAY_SIZE(switch_data->states);
	attr->state = 0;
	attr->flags = SWITCH_ATTR_READABLE | SWITCH_ATTR_WRITABLE | SWITCH_ATTR_UEVENT;
	attr->set_state = toggle_set_state;

	if (pdata->state_on)
		attr->states[1] = pdata->state_on;
	else
		attr->states[1] = "1";
	if (pdata->state_off)
		attr->states[0] = pdata->state_off;
	else
		attr->states[0] = "0";
	
	switch_data->sdev.name = pdata->name;
	switch_data->sdev.attrs = switch_data->attrs;
	switch_data->sdev.attrs_nr = ARRAY_SIZE(switch_data->attrs);

	switch_data->pdata = pdata;
	if (pdata->name_on || pdata->name_off)
		switch_data->sdev.print_name = print_name;

	ret = switch_dev_register(&switch_data->sdev);
	if (ret < 0)
		goto err_switch_dev_register;

	return 0;

err_switch_dev_register:
	kfree(switch_data);

	return ret;
}

static int __devexit toggle_switch_remove(struct platform_device *pdev)
{
	struct toggle_switch_data *switch_data = platform_get_drvdata(pdev);

	switch_dev_unregister(&switch_data->sdev);
	kfree(switch_data);

	return 0;
}

static struct platform_driver toggle_switch_driver = {
	.probe		= toggle_switch_probe,
	.remove		= __devexit_p(toggle_switch_remove),
	.driver		= {
		.name	= "switch-toggle",
		.owner	= THIS_MODULE,
	},
};

static int __init toggle_switch_init(void)
{
	return platform_driver_register(&toggle_switch_driver);
}

static void __exit toggle_switch_exit(void)
{
	platform_driver_unregister(&toggle_switch_driver);
}

module_init(toggle_switch_init);
module_exit(toggle_switch_exit);

MODULE_AUTHOR("Suryandaru Triandana <syndtr@gmail.com>");
MODULE_DESCRIPTION("Toggle Switch driver");
MODULE_LICENSE("GPL");
