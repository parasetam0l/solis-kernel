#include <linux/time_history.h>
#include <linux/time.h>
#include <linux/hrtimer.h>
#include <linux/alarmtimer.h>
#include <linux/sched.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/rtc.h>
#include <linux/seqlock.h>
#include <linux/printk.h>

#ifdef CONFIG_SLEEP_MONITOR
#include <linux/power/sleep_monitor.h>
#endif

#ifdef CONFIG_SLP_KERNEL_ENG
	#define TIME_LOG_MAX	(1000)
	#define ALARM_LOG_MAX	(3000)
#else
	#define TIME_LOG_MAX	(500)
	#define ALARM_LOG_MAX	(1500)
#endif

#define ALARM_ID_MAX		(32)
#define OWNER_LEN			(32)

#define for_each_alarm(_alarm_table, _alarm)	 \
	for (_alarm = _alarm_table;					 \
		(_alarm < _alarm_table + ALARM_ID_MAX) && (_alarm->alarmid); \
			_alarm++)

#define ARRAYSIZE(arr) (sizeof(arr) / sizeof(arr[0]))

struct time_log_entry {
	u64 history_idx;
	enum time_history_type history_type;
	void *alarmid;
	struct timespec realtime;
	struct timespec monotonic;
#ifdef CONFIG_TIME_HISTORY_SAVE_BOOTTIME
	struct timespec boottime;
#endif
	struct timespec oldtime;
	struct timespec newtime;
	time_t tz_offset;
	int err;
	void *caller;
	char comm[TASK_COMM_LEN + 1];
	pid_t pid;
};

struct time_history_log_buf {
	struct time_log_entry *buf;
	int head;
	int tail;
	int buf_size;
	u64 cnt;
};

struct alarm_info {
	void *alarmid;
	void *caller;
	void *handler;
	char owner[OWNER_LEN + 1];
};

struct time_history {
	u64 history_cnt;
	struct alarm_info alarm_table[ALARM_ID_MAX];
	int alarmid_cnt;
	spinlock_t lock;
	char timezone[8];
	char timezone_id[16];
	time_t tz_offset;
	int nitz_update;
};

static struct time_log_entry time_log_buf[TIME_LOG_MAX];
static struct time_log_entry alarm_log_buf[ALARM_LOG_MAX];

static struct time_history_log_buf time_log = {
	.buf  = time_log_buf,
	.buf_size = ARRAYSIZE(time_log_buf), }; 
static struct time_history_log_buf alarm_log = {
	.buf  = alarm_log_buf,
	.buf_size = ARRAYSIZE(alarm_log_buf),
};

static struct time_history th_ctxt = {
	.timezone = "+00:00",
	.timezone_id = "UTC",
	.nitz_update = -1,
};

static DEFINE_SEQLOCK(th_seq_lock);

static struct timespec th_old_system;
static struct rtc_time th_old_tm;

#ifdef CONFIG_RTC_CLASS
static struct rtc_device *th_rtcdev;
static DEFINE_SPINLOCK(th_rtcdev_lock);

struct rtc_device *timeh_get_rtcdev(void)
{
	unsigned long flags;
	struct rtc_device *ret;

	spin_lock_irqsave(&th_rtcdev_lock, flags);
	ret = th_rtcdev;
	spin_unlock_irqrestore(&th_rtcdev_lock, flags);

	return ret;
}

static int timeh_rtc_add_device(struct device *dev,
				struct class_interface *class_intf)
{
	unsigned long flags;
	struct rtc_device *rtc = to_rtc_device(dev);

	if (th_rtcdev)
		return -EBUSY;

	if (!rtc->ops->read_time)
		return -1;

	if (strcmp(dev_name(dev),
			CONFIG_RTC_HCTOSYS_DEVICE) != 0) {
		pr_err("Miss-matched rtc(%s) for timeh\n", dev_name(dev));
		return -1;
	}

	spin_lock_irqsave(&th_rtcdev_lock, flags);
	if (!th_rtcdev) {
		th_rtcdev = rtc;
		/* hold a reference so it doesn't go away */
		get_device(dev);
	}
	spin_unlock_irqrestore(&th_rtcdev_lock, flags);
	return 0;
}

static struct class_interface timeh_rtc_interface = {
	.add_dev = &timeh_rtc_add_device,
};

