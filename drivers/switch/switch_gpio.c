/*
 *  drivers/switch/switch_gpio.c
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/switch.h>
#include <linux/gpio.h>

struct gpio_switch_data {
	struct switch_dev sdev;
	struct gpio_switch_platform_data *pdata;
	int irq;
	const char *states[2];
	struct switch_attr attrs[1];
};

static irqreturn_t gpio_irq_handler(int irq, void *dev_id)
{
	struct gpio_switch_data *switch_data =
		(struct gpio_switch_data *)dev_id;
	int state;

	state = gpio_get_value(switch_data->pdata->gpio);
	switch_set_state(&switch_data->sdev, 0, state);
	return IRQ_HANDLED;
}

static int gpio_set_state(struct switch_dev *sdev, struct switch_attr *attr, unsigned state)
{
	struct gpio_switch_data *switch_data =
		container_of(sdev, struct gpio_switch_data, sdev);

	gpio_set_value(switch_data->pdata->gpio, state);
	return 0;
}

ssize_t	print_name(struct switch_dev *sdev, char *buf)
{
	struct gpio_switch_data	*switch_data =
		container_of(sdev, struct gpio_switch_data, sdev);
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

static int gpio_switch_probe(struct platform_device *pdev)
{
	struct gpio_switch_platform_data *pdata = pdev->dev.platform_data;
	struct gpio_switch_data *switch_data;
	struct switch_attr *attr;
	int ret = 0;

	if (!pdata)
		return -EBUSY;

	switch_data = kzalloc(sizeof(struct gpio_switch_data), GFP_KERNEL);
	if (!switch_data)
		return -ENOMEM;

	attr = &(switch_data->attrs[0]);
	attr->name = "state";
	attr->states = switch_data->states;
	attr->states_nr = ARRAY_SIZE(switch_data->states);
	attr->state = pdata->value;
	attr->flags = SWITCH_ATTR_READABLE | SWITCH_ATTR_UEVENT;

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
	
	ret = gpio_request(pdata->gpio, pdev->name);
	if (ret < 0)
		goto err_kfree;

	if (pdata->direction == SWITCH_GPIO_OUTPUT) {
		attr->flags |= SWITCH_ATTR_WRITABLE;
		attr->set_state = gpio_set_state;
		gpio_direction_output(pdata->gpio, attr->state);
	}

	ret = switch_dev_register(&switch_data->sdev);
	if (ret < 0)
		goto err_kfree;

	if (pdata->direction == SWITCH_GPIO_INPUT) {
		gpio_direction_input(pdata->gpio);
		switch_set_state(&switch_data->sdev, 0, gpio_get_value(pdata->gpio));
		switch_data->irq = gpio_to_irq(pdata->gpio);
		if (switch_data->irq >= 0) {
			ret = request_irq(switch_data->irq, gpio_irq_handler,
					  IRQF_TRIGGER_LOW, pdev->name, switch_data);
			if (ret < 0) {
				dev_err(&pdev->dev, "unable to request IRQ%d for GPIO%d (%d)\n",
					switch_data->irq, pdata->gpio, ret);
				switch_data->irq = -1;
				ret = 0;
			}
		}
	}

	return 0;

err_kfree:
	kfree(switch_data);

	return ret;
}

static int __devexit gpio_switch_remove(struct platform_device *pdev)
{
	struct gpio_switch_data *switch_data = platform_get_drvdata(pdev);

	if (switch_data->pdata->direction == SWITCH_GPIO_INPUT && switch_data->irq >= 0)
		free_irq(switch_data->irq, switch_data);
	gpio_free(switch_data->pdata->gpio);
	switch_dev_unregister(&switch_data->sdev);
	kfree(switch_data);

	return 0;
}

static struct platform_driver gpio_switch_driver = {
	.probe		= gpio_switch_probe,
	.remove		= __devexit_p(gpio_switch_remove),
	.driver		= {
		.name	= "switch-gpio",
		.owner	= THIS_MODULE,
	},
};

static int __init gpio_switch_init(void)
{
	return platform_driver_register(&gpio_switch_driver);
}

static void __exit gpio_switch_exit(void)
{
	platform_driver_unregister(&gpio_switch_driver);
}

module_init(gpio_switch_init);
module_exit(gpio_switch_exit);

MODULE_AUTHOR("Mike Lockwood <lockwood@android.com>");
MODULE_AUTHOR("Suryandaru Triandana <syndtr@gmail.com>");
MODULE_DESCRIPTION("GPIO Switch driver");
MODULE_LICENSE("GPL");
