/*
 *  drivers/switch/switch_class.c
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
#include <linux/types.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/switch.h>

struct switch_handler_head {
	const char *dev;

	struct list_head list;
	struct list_head handlers;
};

struct switch_handler_list {
	unsigned attr_id;
	struct switch_attr *attr;

	struct list_head list;
	struct list_head handlers;
	struct mutex mutex;
};

static LIST_HEAD(handlers);
static DEFINE_MUTEX(handlers_mutex);

static struct class *switch_class;
static atomic_t device_count;
static DEFINE_MUTEX(mutex);

static struct switch_handler_head *handler_get_head(const char *dev)
{
	struct list_head *p;
	struct switch_handler_head *h;

	mutex_lock(&handlers_mutex);

	list_for_each(p, &handlers) {
		h = list_entry(p, struct switch_handler_head, list);
		if (!strcmp(h->dev, dev))
			goto out;
	}

	h = kmalloc(sizeof(struct switch_handler_head), GFP_KERNEL);
	if (!h)
		goto out;

	h->dev = dev;
	INIT_LIST_HEAD(&h->handlers);
	list_add_tail(&h->list, &handlers);

out:
	mutex_unlock(&handlers_mutex);

	return h;
}

static struct switch_handler_list *handler_get_list(struct switch_handler_head *h, unsigned attr_id)
{
	struct list_head *p;
	struct switch_handler_list *l;

	mutex_lock(&handlers_mutex);

	list_for_each(p, &h->handlers) {
		l = list_entry(p, struct switch_handler_list, list);
		if (l->attr_id == attr_id)
			goto out;
	}

	l = kmalloc(sizeof(struct switch_handler_list), GFP_KERNEL);
	if (!l)
		goto out;

	l->attr_id = attr_id;
	l->attr = NULL;
	mutex_init(&l->mutex);
	INIT_LIST_HEAD(&l->handlers);
	list_add_tail(&l->list, &h->handlers);

out:
	mutex_unlock(&handlers_mutex);

	return l;
}

static void handler_exec(struct switch_handler_list *l, unsigned state)
{
	struct list_head *p;
	struct switch_handler *handler;

	if (!l)
		return;
	
	mutex_lock(&l->mutex);
	
	list_for_each(p, &l->handlers) {
		handler = list_entry(p, struct switch_handler, list);
		if (handler->func)
			handler->func(handler, state);
	}
	
	mutex_unlock(&l->mutex);
}

static ssize_t name_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct switch_dev *sdev = (struct switch_dev *)
		dev_get_drvdata(dev);

	if (sdev->print_name) {
		int ret = sdev->print_name(sdev, buf);
		if (ret >= 0)
			return ret;
	}

	return sprintf(buf, "%s\n", sdev->name);
}

static DEVICE_ATTR(name, S_IRUGO | S_IWUSR, name_show, NULL);

static int _switch_set_state(struct switch_attr *attr, unsigned state)
{
	if (state >= attr->states_nr)
		return -EINVAL;

	if (attr->state != state || attr->flags & SWITCH_ATTR_IGNSTATE) {
		attr->state = state;
		schedule_work(&attr->wq);
	}

	return 0;
}

static void _switch_set_state_wq(struct work_struct *work)
{
	struct switch_attr *attr = container_of(work, struct switch_attr, wq);
	struct switch_dev *sdev = attr->sdev;
	char name_buf[120];
	char attr_buf[120];
	char state_buf[120];
	char *envp[4];
	int env_offset = 0;
	char *prop_buf;
	int length = -1;
	int ret;

	if (attr->set_state) {
		ret = attr->set_state(sdev, attr, attr->state);
	}

	handler_exec(attr->handlers, attr->state);

	if (attr->flags & SWITCH_ATTR_UEVENT) {
		if (sdev->print_name) {
			prop_buf = (char *)get_zeroed_page(GFP_KERNEL);
			if (prop_buf) {
				length = sdev->print_name(sdev, prop_buf);
				if (length <= 0) {
					free_page((unsigned long)prop_buf);
					prop_buf = (char *)sdev->name;
				} else {
					if (prop_buf[length - 1] == '\n')
						prop_buf[length - 1] = 0;
				}
			} else {
				printk(KERN_ERR "switch: out of memory in switch_set_state\n");
				kobject_uevent(&sdev->dev->kobj, KOBJ_CHANGE);
			}
		} else
			prop_buf = (char *)sdev->name;
		if (prop_buf && length < 0) {
			snprintf(name_buf, sizeof(name_buf),
				"SWITCH_NAME=%s", prop_buf);
			envp[env_offset++] = name_buf;
		}
		snprintf(attr_buf, sizeof(attr_buf),
			"SWITCH_ATTR=%s", attr->name);
		envp[env_offset++] = attr_buf;
		snprintf(state_buf, sizeof(state_buf),
			"SWITCH_STATE=%s", attr->states[attr->state]);
		envp[env_offset++] = state_buf;
		envp[env_offset] = NULL;
		kobject_uevent_env(&sdev->dev->kobj, KOBJ_CHANGE, envp);
		if (prop_buf && prop_buf != sdev->name)
			free_page((unsigned long)prop_buf);
	}
}

int _switch_get_state(struct switch_dev *sdev, struct switch_attr *attr)
{
	int ret;

	if (attr->get_state) {
		ret = attr->get_state(sdev, attr);
		if (ret < 0)
			return ret;
		attr->state = (unsigned)ret;
	}

	return (int)attr->state;
}

static ssize_t switch_show(struct device *dev, struct device_attribute *dev_attr,
		char *buf)
{
	struct switch_dev *sdev = (struct switch_dev *)
		dev_get_drvdata(dev);
	struct switch_attr *attr;
	unsigned i;
	int state;

	for (i = 0; i < sdev->attrs_nr; i++) {
		attr = &sdev->attrs[i];
		if (attr->name == dev_attr->attr.name) {
			state = _switch_get_state(sdev, attr);
			if (state < 0)
				return state;
			if (state >= attr->states_nr)
				return sprintf(buf, "unknown [%x]\n", state);
			return sprintf(buf, "%s\n", attr->states[state]);
		}
	}

	return -EINVAL;
}

static ssize_t switch_store(struct device *dev, struct device_attribute *dev_attr,
		const char *buf, size_t count)
{
	struct switch_dev *sdev = (struct switch_dev *)
		dev_get_drvdata(dev);
	struct switch_attr *attr;
	unsigned i;

	for (i = 0; i < sdev->attrs_nr; i++) {
		attr = &sdev->attrs[i];
		if (attr->name == dev_attr->attr.name)
			goto attr_found;
	}

	return -EINVAL;

attr_found:
	for (i = 0; i < attr->states_nr; i++)
		if (!strncmp(attr->states[i], buf, strlen(attr->states[i]))) {
			return _switch_set_state(attr, i);
		}

	return -EINVAL;
}

int switch_set_state(struct switch_dev *sdev, unsigned id, unsigned state)
{
	if (id >= sdev->attrs_nr)
		return -EINVAL;

	return _switch_set_state(&sdev->attrs[id], state);
}
EXPORT_SYMBOL_GPL(switch_set_state);

int switch_get_state(struct switch_dev *sdev, unsigned id)
{
	struct switch_attr *attr;

	if (id >= sdev->attrs_nr)
		return -EINVAL;

	attr = &sdev->attrs[id];
	return _switch_get_state(sdev, attr);
}
EXPORT_SYMBOL_GPL(switch_get_state);

int switch_handler_register(struct switch_handler *handler)
{
	struct switch_handler_head *h;
	struct switch_handler_list *l;

	if (!handler->dev)
		return -EINVAL;

	h = handler_get_head(handler->dev);
	if (!h)
		return -ENOMEM;

	l = handler_get_list(h, handler->attr_id);
	if (!l)
		return -ENOMEM;

	handler->top = l;
	mutex_lock(&l->mutex);
	list_add_tail(&handler->list, &l->handlers);
	if (handler->func && l->attr)
		handler->func(handler, l->attr->state);
	mutex_unlock(&l->mutex);

	return 0;
}
EXPORT_SYMBOL_GPL(switch_handler_register);

int switch_handler_remove(struct switch_handler *handler)
{
	mutex_lock(&handler->top->mutex);
	list_del(&handler->list);
	mutex_unlock(&handler->top->mutex);

	return 0;
}
EXPORT_SYMBOL_GPL(switch_handler_remove);

int switch_handler_set_state(struct switch_handler *handler, unsigned state)
{
	struct switch_handler_list *l = handler->top;

	if (!l)
		return -EINVAL;
	if (!l->attr)
		return -EBUSY;

	return _switch_set_state(l->attr, state);
}
EXPORT_SYMBOL_GPL(switch_handler_set_state);

static int create_switch_class(void)
{
	int ret = 0;

	mutex_lock(&mutex);
	if (!switch_class) {
		switch_class = class_create(THIS_MODULE, "switch");
		if (IS_ERR(switch_class)) {
			ret = PTR_ERR(switch_class);
			switch_class = NULL;
			printk(KERN_ERR "switch: unable to create class: %d", ret);
			goto out;
		}
		atomic_set(&device_count, 0);
	}

out:
	mutex_unlock(&mutex);

	return ret;
}

static void switch_attrs_register(unsigned i, struct switch_dev *sdev,
				  struct switch_attr *attr,
				  struct switch_handler_head *hdlhead) {
	struct device_attribute *dev_attr = &attr->dev_attr;
	int ret;

	attr->sdev = sdev;

	dev_attr->attr.name = attr->name;
	dev_attr->attr.mode = S_IRUGO | S_IWUSR;

	if (attr->flags & SWITCH_ATTR_READABLE)
		dev_attr->show = switch_show;

	if (attr->flags & SWITCH_ATTR_WRITEABLE)
		dev_attr->store = switch_store;

	ret = device_create_file(sdev->dev, dev_attr);

	attr->handlers = handler_get_list(hdlhead, i);
	mutex_lock(&attr->handlers->mutex);
	if (attr->handlers)
		attr->handlers->attr = attr;
	mutex_unlock(&attr->handlers->mutex);

	INIT_WORK(&attr->wq, _switch_set_state_wq);
}

static void switch_attrs_unregister(struct switch_dev *sdev, struct switch_attr *attr) {
	device_remove_file(sdev->dev, &attr->dev_attr);
}

int switch_dev_register(struct switch_dev *sdev)
{
	int ret;
	unsigned i;
	struct switch_handler_head *hdlhead;

	ret = create_switch_class();
	if (ret)
		return ret;

	sdev->index = atomic_inc_return(&device_count);
	sdev->dev = device_create(switch_class, NULL,
		MKDEV(0, sdev->index), NULL, sdev->name);
	if (IS_ERR(sdev->dev))
		return PTR_ERR(sdev->dev);

	ret = device_create_file(sdev->dev, &dev_attr_name);
	if (ret < 0)
		goto err_create_file;

	hdlhead = handler_get_head(sdev->name);

	printk(KERN_INFO "switch: %s registered with %d attrs\n",
	       sdev->name, sdev->attrs_nr);

	for (i = 0; i < sdev->attrs_nr; i++)
		switch_attrs_register(i, sdev, &sdev->attrs[i], hdlhead);

	dev_set_drvdata(sdev->dev, sdev);

	return 0;

err_create_file:
	device_destroy(switch_class, MKDEV(0, sdev->index));
	return ret;
}
EXPORT_SYMBOL_GPL(switch_dev_register);

void switch_dev_unregister(struct switch_dev *sdev)
{
	unsigned i;

	for (i = 0; i < sdev->attrs_nr; i++)
		switch_attrs_unregister(sdev, &sdev->attrs[i]);
	device_remove_file(sdev->dev, &dev_attr_name);
	device_destroy(switch_class, MKDEV(0, sdev->index));
	dev_set_drvdata(sdev->dev, NULL);
}
EXPORT_SYMBOL_GPL(switch_dev_unregister);

static int __init switch_class_init(void)
{
	return create_switch_class();
}

static void __exit switch_class_exit(void)
{
	class_destroy(switch_class);
}

module_init(switch_class_init);
module_exit(switch_class_exit);

MODULE_AUTHOR("Mike Lockwood <lockwood@android.com>");
MODULE_AUTHOR("Suryandaru Triandana <syndtr@gmail.com>");
MODULE_DESCRIPTION("Switch class driver");
MODULE_LICENSE("GPL");