static int timeh_rtc_interface_setup(void)
{
	timeh_rtc_interface.class = rtc_class;
	return class_interface_register(&timeh_rtc_interface);
}

#else
#define th_rtcdev (NULL)

struct rtc_device *timeh_get_rtcdev(void)
{
	return NULL;
}

static inline int timeh_rtc_interface_setup(void) { return 0; }
#endif

#ifdef CONFIG_SLEEP_MONITOR
static int timeh_get_sys_time_cb(void* priv,
			unsigned int *raw_val, int chk_lv, int caller_type)
{
	struct timespec ts;

	ts = current_kernel_time();
	*raw_val = ts.tv_sec;

	return 0;
}

static struct sleep_monitor_ops timeh_sys_time_ops = {
	 .read_cb_func = timeh_get_sys_time_cb,
};

static int timeh_get_rtc_time_cb(void* priv,
			unsigned int *raw_val, int chk_lv, int caller_type)
{
	struct rtc_time tm;
	struct rtc_device *rtc_dev;
	struct timespec ts, curr_syst, delta_syst;
	int err = 0;
	unsigned int seq;

	rtc_dev = timeh_get_rtcdev();
	if (!rtc_dev)
		return -ENODEV;

	if (chk_lv == SLEEP_MONITOR_CHECK_SOFT) {
		do {
			seq = read_seqbegin(&th_seq_lock);
			curr_syst = current_kernel_time();
			delta_syst.tv_sec = curr_syst.tv_sec - th_old_system.tv_sec;
			if (delta_syst.tv_sec < 0) {
				pr_warn("%s:system time back-travel\n", __func__);
				return -EINVAL;
			}
			rtc_tm_to_time(&th_old_tm, &ts.tv_sec);
			ts.tv_sec += delta_syst.tv_sec;
		} while (read_seqretry(&th_seq_lock, seq));
	} else {
		err = rtc_read_time(rtc_dev, &tm);
		if (err)
			return err;
		rtc_tm_to_time(&tm, &ts.tv_sec);
	}

	*raw_val = ts.tv_sec;
	return 0;
}

static struct sleep_monitor_ops timeh_rtc_time_ops = {
	 .read_cb_func = timeh_get_rtc_time_cb,
};

static void timeh_sleep_monitor_cb_setup(void)
{
	int err;

	err = sleep_monitor_register_ops(NULL,
			&timeh_sys_time_ops, SLEEP_MONITOR_SYS_TIME);
	if (err)
		pr_err("%s:failed SYSTIME sm_cb(%d)\n",
				__func__, err);

	err = sleep_monitor_register_ops(NULL,
			&timeh_rtc_time_ops, SLEEP_MONITOR_RTC_TIME);
	if (err)
		pr_err("%s:failed RTCTIME sm_cb(%d)\n",
				__func__, err);
}
#else
static inline void timeh_sleep_monitor_cb_setup(void) { }
#endif

static bool is_realtime(struct timespec *time)
{
	const struct timespec realtime = {
		.tv_sec = 946684800, // 2000-01-01 00:00:00 //
		.tv_nsec = 0,
	};
	/*
	 * lhs == rhs: return  0
	 * lhs  > rhs: return >0
	 */
	return (timespec_compare(time, &realtime) >= 0);
}

static void set_alarm_owner(struct alarm_info *alarm_info, void *caller)
{
	const char *remove_str[] = {
		"_init",
		"_probe",
		"_register",
	};
	char *pos;
	int i;

	snprintf(alarm_info->owner, OWNER_LEN, "%pf", caller);

	for (i = 0; i < ARRAYSIZE(remove_str); i++) {
		pos = strstr(alarm_info->owner, remove_str[i]);
		if (pos && pos != alarm_info->owner)
			*pos = '\0';
	}
}

static struct alarm_info *time_history_get_alarm_by_id(const void *alarmid)
{
	struct alarm_info *alarm;

	for_each_alarm(th_ctxt.alarm_table, alarm) {
		if (alarmid == alarm->alarmid)
			return alarm;
	}

	return NULL;
}

static int time_history_ringbuf_tail(struct time_history_log_buf *log_buf)
{
	/* overwrite ring buf */
	if (log_buf->cnt >= log_buf->buf_size)
		log_buf->head++;

	if (log_buf->head >= log_buf->buf_size)
		log_buf->head = 0;

	if (log_buf->cnt)
		log_buf->tail++;

	if (log_buf->tail >= log_buf->buf_size)
		log_buf->tail = 0;

	log_buf->cnt++;

	return log_buf->tail;
}

