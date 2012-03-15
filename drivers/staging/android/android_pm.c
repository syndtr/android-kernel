/*
 * Android PM driver
 * 
 * Copyright (c) 2012 Suryandaru Triandana <syndtr@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/atomic.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/suspend.h>
#include <linux/jiffies.h>

#include "android_pm.h"

enum {
	STATE_RESUMED	= 1,
	STATE_RESUMING,
	STATE_SUSPENDING,
	STATE_SUSPENDED
};

static struct android_pm {
	struct device *dev;
	struct kobject *kobj;
	struct wakeup_source *ws;
	struct mutex mutex_pm;
	struct mutex mutex_ws;
	atomic_t state;
	struct workqueue_struct *wq;
	struct work_struct work;
	struct workqueue_struct *suspend_wq;
	struct work_struct suspend_work;
	wait_queue_head_t wait;
	wait_queue_head_t wait_event;
	atomic_t wait_event_pending;
	struct list_head devices;
	struct list_head suspend_head;
	struct list_head resume_head;
	struct rb_root user_ws;
	unsigned suspend_fail_cnt;
} android_pm_state;

struct android_pm_dev {
	struct device *dev;
	const struct android_pm_ops *ops;
	struct list_head list;
	struct list_head work_list;
};

struct android_pm_user_ws {
	char *name;
	struct rb_node node;
	struct wakeup_source *ws;
};

static struct android_pm_user_ws *user_ws_lookup(struct android_pm *pm,
						 const char *name)
{
	struct rb_root *root = &(pm->user_ws);
	struct rb_node **new = &(root->rb_node), *parent = NULL;
	struct android_pm_user_ws *ws;
	int result;

	mutex_lock(&pm->mutex_ws);
	
	/* Figure out where to put new node */
	while (*new) {
		ws = container_of(*new, struct android_pm_user_ws, node);
		result = strcmp(name, ws->name);

		parent = *new;
		if (result < 0)
			new = &((*new)->rb_left);
		else if (result > 0)
			new = &((*new)->rb_right);
		else
			goto end;
	}

	ws = kmalloc(sizeof(struct android_pm_user_ws) + strlen(name) + 1,
		     GFP_KERNEL);
	if (!ws)
		goto end;

	ws->name = ((char *)ws + sizeof(struct android_pm_user_ws));
	sprintf(ws->name, "%s", name);
	ws->ws = wakeup_source_register(ws->name);
	if (!ws->ws) {
		kfree(ws);
		ws = NULL;
		goto end;
	}

	/* Add new node and rebalance tree. */
	rb_init_node(&ws->node);
	rb_link_node(&ws->node, parent, new);
	rb_insert_color(&ws->node, root);

end:
	mutex_unlock(&pm->mutex_ws);
	return ws;
}

static void user_ws_free(struct android_pm *pm, struct android_pm_user_ws *ws)
{
	mutex_lock(&pm->mutex_ws);
	wakeup_source_unregister(ws->ws);
	rb_erase(&ws->node, &pm->user_ws);
	kfree(ws);
	mutex_unlock(&pm->mutex_ws);
}

static void android_pm_schedule(struct android_pm *pm)
{
	__pm_stay_awake(pm->ws);
	queue_work(pm->wq, &pm->work);
}

int android_pm_enable(struct device *dev, const struct android_pm_ops *ops)
{
	struct android_pm *pm = &android_pm_state;
	struct android_pm_dev *d;
	int ret = 0;
	
	if (!dev)
		return -EINVAL;

	mutex_lock(&pm->mutex_pm);

	list_for_each_entry(d, &pm->devices, list) {
		if (d->dev == dev)
			goto done;
	}

	d = kmalloc(sizeof(struct android_pm_dev), GFP_KERNEL);
	if (d == NULL) {
		ret = -ENOMEM;
		goto done;
	}

	d->dev = dev;
	d->ops = ops;

	list_add(&d->list, &pm->devices);
	list_add(&d->work_list, &pm->suspend_head);
	if (atomic_cmpxchg(&pm->state, STATE_SUSPENDED, STATE_SUSPENDING) == STATE_SUSPENDED)
		android_pm_schedule(pm);

done:
	mutex_unlock(&pm->mutex_pm);
	return ret;
}
EXPORT_SYMBOL_GPL(android_pm_enable);

int android_pm_disable(struct device *dev)
{
	struct android_pm *pm = &android_pm_state;
	struct android_pm_dev *d;
	int ret = -ENODEV;

	mutex_lock(&pm->mutex_pm);

	list_for_each_entry(d, &pm->devices, list) {
		if (d->dev == dev) {
			list_del(&d->list);
			list_del(&d->work_list);
			kfree(d);
			ret = 0;
			goto done;
		}
	}

done:
	mutex_unlock(&pm->mutex_pm);
	return ret;
}
EXPORT_SYMBOL_GPL(android_pm_disable);

