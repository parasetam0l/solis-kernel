#include <linux/kernel.h>
#include <linux/percpu-defs.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/cpu.h>

#include <linux/sluggish_analyzer.h>

#define OUTPUT_SIZE	2048
#define K(x) ((x) << (PAGE_SHIFT - 10)) /* pages to kb */

static u32 period_in_msec = 5000; /* 5 seconds, may be tunable */
static unsigned long timeout;

/* warn(printed out) if iowait is above 10% of period(printed out).
 * (ie. 500 ms) Tunalbe.
 */
static u32 warning_ratio = 10;

static unsigned long old_vmstat[NR_VM_EVENT_ITEMS];
static unsigned long vmstat[NR_VM_EVENT_ITEMS];
static char output_buf[OUTPUT_SIZE];

#ifdef SLUGGISH_ANALYZER_THREAD
static struct task_struct *thread;
#endif /* SLUGGISH_ANALYZER_THREAD */

#define CPU_NUM	NR_CPUS
static u64 user[CPU_NUM], nice[CPU_NUM], system[CPU_NUM], idle[CPU_NUM], iowait[CPU_NUM], irq[CPU_NUM], softirq[CPU_NUM];
static u64 old_user[CPU_NUM];
static u64 old_nice[CPU_NUM];
static u64 old_system[CPU_NUM];
static u64 old_idle[CPU_NUM];
static u64 old_iowait[CPU_NUM];
static u64 old_irq[CPU_NUM];
static u64 old_softirq[CPU_NUM];
static u64 total_old_iowait;

static u64 get_idle_time(int cpu)
{
	u64 idle, idle_time = -1ULL;

	if (cpu_online(cpu))
		idle_time = get_cpu_idle_time_us(cpu, NULL);

	if (idle_time == -1ULL)
		/* !NO_HZ or cpu offline, reinit  */
		idle = 0;
	else
		idle = usecs_to_cputime64(idle_time);

	return idle;
}

static u64 get_iowait_time(int cpu)
{
	u64 ret, iowait_time = -1ULL;

	if (cpu_online(cpu))
		iowait_time = get_cpu_iowait_time_us(cpu, NULL);

	if (iowait_time == -1ULL)
		/* !NO_HZ or cpu offline so let's use the same one as old */
		ret = 0;
	else
		ret = usecs_to_cputime64(iowait_time);

	return ret;
}


