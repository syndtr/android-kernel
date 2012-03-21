
#ifndef _ANDROID_PM_H
#define _ANDROID_PM_H

#ifdef CONFIG_ANDROID_PM

struct android_pm_ops {
	int (*suspend)(struct device *dev);
	int (*resume)(struct device *dev);
};

#define SET_ANDROID_PM_SLEEP_OPS(suspend_fn, resume_fn) \
	.suspend = suspend_fn, \
	.resume = resume_fn,

#define ANDROID_PM_OPS(name, suspend_fn, resume_fn) \
const struct android_pm_ops name = { \
	SET_ANDROID_PM_SLEEP_OPS(suspend_fn, resume_fn) \
}
#define STATIC_ANDROID_PM_OPS(name, suspend_fn, resume_fn) \
	static ANDROID_PM_OPS(name, suspend_fn, resume_fn)

#define ANDROID_PM_OPS_PROTO(name) \
	const struct android_pm_ops name
#define STATIC_ANDROID_PM_OPS_PROTO(name) \
	static ANDROID_PM_OPS_PROTO(name)

extern int android_pm_enable(struct device *dev, const struct android_pm_ops *ops);
extern int android_pm_disable(struct device *dev);

#else

#define SET_ANDROID_PM_SLEEP_OPS(suspend_fn, resume_fn)

#define ANDROID_PM_OPS(name, suspend_fn, resume_fn)
#define STATIC_ANDROID_PM_OPS(name, suspend_fn, resume_fn)

#define ANDROID_PM_OPS_PROTO(name)
#define STATIC_ANDROID_PM_OPS_PROTO(name)

static inline int ___android_pm_none(void) { return 0; }
#define android_pm_enable(dev, ops) ___android_pm_none()
#define android_pm_disable(dev) ___android_pm_none()

#endif

#endif // _ANDROID_PM_H