static void do_one_suspend(struct android_pm *pm,
			   struct android_pm_dev *d)
{
	int ret;
	if (d->ops && d->ops->suspend) {
		ret = d->ops->suspend(d->dev);
		if (ret < 0)
			pr_warn("android_pm: failed to suspend '%s' ret=%d\n",
			       dev_name(d->dev), ret);
	}
	list_move(&d->work_list, &pm->resume_head);
}

static void do_one_resume(struct android_pm *pm,
			   struct android_pm_dev *d)
{
	int ret;
	if (d->ops && d->ops->resume) {
		ret = d->ops->resume(d->dev);
		if (ret < 0)
			pr_warn("android_pm: failed to resume '%s' ret=%d\n",
			       dev_name(d->dev), ret);
	}
	list_move(&d->work_list, &pm->suspend_head);
}

static void android_pm_work(struct work_struct *work)
{
	struct android_pm *pm = &android_pm_state;
	struct android_pm_dev *d;
	int state;
	
	wait_event(pm->wait_event, !atomic_read(&pm->wait_event_pending));

	mutex_lock(&pm->mutex_pm);
	while (1) {
		if ((state = atomic_read(&pm->state)) == STATE_SUSPENDING) {
			if (list_empty(&pm->suspend_head)) {
				if (atomic_cmpxchg(&pm->state, STATE_SUSPENDING, STATE_SUSPENDED) == STATE_SUSPENDING)
					break;
				else
					continue;
			}

			d = list_first_entry(&pm->suspend_head, struct android_pm_dev, work_list);
			do_one_suspend(pm, d);
		} else if (state == STATE_RESUMING) {
			if (list_empty(&pm->resume_head)) {
				if (atomic_cmpxchg(&pm->state, STATE_RESUMING, STATE_RESUMED) == STATE_RESUMING) {
					wake_up(&pm->wait);
					break;
				} else
					continue;
			}

			d = list_first_entry(&pm->resume_head, struct android_pm_dev, work_list);
			do_one_resume(pm, d);
		} else
			break;
	}
	mutex_unlock(&pm->mutex_pm);

	__pm_relax(pm->ws);
	queue_work(pm->suspend_wq, &pm->suspend_work);
}

static void pm_suspend_work(struct work_struct *work)
{
	struct android_pm *pm = &android_pm_state;
	unsigned int count;
	int ret;

	if (atomic_read(&pm->state) != STATE_SUSPENDED)
		return;

	ret = pm_get_wakeup_count(&count);
	if (atomic_read(&pm->state) == STATE_SUSPENDED) {
		if (ret) {
			pm_save_wakeup_count(count);
			ret = pm_suspend(PM_SUSPEND_MEM);
			if (ret) {
				if (pm->suspend_fail_cnt > 10) {
					pm->suspend_fail_cnt = 1;
					__pm_wakeup_event(pm->ws, 3000);
				} else
					pm->suspend_fail_cnt++;
			} else {
				pm->suspend_fail_cnt = 0;
				__pm_wakeup_event(pm->ws, 200);
			}
		}

		queue_work(pm->suspend_wq, &pm->suspend_work);
	}
}

static ssize_t android_pm_state_show(struct kobject *kobj,
				     struct kobj_attribute *attr,
				     char *buf)
{
	struct android_pm *pm = &android_pm_state;
	const char *state_str = "unknown";

	switch (atomic_read(&pm->state)) {
	case STATE_RESUMED:
		state_str = "resumed";
		break;
	case STATE_RESUMING:
		state_str = "resuming";
		break;
	case STATE_SUSPENDED:
		state_str = "suspended";
		break;
	case STATE_SUSPENDING:
		state_str = "suspending";
		break;
	default:
		break;
	}

	return sprintf(buf, "%s\n", state_str);
}

static ssize_t android_pm_state_store(struct kobject *kobj,
				      struct kobj_attribute *attr,
				      const char *buf, size_t count)
{
	struct android_pm *pm = &android_pm_state;

	if (count < 1)
		return -EINVAL;

	switch (*buf) {
	case 'r':
		if (atomic_cmpxchg(&pm->state, STATE_SUSPENDED, STATE_RESUMING) == STATE_SUSPENDED)
			android_pm_schedule(pm);
		else
			atomic_cmpxchg(&pm->state, STATE_SUSPENDING, STATE_RESUMING);
		break;
	case 's':
		if (atomic_cmpxchg(&pm->state, STATE_RESUMED, STATE_SUSPENDING) == STATE_RESUMED) {
			wake_up(&pm->wait);
			android_pm_schedule(pm);
		} else
			atomic_cmpxchg(&pm->state, STATE_RESUMING, STATE_SUSPENDING);
		break;
	default:
		pr_err("android_pm: unknown state '%c'\n", *buf);
		return -EINVAL;
	}

	return count;
}