#define SAVE_TO_OLD(val)	do { if (val[i]) (old_##val[i]) = (val[i]); } while(0)
#define DIFF_IN_MSEC(val)	((val[i]) && ((val[i]) > (old_##val[i]))) ? \
		(cputime_to_usecs(val[i] - old_##val[i]) / 1000) : 0
#define MSEC_TO_USEC(msec)	(msec * 1000)

static int check_io_wait_warning(void)
{
	int ret = 0, i;
	u64 total_iowait = 0;

	for_each_possible_cpu(i)
		total_iowait += iowait[i];

	if (period_in_msec && total_iowait > total_old_iowait &&
		/*
		 * When cpu1 is on, total iowait time suddenly be larger than
		 * previos one. At that time, that is not io busy situation. So
		 * ignore and do not print out logs. The difference of old and
		 * current io wait time should be smaller than period time.
		 */
			(cputime_to_usecs(total_iowait - total_old_iowait) <
			 MSEC_TO_USEC(period_in_msec)) &&
			((cputime_to_usecs(total_iowait - total_old_iowait) /
					10 / period_in_msec) > warning_ratio))
		ret = 1;
	else
		pr_debug("[SLUG] io wait(msec) cur: %u , old: %u\n",
				cputime_to_usecs(total_iowait) / 1000,
				cputime_to_usecs(total_old_iowait) / 1000);
	total_old_iowait = total_iowait; /* iowait of all cpus */

	return ret;
}

static void save_old_cpustat(void)
{
	int i;

	for_each_possible_cpu(i) {
		SAVE_TO_OLD(user);
		SAVE_TO_OLD(nice);
		SAVE_TO_OLD(system);
		SAVE_TO_OLD(idle);
		SAVE_TO_OLD(iowait);
		SAVE_TO_OLD(irq);
		SAVE_TO_OLD(softirq);
	}
}

static void get_cpustat(void)
{
	int i;

	for_each_possible_cpu(i) {
		user[i] = nice[i] = system[i] = idle[i] = iowait[i] = irq[i] = softirq[i] = 0;
		user[i] = kcpustat_cpu(i).cpustat[CPUTIME_USER];
		nice[i] = kcpustat_cpu(i).cpustat[CPUTIME_NICE];
		system[i] = kcpustat_cpu(i).cpustat[CPUTIME_SYSTEM];
		idle[i] = get_idle_time(i);
		iowait[i] = get_iowait_time(i);
		irq[i] = kcpustat_cpu(i).cpustat[CPUTIME_IRQ];
		softirq[i] = kcpustat_cpu(i).cpustat[CPUTIME_SOFTIRQ];
	}

}

static int print_cpustat(char *buf, ssize_t count, bool show_title)
{
	int i, ret = 0;

	if (show_title)
		ret += snprintf(buf + ret, count,
				"\t\tuser\tnice\tsystem\tidle\tiowait\tirq\tsoftirq\n");
	for_each_possible_cpu(i) {
		pr_debug("cpu%d: iowait now: %llu, old %llu\n", i, iowait[i],
				old_iowait[i]);

		ret += snprintf(buf + ret, count,
				"[SLUG] cpu%d:\t%u\t%u\t%u\t%u\t%u\t%u\t%u\n",
				i, DIFF_IN_MSEC(user),
				DIFF_IN_MSEC(nice),
				DIFF_IN_MSEC(system),
				DIFF_IN_MSEC(idle),
				DIFF_IN_MSEC(iowait),
				DIFF_IN_MSEC(irq),
				DIFF_IN_MSEC(softirq));

	}
	return ret;
}

#ifdef CONFIG_VM_EVENT_COUNTERS
enum writeback_stat_item {
	NR_DIRTY_THRESHOLD,
	NR_DIRTY_BG_THRESHOLD,
	NR_VM_WRITEBACK_STAT_ITEMS,
};

static void save_old_vmstat(void)
{
	int i;
	for (i = 0; i < NR_VM_EVENT_ITEMS; i++)
		old_vmstat[i] = vmstat[i];
}

static void get_vmstat(void)
{
	int cpu;
	int i;

	memset(vmstat, 0, NR_VM_EVENT_ITEMS * sizeof(unsigned long));

	for_each_online_cpu(cpu) {
		struct vm_event_state *this = &per_cpu(vm_event_states, cpu);

		for (i = 0; i < NR_VM_EVENT_ITEMS; i++)
			vmstat[i] += this->event[i];
	}

//	all_vm_events(vmstat);
	vmstat[PGPGIN] /= 2;		/* sectors -> kbytes */
	vmstat[PGPGOUT] /= 2;
}


#define DIFF_VMSTAT(idx)	(vmstat[idx] - old_vmstat[idx])
static int check_mem_warning(void)
{
	/* XXX :
	 * Check alloc_stall(direct reclaimed).
	 * Is it enough? or more something? i.e pageoutrun(kswapd), slowpath
	 * available memory?
	 */
	if (DIFF_VMSTAT(ALLOCSTALL) > 20) /* experimental value */
		return 1;
	return 0;
}


#define TEXT_OFFSET	(NR_VM_ZONE_STAT_ITEMS + NR_VM_WRITEBACK_STAT_ITEMS - 1)
#define PR_VMSTAT(idx)	sprintf(buf + ret, " %8lu", \
		DIFF_VMSTAT(idx))
#define PR_VMSTAT_TEXT(idx) sprintf(buf + ret, " %8s", vmstat_text[TEXT_OFFSET + idx])

/* print vmstat */
static int print_vmstat(char *buf, ssize_t count, bool show_title)
{
	unsigned long freeram = global_page_state(NR_FREE_PAGES);
	/* If using available memory, we have to consider buffer and it uses
	 * block device spinlock which is not simple and may make deadlock or
	 * fall to sleep state of current process. So just printing number
	 * of cached pages out are enough to get meaningful data for sluggish
	 * analyzing.
	 */
	unsigned long cached = global_page_state(NR_FILE_PAGES);
	int ret = 0;


	if (show_title) {
		ret += sprintf(buf, "    Free");
		ret += sprintf(buf + ret, "  Cached");
		ret += PR_VMSTAT_TEXT(PSWPIN);
		ret += PR_VMSTAT_TEXT(PSWPOUT);

		ret += PR_VMSTAT_TEXT(PGMAJFAULT);

		/* kswapd things */
		ret += PR_VMSTAT_TEXT(PGSCAN_KSWAPD_NORMAL);
#ifdef CONFIG_HIGHMEM
		ret += PR_VMSTAT_TEXT(PGSCAN_KSWAPD_HIGH);
#endif
		ret += PR_VMSTAT_TEXT(PGSCAN_KSWAPD_MOVABLE);
		ret += PR_VMSTAT_TEXT(KSWAPD_LOW_WMARK_HIT_QUICKLY);
		ret += PR_VMSTAT_TEXT(PAGEOUTRUN);

		/* direct reclaim things */
		ret += PR_VMSTAT_TEXT(PGSCAN_DIRECT_NORMAL);
#ifdef CONFIG_HIGHMEM
		ret += PR_VMSTAT_TEXT(PGSCAN_DIRECT_HIGH);
#endif
		ret += PR_VMSTAT_TEXT(PGSCAN_DIRECT_MOVABLE);
		ret += PR_VMSTAT_TEXT(ALLOCSTALL);
		ret += sprintf(buf + ret, "\n");
	}


	ret += sprintf(buf + ret, "[SLUG] %8lu", K(freeram));
	ret += sprintf(buf + ret, " %8lu", K(cached));

	/* swap in/out */
	ret += PR_VMSTAT(PSWPIN);
	ret += PR_VMSTAT(PSWPOUT);

	ret += PR_VMSTAT(PGMAJFAULT);

	/* kswapd things */
	ret += PR_VMSTAT(PGSCAN_KSWAPD_NORMAL);
#ifdef CONFIG_HIGHMEM
	ret += PR_VMSTAT(PGSCAN_KSWAPD_HIGH);
#endif
	ret += PR_VMSTAT(PGSCAN_KSWAPD_MOVABLE);
	ret += PR_VMSTAT(KSWAPD_LOW_WMARK_HIT_QUICKLY);
	ret += PR_VMSTAT(PAGEOUTRUN);

	/* direct reclaim things */
	ret += PR_VMSTAT(PGSCAN_DIRECT_NORMAL);
#ifdef CONFIG_HIGHMEM
	ret += PR_VMSTAT(PGSCAN_DIRECT_HIGH);
#endif
	ret += PR_VMSTAT(PGSCAN_DIRECT_MOVABLE);
	ret += PR_VMSTAT(ALLOCSTALL);

	ret += sprintf(buf + ret, "\n");
	return ret;
}
#else /* CONFIG_VM_EVENT_COUNTERS */
static void save_old_vmstat(int warning)
{
	return;
}

static void get_vmstat(void)
{
	return;
}

static int print_vmstat(char *buf, ssize_t count, bool show_title)
{
	return 0;
}
#endif /* CONFIG_VM_EVENT_COUNTERS */



#ifdef SLUGGISH_ANALYZER_THREAD
static int sluggish_analyzer_thread(void *d)
{
	int warning;

	do {
		msleep(period_in_msec);

		if (period_in_msec == 0 || kthread_should_stop())
			break; /* disable sluggish monitor */

		get_cpustat();
		warning = 0;
		/* consider warning_ratio */
		warning += check_io_wait_warning();

		get_vmstat();
		warning += check_mem_warning();
		/* TODO: check touch pressed and if that, then skip print out or
		 * defer to print it
		 */
		output_buf[0] = '\0';
		print_cpustat(output_buf, sizeof(output_buf), false);

		if (warning) {
			pr_warn("[SLUG] %s", output_buf);
			output_buf[0] = '\0';
			print_vmstat(output_buf, sizeof(output_buf), false);
			pr_warn("[SLUG] %s", output_buf);
			pr_warn("[SLUG] warning = %d", warning);
		}
		save_old_cpustat();
		save_old_vmstat();

	} while (1);

	thread = NULL;

	return 0;
}
#endif /* SLUGGISH_ANALYZER_THREAD */

static void check_warning(void)
{
	unsigned long now;

	now = jiffies;
	if (period_in_msec && time_after(now, timeout)) {
		/* period is expired */
		int warning = 0, ret;

		if (check_io_wait_warning()) {
			warning++;
			pr_debug("[SLUG] io wait usec old = %u",
					cputime_to_usecs(total_old_iowait)
					/ 1000);
		}

		if (check_mem_warning()) {
			warning++;
			pr_debug("[SLUG] allocstall(old:now) = %lu : %lu",
					old_vmstat[ALLOCSTALL],
					vmstat[ALLOCSTALL]);
		}

		if (warning) {
			/* previous stat data is printed out */
			pr_warn("%s", output_buf);
			output_buf[0] = '\0';

			/* current stat data is printed out */
			print_cpustat(output_buf, sizeof(output_buf), false);
			pr_warn("%s", output_buf);
			output_buf[0] = '\0';

			print_vmstat(output_buf, sizeof(output_buf), false);
			pr_warn("%s", output_buf);
		} else {
			/* One previous stat data is stored but it is not printed out.
			 * If warning on next period, it will be printed */
			output_buf[0] = '\0';
			ret = print_cpustat(output_buf, sizeof(output_buf), false);
			ret += print_vmstat(output_buf + ret, sizeof(output_buf) - ret, false);
		}

		save_old_cpustat();
		save_old_vmstat();
		timeout = now + msecs_to_jiffies(period_in_msec);
	}
}


void store_sluggish_load_factor(struct sluggish_load_factor_tag *factor)
{
	int i;

	get_cpustat();
	get_vmstat();

	for_each_possible_cpu(i) {
		factor->iowait[i] = iowait[i];
		factor->irq[i] = irq[i];
	}

	factor->freeram = global_page_state(NR_FREE_PAGES);

	for(i = 0; i < NR_VM_EVENT_ITEMS; i++)
		factor->vmstat[i] = vmstat[i];

	check_warning();
}


static void sluggish_analyzer_exit(void)
{
	return;
}

static ssize_t debugfs_stat_read(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)

{
	unsigned int ret = 0;

	if (*ppos > 0)
		return 0;
	else if (*ppos < 0)
		return -EINVAL;

	get_vmstat();
	output_buf[0] = '\0';
	ret += print_cpustat(output_buf, sizeof(output_buf), true);
	ret += print_vmstat(output_buf + ret, sizeof(output_buf) - ret, true);
	if (copy_to_user(buffer, output_buf, ret))
		return -EFAULT;
	*ppos += ret;
	count += ret;
	return ret;
}

static const struct file_operations fops_stat = {
	.owner = THIS_MODULE,
	.read = debugfs_stat_read,
};

static int debugfs_period_set(void *data, u64 val)
{
	*(u32 *)data = val;
	pr_info("%s:%d val= %llu\n", __func__, __LINE__, val);

#ifdef SLUGGISH_ANALYZER_THREAD
	if (val > 0 && thread == NULL) {
		pr_info("%s:%d\n", __func__, __LINE__);
		thread = kthread_run(sluggish_analyzer_thread, NULL,
				"sluggish_analyzer");
		if (IS_ERR(thread))
			return PTR_ERR(thread);
	}
#endif /* SLUGGISH_ANALYZER_THREAD */

	return 0;
}
static int debugfs_u32_get(void *data, u64 *val)
{
	*val = *(u32 *)data;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(fops_period, debugfs_u32_get, debugfs_period_set, "%llu\n");


static int sluggish_analyzer_init(void)
{
	struct dentry *dir;

#ifdef SLUGGISH_ANALYZER_THREAD
	thread = kthread_run(sluggish_analyzer_thread, NULL, "sluggish_analyzer");
	if (IS_ERR(thread))
		return PTR_ERR(thread);
#endif /* SLUGGISH_ANALYZER_THREAD */

	dir = debugfs_create_dir("sluggish_analyzer", NULL);

	debugfs_create_file("stat", 0400, dir, NULL,
			&fops_stat);
	debugfs_create_file("period_in_msec", 0644, dir, &period_in_msec,
			&fops_period);
	debugfs_create_u32("warning_ratio", 0644, dir, &warning_ratio);

	return 0;
}

module_init(sluggish_analyzer_init);
module_exit(sluggish_analyzer_exit);

