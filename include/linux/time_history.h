#ifndef _LINUX_TIME_HISTORY_H
#define _LINUX_TIME_HISTORY_H

#include <linux/ktime.h>
#include <linux/alarmtimer.h>
#include <linux/rtc.h>

enum time_history_type {
	TIME_HISTORY_TYPE_TIME_SET = 0,
	TIME_HISTORY_TYPE_RTC_TIME_SET,
	TIME_HISTORY_TYPE_HOST_TIME_SET,
	TIME_HISTORY_TYPE_NETWORK_TIME_SET,
	TIME_HISTORY_TYPE_TIMEZONE_SET,
	TIME_HISTORY_TYPE_NITZ_UPDATE_SET,
	TIME_HISTORY_TYPE_ALARM_START,
	TIME_HISTORY_TYPE_ALARM_RESTART,
	TIME_HISTORY_TYPE_ALARM_EXPIRED,
	TIME_HISTORY_TYPE_ALARM_DEL,
	TIME_HISTORY_TYPE_RTC_ALARM_SET,
	TIME_HISTORY_TYPE_MAX,
};

#ifdef CONFIG_TIME_HISTORY
extern void __time_history_alarm_init(const struct alarm *alarm, void *caller);
extern void __time_history_alarm_start(const struct alarm *alarm, void *caller);
extern void __time_history_alarm_restart(const struct alarm *alarm, void *caller);
extern void __time_history_alarm_expired(const struct alarm *alarm, ktime_t now);
extern void __time_history_alarm_del(const struct alarm *alarm, void *caller);
extern void __time_history_time_set(const struct timespec *oldtime,
		const struct timespec *newtime, void *caller);
extern void __time_history_rtc_time_set(const struct timespec *newtime,
		void *caller, int err);
extern void __time_history_rtc_alarm_init(const struct rtc_timer *timer,
		void *caller);
extern void __time_history_rtc_alarm_set(struct rtc_device *rtc,
		const struct rtc_wkalrm *wkalrm, void *caller, int err);

extern void __time_history_marker_system_rtc(const struct timespec *system,
		const struct rtc_time *tm);

static inline void time_history_alarm_init(const struct alarm *alarm)
{
	if (unlikely(!alarm))
		return;

	__time_history_alarm_init(alarm, __builtin_return_address(0));
}

static inline void time_history_alarm_start(const struct alarm *alarm)
{
	if (unlikely(!alarm))
		return;

	__time_history_alarm_start(alarm, __builtin_return_address(0));
}

static inline void time_history_alarm_restart(const struct alarm *alarm)
{
	if (unlikely(!alarm))
		return;

	__time_history_alarm_restart(alarm, __builtin_return_address(0));
}

static inline void time_history_alarm_expired(const struct alarm *alarm,
		ktime_t now)
{
	if (unlikely(!alarm))
		return;

	__time_history_alarm_expired(alarm, now);
}

static inline void time_history_alarm_del(const struct alarm *alarm)
{
	if (unlikely(!alarm))
		return;

	__time_history_alarm_del(alarm, __builtin_return_address(0));
}

static inline void time_history_time_set(const struct timespec *oldtime,
		const struct timespec *newtime)
{
	if (unlikely(!oldtime || !newtime))
		return;


	__time_history_time_set(oldtime, newtime, __builtin_return_address(0));
}

static inline void time_history_rtc_time_set(const struct rtc_time *tm, int err)
{
	struct timespec newtime;

	if (unlikely(!tm))
		return;

	newtime = ktime_to_timespec(rtc_tm_to_ktime(*tm));
	__time_history_rtc_time_set(&newtime, __builtin_return_address(0), err);
}

static inline void time_history_rtc_alarm_init(struct rtc_timer *timer)
{
	if (unlikely(!timer))
		return;

	__time_history_rtc_alarm_init(timer, __builtin_return_address(0));
	return;
}

static inline void time_history_rtc_alarm_set(struct rtc_device *rtc,
		struct rtc_wkalrm *wkalrm, int err)
{
	if (unlikely(!rtc || !wkalrm))
		return;

	__time_history_rtc_alarm_set(rtc, wkalrm, __builtin_return_address(0), err);
	return;
}

static inline void time_history_marker_system_rtc(const struct timespec *system,
		const struct rtc_time *tm)
{
	if (unlikely(!system || !tm))
		return;

	__time_history_marker_system_rtc(system, tm);
	return;
}
#else /* !CONFIG_TIME_HISTORY */
static inline void time_history_alarm_init(const struct alarm *alarm)
{
	return;
}
static inline void time_history_alarm_start(const struct alarm *alarm)
{
	return;
}
static inline void time_history_alarm_restart(const struct alarm *alarm)
{
	return;
}
static inline void time_history_alarm_expired(const struct alarm *alarm,
		ktime_t now)
{
	return;
}
static inline void time_history_alarm_del(const struct alarm *alarm)
{
	return;
}
static inline void time_history_time_set(const struct timespec *oldtime,
		const struct timespec *newtime)
{
	return;
}
static inline void time_history_rtc_time_set(const struct rtc_time *tm, int err)
{
	return;
}
static inline void time_history_rtc_alarm_init(struct rtc_timer *timer)
{
	return;
}
static inline void time_history_rtc_alarm_set(struct rtc_device *rtc,
		struct rtc_wkalrm *wkalrm, int err)
{
	return;
}
static inline void time_history_marker_system_rtc(const struct timespec *system,
		const struct rtc_time *tm)
{
	return;
}
#endif /* CONFIG_TIME_HISTORY */

#ifdef CONFIG_SUPPORT_ALARM_TIMEZONE_DST
extern int alarm_get_tz(void);
#else
static inline int alarm_get_tz(void){return 0;}
#endif

#endif /* _LINUX_TIME_HISTORY_H */