static time_t time_history_timezone_offset(char *timezone)
{
	int tz_hour, tz_min;

	/* except first char '+' or '-' */
	if (sscanf(&timezone[1], "%d:%d", &tz_hour, &tz_min) < 0)
		return 0;

	if (tz_hour == 0 && tz_min == 0)
		return 0;

	if (timezone[0] == '-') {
		tz_hour = 0 - tz_hour;
		tz_min  = 0 - tz_min;
	}

	return ((tz_hour * 3600) + (tz_min * 60));
}

static struct time_log_entry *time_history_init_log_entry(
		struct time_history_log_buf *log_buf)
{
	struct time_log_entry *entry;
	int tail;

	tail = time_history_ringbuf_tail(log_buf);
	entry = &log_buf->buf[tail];

	memset(entry, 0, sizeof(struct time_log_entry));

	entry->history_idx = th_ctxt.history_cnt;
	entry->realtime  = current_kernel_time();
	entry->tz_offset = th_ctxt.tz_offset;
	ktime_get_ts(&entry->monotonic);
#ifdef CONFIG_TIME_HISTORY_SAVE_BOOTTIME
	get_monotonic_boottime(&entry->boottime);
#endif
	return entry;
}

#ifdef CONFIG_TIME_HISTORY_LOG_FILTER
static const char *alarm_owner_filter[] = {
	"sec_battery",
};

static int time_history_check_log_filter(const void *alarmid)
{
	struct alarm_info *alarm_info;
	int i;

	alarm_info = time_history_get_alarm_by_id(alarmid);
	if (!alarm_info)
		return 0;

	for (i = 0; i < ARRAYSIZE(alarm_owner_filter); i++) {
		if (strcasecmp(alarm_owner_filter[i], alarm_info->owner) == 0)
			return 1;
	}
	return 0;
}
#endif /* CONFIG_TIME_HISTORY_LOG_FILTER */

static int time_history_insert_alarm_log(enum time_history_type type,
		const struct alarm *alarm, void *caller)
{
	struct time_log_entry *entry;
	unsigned long flags;

#ifdef CONFIG_TIME_HISTORY_LOG_FILTER
	if (time_history_check_log_filter(&alarm->node.node) != 0)
		return 0;
#endif

	spin_lock_irqsave(&th_ctxt.lock, flags);
	entry = time_history_init_log_entry(&alarm_log);
	entry->history_type = type;
	entry->newtime = ktime_to_timespec(alarm->node.expires);
	entry->caller = caller;
	entry->alarmid = (void*)&alarm->node.node;
	entry->pid = current->pid;
	memcpy(entry->comm, current->comm, TASK_COMM_LEN);

	th_ctxt.history_cnt++;
	spin_unlock_irqrestore(&th_ctxt.lock, flags);

	return 0;
}

static int time_history_insert_time_log(enum time_history_type type,
		const struct timespec *oldtime, const struct timespec *newtime,
		void *caller, int err)
{
	struct time_log_entry *entry;
	unsigned long flags;

	spin_lock_irqsave(&th_ctxt.lock, flags);
	entry = time_history_init_log_entry(&time_log);
	entry->history_type = type;
	if (type == TIME_HISTORY_TYPE_TIME_SET)
		entry->realtime = *oldtime;
	entry->oldtime = *oldtime;
	entry->newtime = *newtime;
	entry->caller = caller;
	entry->pid = current->pid;
	entry->err = err;
	memcpy(entry->comm, current->comm, TASK_COMM_LEN);

	th_ctxt.history_cnt++;
	spin_unlock_irqrestore(&th_ctxt.lock, flags);

	return 0;
}

static int time_history_insert_timezone_log(char *old_tz, char *new_tz)
{
	struct time_log_entry *entry;
	unsigned long flags;

	spin_lock_irqsave(&th_ctxt.lock, flags);
	entry = time_history_init_log_entry(&time_log);
	entry->history_type = TIME_HISTORY_TYPE_TIMEZONE_SET;
	snprintf(entry->comm, sizeof(entry->comm) - 1, "%s > %s", old_tz, new_tz);

	th_ctxt.history_cnt++;
	spin_unlock_irqrestore(&th_ctxt.lock, flags);

	return 0;
}

