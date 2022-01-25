
#include <linux/jiffies.h>

static unsigned long current_boot_completed;

#if defined(CONFIG_SLP_BUSY_LEVEL)
int cpu_busy_level;

int la_get_cpu_busy_level(void)
{
	return cpu_busy_level;
}

void la_set_cpu_busy_level(int set_cpu_busy_level)
{
	cpu_busy_level = set_cpu_busy_level;

	return;
}

#define NOT_BREAK	0xFFFF
int check_load_level(unsigned int current_cnt)
{
	#define DEFAULT_CHECK_COUNT	100
	#define CPU_NOT_BUSY_COUNT	4

	unsigned int check_count = DEFAULT_CHECK_COUNT;
	unsigned int cpu_not_busy_count = 0;
	static unsigned int cpu_not_busy_count_before;
	static unsigned int shake_busy = 0;

	int i, cnt, cpu_load_result;
	static int busy_level = 0, last_break_cnt = 0;

	if (time_before(jiffies, current_boot_completed))
		return 0;

	cnt = current_cnt;

	if (last_break_cnt < check_count) {
		last_break_cnt++;

		busy_level = 0;
		return busy_level;
	}

	for (i = 0; i < check_count; i++) {
		cpu_load_result = cpu_load_checking(cnt);
		if (cpu_load_result == NOT_BUSY_LOAD)
			cpu_not_busy_count++;

		if(cpu_not_busy_count >=CPU_NOT_BUSY_COUNT) {
			break;
		}

		cnt = get_index(cnt, cpu_load_history_num, -1);
	}

	if (i == check_count) {
		busy_level = 9;
		last_break_cnt = NOT_BREAK;  /* very high value*/
	}else {
		busy_level = 0;
		shake_busy = 0;
		last_break_cnt = i;
	}

	cpu_not_busy_count_before = cpu_not_busy_count;

	return	busy_level;
}
#endif

#define CM_SAVE_DATA_NUM	100

int save_end_index;
int save_start_index = 1;

void update_current_log_req(void);

void current_monitor_manager(int cnt)
{
	static int index_to_save_trigger = -1;

	if (index_to_save_trigger == -1) {
		index_to_save_trigger = get_index(save_end_index
				, cpu_load_history_num, CM_SAVE_DATA_NUM);
	}

	if (index_to_save_trigger == cnt ) {
		index_to_save_trigger = -1;
		save_start_index = save_end_index +1;
		save_end_index = cnt;
		update_current_log_req();
	}
}

static ssize_t current_monitor_read(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	ssize_t ret = 0;
	char *buf;
	static int cnt = 0, show_line_num = 0,  remained_line = 0;

	if (*ppos < 0 || !count)
		return -EINVAL;

	buf = vmalloc(count);
	if (!buf)
		return -ENOMEM;

	if (*ppos == 0) {
		remained_line = CM_SAVE_DATA_NUM;
		cnt = save_start_index - 1;
	}

	if (remained_line >= CPU_BUS_SHOW_LINE_NUM) {
		show_line_num = CPU_BUS_SHOW_LINE_NUM;
		remained_line -= CPU_BUS_SHOW_LINE_NUM;

	} else {
		show_line_num = remained_line;
		remained_line = 0;
	}
	cnt = get_index(cnt, cpu_load_history_num, show_line_num);

	ret = show_current_monitor_read_sub(cnt, show_line_num, buf, count, ret);

	if (ret >= 0) {
		if (copy_to_user(buffer, buf, ret)) {
			vfree(buf);
			return -EFAULT;
		}
		*ppos += ret;
	}

	vfree(buf);

	return ret;
}

static ssize_t current_monitor_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int show_num = 0;

	show_num = atoi(user_buf);

	if (show_num <= cpu_load_history_num)
		cpu_bus_load_freq_history_show_cnt = show_num;
	else
		return -EINVAL;

	return count;
}

int current_monitor_en = 1;

static int current_monitor_en_read_sub(char *buf, int buf_size)
{
	int ret = 0;

	ret += snprintf(buf + ret, buf_size - ret, "%d\n", current_monitor_en);

	return ret;
}

static ssize_t current_monitor_en_read(struct file *file,
		char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int size_for_copy;

	size_for_copy = wrapper_for_debug_fs(buffer, count, ppos
						,current_monitor_en_read_sub);

	return size_for_copy;
}

static ssize_t current_monitor_en_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	current_monitor_en = atoi(user_buf);

	return count;
}

static const struct file_operations current_monitor_fops = {
	.owner = THIS_MODULE,
	.read  = current_monitor_read,
	.write = current_monitor_write,
};

static const struct file_operations current_monitor_en_fops = {
	.owner = THIS_MODULE,
	.read  = current_monitor_en_read,
	.write = current_monitor_en_write,
};

void debugfs_current(struct dentry *d)
{
	if (!debugfs_create_file("current_monitor", 0600, d, NULL, &current_monitor_fops))
		pr_err("%s : debugfs_create_file, error\n", "current_monitor");

	if (!debugfs_create_file("current_monitor_en", 0600, d, NULL, &current_monitor_en_fops))
		pr_err("%s : debugfs_create_file, error\n", "current_monitor_en");
}

void load_analyzer_current_init(void)
{
	#define EXPECTED_BOOT_TIME	(60 * HZ)
	current_boot_completed = jiffies + EXPECTED_BOOT_TIME;
}