static ssize_t android_pm_wait(struct kobject *kobj,
			       struct kobj_attribute *attr,
			       const char *buf, size_t count)
{
	struct android_pm *pm = &android_pm_state;
	int state, ret = 0;

	if (count < 1)
		return -EINVAL;

	switch (*buf) {
	case 'r':
		if (atomic_cmpxchg(&pm->wait_event_pending, 1, 0))
			wake_up(&pm->wait_event);
		ret = wait_event_interruptible(pm->wait,
			(atomic_read(&pm->state) == STATE_RESUMED));
		break;
	case 's':
		atomic_set(&pm->wait_event_pending, 1);
		ret = wait_event_interruptible(pm->wait,
			((state = atomic_read(&pm->state)) == STATE_SUSPENDING ||
			 state == STATE_SUSPENDED));
		break;
	default:
		pr_err("android_pm: unknown wait type '%c'\n", *buf);
		return -EINVAL;
	}

	return ((ret < 0) ? ret : count);
}

static ssize_t android_pm_wakeup_ref(struct kobject *kobj,
				     struct kobj_attribute *attr,
				     const char *buf, size_t count)
{
	struct android_pm *pm = &android_pm_state;
	struct android_pm_user_ws *ws;

	if (count == 0 || !buf)
		return -EINVAL;

	ws = user_ws_lookup(pm, buf);
	if (!ws)
		return -ENOMEM;

	__pm_stay_awake(ws->ws);

	return count;
}

static ssize_t android_pm_wakeup_unref(struct kobject *kobj,
				       struct kobj_attribute *attr,
				       const char *buf, size_t count)
{
	struct android_pm *pm = &android_pm_state;
	struct android_pm_user_ws *ws;
	
	if (count == 0 || !buf)
		return -EINVAL;

	ws = user_ws_lookup(pm, buf);
	if (!ws)
		return -ENOMEM;

	__pm_relax(ws->ws);

	return count;
}

static struct kobj_attribute state_attribute =
	__ATTR(state, 0644, android_pm_state_show, android_pm_state_store);
static struct kobj_attribute wait_attribute =
	__ATTR(wait, 0666, NULL, android_pm_wait);
static struct kobj_attribute wakeup_ref_attribute =
	__ATTR(wakeup_ref, 0644, NULL, android_pm_wakeup_ref);
static struct kobj_attribute wakeup_unref_attribute =
	__ATTR(wakeup_unref, 0644, NULL, android_pm_wakeup_unref);

static struct attribute *attrs[] = {
	&state_attribute.attr,
	&wait_attribute.attr,
	&wakeup_ref_attribute.attr,
	&wakeup_unref_attribute.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static int __init android_pm_init(void)
{
	struct android_pm *pm = &android_pm_state;
	int ret;

	pm->kobj = kobject_create_and_add("android_pm", power_kobj);
	if (!pm->kobj)
		return -ENOMEM;

	ret = sysfs_create_group(pm->kobj, &attr_group);
	if (ret)
		goto err;
	
	pm->ws = wakeup_source_register("android_pm");
	if (!pm->ws) {
		ret = -ENOMEM;
		goto err;
	}

	pm->wq = create_singlethread_workqueue("android_pm");
	if (!pm->wq) {
		ret = -ENOMEM;
		goto err_ws;
	}

	pm->suspend_wq = create_singlethread_workqueue("android_pm_suspend");
	if (!pm->suspend_wq) {
		ret = -ENOMEM;
		goto err_wq;
	}

	mutex_init(&pm->mutex_pm);
	mutex_init(&pm->mutex_ws);
	atomic_set(&pm->state, STATE_RESUMED);
	INIT_LIST_HEAD(&pm->devices);
	INIT_LIST_HEAD(&pm->suspend_head);
	INIT_LIST_HEAD(&pm->resume_head);
	INIT_WORK(&pm->work, android_pm_work);
	INIT_WORK(&pm->suspend_work, pm_suspend_work);
	init_waitqueue_head(&pm->wait);
	init_waitqueue_head(&pm->wait_event);
	atomic_set(&pm->wait_event_pending, 0);
	pm->suspend_fail_cnt = 0;

	return 0;

err_wq:
	destroy_workqueue(pm->wq);
err_ws:
	wakeup_source_unregister(pm->ws);
err:
	kobject_put(pm->kobj);
	return ret;
}

postcore_initcall(android_pm_init);