static int time_history_insert_setting_log(enum time_history_type type,
		int old, int new)
{
	struct time_log_entry *entry;
	unsigned long flags;

	spin_lock_irqsave(&th_ctxt.lock, flags);
	entry = time_history_init_log_entry(&time_log);
	entry->history_type = type;
	entry->oldtime.tv_sec = old;
	entry->newtime.tv_sec = new;

	th_ctxt.history_cnt++;
	spin_unlock_irqrestore(&th_ctxt.lock, flags);

	return 0;
}

void __time_history_marker_system_rtc(const struct timespec *system,
		const struct rtc_time *tm)
{
	write_seqlock(&th_seq_lock);
	th_old_system = *system;
	th_old_tm = *tm;
	write_sequnlock(&th_seq_lock);

	return;
}

void __time_history_alarm_init(const struct alarm *alarm, void *caller)
{
	struct alarm_info *alarm_info;

	if (th_ctxt.alarmid_cnt >= ALARM_ID_MAX) {
		pr_err("%s: no space in alarm id table\n", __func__);
		return;
	}

	alarm_info = &th_ctxt.alarm_table[th_ctxt.alarmid_cnt++];

	alarm_info->alarmid = (void*)(&alarm->node.node);
	alarm_info->caller  = caller;
	alarm_info->handler = alarm->function;
	set_alarm_owner(alarm_info, caller);

	return;
}

void __time_history_alarm_start(const struct alarm *alarm, void *caller)
{
	time_history_insert_alarm_log( TIME_HISTORY_TYPE_ALARM_START, alarm, caller);
}

void __time_history_alarm_restart(const struct alarm *alarm, void *caller)
{
	time_history_insert_alarm_log(TIME_HISTORY_TYPE_ALARM_RESTART, alarm, caller);
}

void __time_history_alarm_expired(const struct alarm *alarm, ktime_t now)
{
	time_history_insert_alarm_log(TIME_HISTORY_TYPE_ALARM_EXPIRED,
			alarm, __builtin_return_address(0));
}

void __time_history_alarm_del(const struct alarm *alarm, void *caller)
{
	if (time_history_get_alarm_by_id(&alarm->node.node) == NULL)
		return;

	time_history_insert_alarm_log(TIME_HISTORY_TYPE_ALARM_DEL, alarm, caller);
}

void __time_history_time_set(const struct timespec *oldtime,
		const struct timespec *newtime, void *caller)
{
	time_history_insert_time_log(TIME_HISTORY_TYPE_TIME_SET,
			oldtime, newtime, caller, 0);
}

void __time_history_rtc_time_set(const struct timespec *newtime,
		void *caller, int err)
{
	time_history_insert_time_log(TIME_HISTORY_TYPE_RTC_TIME_SET,
			newtime, newtime, caller, err);
}

void __time_history_rtc_alarm_init(const struct rtc_timer *timer,
		void *caller)
{
	struct alarm_info *alarm_info;

	if (th_ctxt.alarmid_cnt >= ALARM_ID_MAX) {
		pr_err("%s: no space in alarm id table\n", __func__);
		return;
	}

	alarm_info = &th_ctxt.alarm_table[th_ctxt.alarmid_cnt++];

	alarm_info->alarmid = (void*)(&timer->node.node);
	alarm_info->caller  = caller;
	alarm_info->handler = timer->task.func;
	set_alarm_owner(alarm_info, caller);

	return;
}

void __time_history_rtc_alarm_set(struct rtc_device *rtc,
		const struct rtc_wkalrm *wkalrm, void *caller, int err)
{
	struct timerqueue_node *tq_node;
	struct alarm_info *alarm;
	struct time_log_entry *entry;
	unsigned long flags;

	tq_node = timerqueue_getnext(&rtc->timerqueue);
	if (!tq_node) {
		pr_err("%s: timerqueue is empty\n", __func__);
		return;
	}

	alarm = time_history_get_alarm_by_id(&tq_node->node);
	if (!alarm) {
		pr_err("%s: can't find alarm\n", __func__);
		return;
	}

#ifdef CONFIG_TIME_HISTORY_LOG_FILTER
	if (time_history_check_log_filter(&tq_node->node) != 0)
		return;
#endif

	spin_lock_irqsave(&th_ctxt.lock, flags);
	entry = time_history_init_log_entry(&alarm_log);
	entry->history_type = TIME_HISTORY_TYPE_RTC_ALARM_SET;
	entry->newtime = ktime_to_timespec(tq_node->expires);
	entry->caller  = caller;
	entry->alarmid = (void*)&tq_node->node;
	entry->pid = current->pid;
	entry->err = err;
	memcpy(entry->comm, current->comm, TASK_COMM_LEN);

	th_ctxt.history_cnt++;
	spin_unlock_irqrestore(&th_ctxt.lock, flags);

	return;
}

