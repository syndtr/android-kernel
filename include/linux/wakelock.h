/* include/linux/wakelock.h
 *
 * Copyright (C) 2007-2008 Google, Inc.
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

#ifndef _LINUX_WAKELOCK_H
#define _LINUX_WAKELOCK_H

#include <linux/device.h>
#include <linux/suspend.h>
#include <linux/jiffies.h>

/* A wake_lock prevents the system from entering suspend or other low power
 * states when active. If the type is set to WAKE_LOCK_SUSPEND, the wake_lock
 * prevents a full system suspend. If the type is WAKE_LOCK_IDLE, low power
 * states that cause large interrupt latencies or that disable a set of
 * interrupts will not entered from idle until the wake_locks are released.
 */

enum {
	WAKE_LOCK_SUSPEND, /* Prevent suspend */
	WAKE_LOCK_IDLE,    /* Prevent low power idle */
	WAKE_LOCK_TYPE_COUNT
};

struct wake_lock {
	struct wakeup_source *ws;
};

#ifdef CONFIG_HAS_WAKELOCK

static inline void wake_lock_init(struct wake_lock *lock, int type, const char *name)
{
	if (type != WAKE_LOCK_SUSPEND) {
		lock->ws = NULL;
		printk(KERN_ERR "invalid wakelock type: %d\n", type);
		return;
	}
	lock->ws = wakeup_source_register(name);
}

static inline void wake_lock_destroy(struct wake_lock *lock)
{
	if (lock->ws)
		wakeup_source_unregister(lock->ws);
}

static inline void wake_lock(struct wake_lock *lock)
{
	if (lock->ws)
		__pm_stay_awake(lock->ws);
}

static inline void wake_lock_timeout(struct wake_lock *lock, long timeout)
{
	if (lock->ws)
		__pm_wakeup_event(lock->ws, jiffies_to_msecs(timeout));
}

static inline void wake_unlock(struct wake_lock *lock)
{
	if (lock->ws)
		__pm_relax(lock->ws);
}

static inline int wake_lock_active(struct wake_lock *lock)
{
	if (!lock->ws)
		return 0;
	return lock->ws->active;
}

static inline long has_wake_lock(int type)
{
	if (type != WAKE_LOCK_SUSPEND)
		return 0;

	return pm_wakeup_pending();
}

#else

static inline void wake_lock_init(struct wake_lock *lock, int type,
					const char *name) {}
static inline void wake_lock_destroy(struct wake_lock *lock) {}
static inline void wake_lock(struct wake_lock *lock) {}
static inline void wake_lock_timeout(struct wake_lock *lock, long timeout) {}
static inline void wake_unlock(struct wake_lock *lock) {}

static inline int wake_lock_active(struct wake_lock *lock) { return 0; }
static inline long has_wake_lock(int type) { return 0; }

#endif

#endif