static const char *history_type_name[] = {
	[TIME_HISTORY_TYPE_TIME_SET]          = "time_set",
	[TIME_HISTORY_TYPE_RTC_TIME_SET]      = "rtc_time_set",
	[TIME_HISTORY_TYPE_HOST_TIME_SET]     = "host_time",
	[TIME_HISTORY_TYPE_NETWORK_TIME_SET]  = "network_time",
	[TIME_HISTORY_TYPE_TIMEZONE_SET]      = "timezone_set",
	[TIME_HISTORY_TYPE_NITZ_UPDATE_SET]   = "nitz_update",
	[TIME_HISTORY_TYPE_ALARM_START]       = "alarm_start",
	[TIME_HISTORY_TYPE_ALARM_RESTART]     = "alarm_restart",
	[TIME_HISTORY_TYPE_ALARM_EXPIRED]     = "alarm_expired",
	[TIME_HISTORY_TYPE_ALARM_DEL]         = "alarm_delete",
	[TIME_HISTORY_TYPE_RTC_ALARM_SET]     = "rtc_alarm_set",
};

struct time_history_iter {
	struct time_history_log_buf *curr_log;
	int curr_idx;
	struct time_history_log_buf *comp_log;
	int comp_idx;
};

static char owner_filter[32] = "-";

static inline void seq_print_difftime(struct seq_file *seq,
		struct timespec old, struct timespec new)
{
	struct timespec diff = timespec_sub(new, old);

	if (diff.tv_sec < 0 && diff.tv_nsec > 0) {
		/* It's normalized time, convert to human readable time format */
		diff.tv_nsec = -(diff.tv_nsec - NSEC_PER_SEC);
		diff.tv_sec++;
	}

	seq_printf(seq, " (%s%ld.%03ld)", (diff.tv_sec >= 0)? "+" : "",
			diff.tv_sec, diff.tv_nsec/NSEC_PER_MSEC);
	return;
}

static inline void seq_print_realtime(struct seq_file *seq, struct timespec *ts,
		time_t tz_offset)
{
	struct tm tm;

	time_to_tm(ts->tv_sec + tz_offset, 0, &tm);

	seq_printf(seq, "%4ld-%02d-%02d %02d:%02d:%02d",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec);
}

static void time_history_show_header(struct seq_file *seq)
{
	struct timespec ts;
	char *tz, *tz_id, *nitz;
	struct rtc_device *rtc_dev;
	unsigned long time;

	tz    = th_ctxt.timezone;
	tz_id = th_ctxt.timezone_id;

	switch (th_ctxt.nitz_update) {
		case 0:
			nitz = "off";
			break;
		case 1:
			nitz = "on";
			break;
		default:
			nitz = "Unknown";
	}

	getnstimeofday(&ts);
	seq_puts(seq, "\n********** time_history v0.4 **********\n\n");
	seq_puts(seq, "system time : ");
	seq_print_realtime(seq, &ts, th_ctxt.tz_offset);
	seq_printf(seq, " (%ld.%03ld)\n", ts.tv_sec, ts.tv_nsec/NSEC_PER_MSEC);

	rtc_dev = timeh_get_rtcdev();
	if (rtc_dev) {
		struct rtc_time tm;
		rtc_read_time(rtc_dev, &tm);
		rtc_tm_to_time(&tm, &time);
		seq_printf(seq, "rtc time    : %d-%02d-%02d %02d:%02d:%02d (%ld)\n",
				tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
				tm.tm_hour, tm.tm_min, tm.tm_sec, time);
	}

	seq_puts(seq, "Marked system (UTC0): ");
	seq_print_realtime(seq, &th_old_system, 0);
	seq_printf(seq, " (%ld.%03ld)\n", th_old_system.tv_sec,
					th_old_system.tv_nsec/NSEC_PER_MSEC);

	rtc_tm_to_time(&th_old_tm, &time);
	seq_printf(seq, "Marked rtc (UTC0)   : %d-%02d-%02d %02d:%02d:%02d (%ld)\n",
			th_old_tm.tm_year+1900, th_old_tm.tm_mon+1, th_old_tm.tm_mday,
			th_old_tm.tm_hour, th_old_tm.tm_min, th_old_tm.tm_sec, time);

	seq_printf(seq, "timezone    : %s (%s)\n", tz_id, tz);
	seq_printf(seq, "nitz update : %s\n", nitz);

	seq_printf(seq, "total alarmtimer : %4d/%-lu\n",
			th_ctxt.alarmid_cnt, ARRAYSIZE(th_ctxt.alarm_table));
	seq_printf(seq, "total time set   : %4lld/%-d\n",
			time_log.cnt, time_log.buf_size);
	seq_printf(seq, "total alarm log  : %4lld/%-d\n\n",
			alarm_log.cnt, alarm_log.buf_size);

#ifdef CONFIG_TIME_HISTORY_DEBUG
	{
		struct alarm_info *alarm;
		seq_printf(seq, "log_entry sz: %d\n\n", sizeof(struct time_log_entry));
		for_each_alarm(th_ctxt.alarm_table, alarm) {
			seq_printf(seq, "id      : %p\n", alarm->alarmid);
			seq_printf(seq, "owner   : %s\n", alarm->owner);
			seq_printf(seq, "handler : %pf\n", alarm->handler);
			seq_printf(seq, "caller  : %pf\n\n", alarm->caller);
		}
	}
#endif
	seq_puts(seq, "      real_time       tz   [ kernel_time ");
#ifdef CONFIG_TIME_HISTORY_SAVE_BOOTTIME
	seq_puts(seq, " / boot_time");
#endif
	seq_puts(seq, "]  idx  type           worker            "
			"function                time                  epoch"
			"       diff\n");
}

static inline void seq_print_timestamp(struct seq_file *seq,
		struct time_log_entry *entry)
{
	char tz[8] = "+00:00";

	if (entry->tz_offset) {
		int tz_hour, tz_min;
		time_t tz_offset;
		tz_offset = (entry->tz_offset < 0)? -(entry->tz_offset) : entry->tz_offset;
		tz_hour = tz_offset/3600;
		tz_min  = (tz_offset%3600)/60;
		snprintf(tz, sizeof(tz) - 1, "%c%02d:%02d",
				(entry->tz_offset < 0)? '-' : '+', tz_hour, tz_min);
	}

	seq_print_realtime(seq, &entry->realtime, entry->tz_offset);
	seq_printf(seq, " %s [%6lu.%06lu", tz,
			entry->monotonic.tv_sec, entry->monotonic.tv_nsec/NSEC_PER_USEC);
#ifdef CONFIG_TIME_HISTORY_SAVE_BOOTTIME
	seq_printf(seq, " /  %5lu.%03lu",
			entry->boottime.tv_sec, entry->boottime.tv_nsec/NSEC_PER_MSEC);
#endif
	seq_puts(seq, "] ");
}

static int time_history_show(struct seq_file *seq, void *v)
{
	struct time_history_iter *iter = seq->private;
	struct time_log_entry *entry;
	struct alarm_info *alarm = NULL;
	enum {
		TIME_LOG,
		ALARM_LOG,
		ETC_LOG,
	} log_type;

	entry = &iter->curr_log->buf[iter->curr_idx];

	switch (entry->history_type) {
		case TIME_HISTORY_TYPE_TIME_SET ... TIME_HISTORY_TYPE_NETWORK_TIME_SET:
			log_type = TIME_LOG;
			break;
		case TIME_HISTORY_TYPE_ALARM_START ... TIME_HISTORY_TYPE_ALARM_DEL:
		case TIME_HISTORY_TYPE_RTC_ALARM_SET:
			log_type = ALARM_LOG;
			break;
		case TIME_HISTORY_TYPE_TIMEZONE_SET:
		case TIME_HISTORY_TYPE_NITZ_UPDATE_SET:
			log_type = ETC_LOG;
			break;
		default:
			return 0;
	};

	if (log_type == ALARM_LOG) {
		alarm = time_history_get_alarm_by_id(entry->alarmid);

		if ((owner_filter[0] != '-' && alarm
			&& strcasecmp(alarm->owner, owner_filter) != 0))
		return 0;
	}

	seq_print_timestamp(seq, entry);
	seq_printf(seq, "%4lld  %-13s  ", entry->history_idx,
			history_type_name[entry->history_type]);

	if ((entry->history_type == TIME_HISTORY_TYPE_ALARM_EXPIRED ||
			entry->history_type == TIME_HISTORY_TYPE_RTC_ALARM_SET) && alarm) {
		seq_printf(seq, "%-16s  ", alarm->owner);
	} else if (entry->history_type == TIME_HISTORY_TYPE_NITZ_UPDATE_SET) {
		if (entry->oldtime.tv_sec == -1)
			seq_printf(seq, "init: %ld", entry->newtime.tv_sec);
		else
			seq_printf(seq, "%ld > %ld", entry->oldtime.tv_sec, entry->newtime.tv_sec);
	} else {
		seq_printf(seq, "%-16s  ", entry->comm);
	}

	if (log_type == ETC_LOG) {
		seq_puts(seq, "\n");
		return 0;
	}

	if (entry->history_type == TIME_HISTORY_TYPE_ALARM_EXPIRED && alarm)
		seq_printf(seq, "%-22pf  ", alarm->handler);
	else if (entry->caller)
		seq_printf(seq, "%-22pf  ", entry->caller);
	else
		seq_printf(seq, "%24s", "");

	if (log_type == TIME_LOG || is_realtime(&entry->newtime)) {
		seq_print_realtime(seq, &entry->newtime, entry->tz_offset);
		seq_printf(seq, "   %10ld ", entry->newtime.tv_sec);
	} else {
		seq_printf(seq, "%15lu.%03lu",
				entry->newtime.tv_sec, entry->newtime.tv_nsec/NSEC_PER_MSEC);
	}

	if (log_type == TIME_LOG)
		seq_print_difftime(seq, entry->realtime, entry->newtime);

	if (entry->err)
		seq_printf(seq, " -> err: %d", entry->err);

	seq_puts(seq, "\n");
	return 0;
}

static struct time_history_iter *swap_log(struct time_history_iter *iter)
{
	struct time_history_log_buf *temp_log = iter->curr_log;
	int temp_idx = iter->curr_idx;

	if (unlikely(!iter))
		return NULL;

	/* Keep current log buf */
	if (!iter->comp_log)
		return (iter->curr_log)? iter : NULL;

	iter->curr_log = iter->comp_log;
	iter->comp_log = temp_log;

	iter->curr_idx = iter->comp_idx;
	iter->comp_idx = temp_idx;

	return iter;
}

static void *move_iter(struct time_history_iter *iter)
{
	if (!iter->curr_log)
		return NULL;

	if (iter->curr_idx == iter->curr_log->tail) {
		iter->curr_log = NULL;
		return swap_log(iter);
	} else if (iter->curr_idx + 1 >= iter->curr_log->buf_size) {
		iter->curr_idx = 0;
	} else {
		iter->curr_idx++;
	}

	if (iter->comp_log && (iter->curr_log->buf[iter->curr_idx].history_idx
			> iter->comp_log->buf[iter->comp_idx].history_idx)) {
		return swap_log(iter);
	}

	return iter;
}

static void *time_history_start(struct seq_file *seq, loff_t *pos)
{
	struct time_history_iter *iter = seq->private;

	if (*pos >= th_ctxt.history_cnt)
		return NULL;

	if (*pos)
		return move_iter(iter);

	time_history_show_header(seq);

	if (time_log.buf[time_log.head].history_idx
			< alarm_log.buf[alarm_log.head].history_idx) {
		iter->curr_log = &time_log;
		iter->curr_idx = time_log.head;
		iter->comp_log = &alarm_log;
		iter->comp_idx = alarm_log.head;
	} else {
		iter->curr_log = &alarm_log;
		iter->curr_idx = alarm_log.head;
		iter->comp_log = &time_log;
		iter->comp_idx = time_log.head;
	}

	return iter;
}

static void *time_history_next(struct seq_file *seq, void *v, loff_t *pos)
{
	struct time_history_iter *iter = seq->private;

	(*pos)++;

	if (*pos >= th_ctxt.history_cnt)
		return NULL;

	return move_iter(iter);
}

static void time_history_stop(struct seq_file *seq, void *v)
{
}

static const struct seq_operations time_history_seq_ops = {
	.start = time_history_start,
	.show  = time_history_show,
	.next  = time_history_next,
	.stop  = time_history_stop,
};

static int time_history_open(struct inode *inode, struct file *file)
{
	return seq_open_private(file, &time_history_seq_ops,
			sizeof(struct time_history_iter));
}

/* return true if valid timezone format */
static int time_history_valid_tz_format(char *timezone)
{
	int tz_hour, tz_min;
	int len;

	if (unlikely(!timezone))
		return false;

	len = strlen(timezone);
	if (len < 5 || len > 6)
		return false;

	if (timezone[0] != '+' && timezone[0] != '-')
		return false;

	if (sscanf(&timezone[1], "%d:%d", &tz_hour, &tz_min) < 0)
		return false;

	if (tz_hour > 14)
		return false;

	if (tz_min != 0 && tz_min != 30 && tz_min != 45)
		return false;

	return true;
}

static ssize_t time_history_write(struct file *file,
		const char __user *buf, size_t count, loff_t *pos)
{
	char option[32] = {0};
	char val[64] = {0};

	if (sscanf(buf, "%s %s", option, val) < 0)
		return -EINVAL;

	if (strcasecmp(option, "timezone") == 0) {
		if (!time_history_valid_tz_format(val)) {
			pr_err("%s: Invalid timezone format: %s\n", __func__, val);
			return -EINVAL;
		}
		time_history_insert_timezone_log(th_ctxt.timezone, val);
		memset(th_ctxt.timezone, 0, sizeof(th_ctxt.timezone));
		snprintf(th_ctxt.timezone, sizeof(th_ctxt.timezone) - 1, "%s", val);
		th_ctxt.tz_offset = time_history_timezone_offset(th_ctxt.timezone);
	} else if (strcasecmp(option, "timezone_id") == 0) {
		memset(th_ctxt.timezone_id, 0, sizeof(th_ctxt.timezone_id));
		snprintf(th_ctxt.timezone_id, sizeof(th_ctxt.timezone_id) - 1, "%s", val);
	} else if (strcasecmp(option, "host") == 0) {
		struct timespec newtime = {0, 0};
		sscanf(val, "%ld", &newtime.tv_sec);
		time_history_insert_time_log(TIME_HISTORY_TYPE_HOST_TIME_SET,
				&newtime, &newtime, NULL, 0);
	} else if (strcasecmp(option, "nitz") == 0) {
		struct timespec newtime = {0, 0};
		sscanf(val, "%ld", &newtime.tv_sec);
		time_history_insert_time_log(TIME_HISTORY_TYPE_NETWORK_TIME_SET,
				&newtime, &newtime, NULL, 0);
	} else if (strcasecmp(option, "nitz_update") == 0) {
		int nitz_update = -1;
		sscanf(val, "%d", &nitz_update);
		time_history_insert_setting_log(TIME_HISTORY_TYPE_NITZ_UPDATE_SET,
				th_ctxt.nitz_update, nitz_update);
		th_ctxt.nitz_update = nitz_update;
	} else if (strcasecmp(option, "filter") == 0) {
		char *cur;
		memset(owner_filter, 0, sizeof(owner_filter));
		snprintf(owner_filter, sizeof(owner_filter), "%s", val);
		memset(th_ctxt.timezone, 0, sizeof(th_ctxt.timezone));
		cur = strchr(owner_filter, '\n');
		if (cur)
			*cur = '\0';
	} else {
		pr_info("%s: Failed to parse option\n", __func__);
		return -EINVAL;
	}

	return count;
}

static const struct file_operations time_history_fops = {
	.open		= time_history_open,
	.read		= seq_read,
	.write		= time_history_write,
	.llseek		= seq_lseek,
	.release	= seq_release_private,
};

static int __init time_history_init(void)
{
	struct dentry *d;

	d = debugfs_create_file("time_history", 0664, NULL,
			NULL, &time_history_fops);
	if (!d) {
		pr_err("Failed to create time_history debugfs\n");
		return -EFAULT;
	}

	if (timeh_rtc_interface_setup())
		pr_err("Failed to setup rtc intf for timeh\n");

	spin_lock_init(&th_ctxt.lock);

	time_log.buf[0].history_idx  = ULLONG_MAX;
	alarm_log.buf[0].history_idx = ULLONG_MAX;

	timeh_sleep_monitor_cb_setup();

	return 0;
}
fs_initcall(time_history_init);
