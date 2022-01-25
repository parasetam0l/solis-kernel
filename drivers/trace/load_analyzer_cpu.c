unsigned int cpu_load_history_num = CPU_LOAD_HISTORY_NUM_MIN;
unsigned int cpu_task_history_num = CPU_TASK_HISTORY_NUM_MIN;

static int __init cpu_history_num_setup(char *str)
{
#if defined(CONFIG_SLP_KERNEL_ENG) || defined (CONFIG_TIZEN_SEC_KERNEL_ENG)
	cpu_load_history_num = CPU_LOAD_HISTORY_NUM_MAX;
	cpu_task_history_num = CPU_TASK_HISTORY_NUM_MAX;
#else
	{
		int debug_enable = 0;

		get_option(&str, &debug_enable);
		if (debug_enable) {
			cpu_load_history_num = CPU_LOAD_HISTORY_NUM_MAX;
			cpu_task_history_num = CPU_TASK_HISTORY_NUM_MAX;
		}
	}
#endif

	pr_info("%s: history_num: %d/%d\n", __func__,
			cpu_load_history_num, cpu_task_history_num);

	return 0;
}
early_param("sec_debug.enable", cpu_history_num_setup);

static unsigned int cpu_load_freq_history_cnt;
static unsigned int cpu_load_freq_history_view_cnt;

static int cpu_load_freq_history_show_cnt = 100;

#if defined(CONFIG_SLP_CHECK_BUS_LOAD)
#define CPU_BUS_VIEW_DEFAULT_VALUE	100
static int cpu_bus_load_freq_history_show_cnt = CPU_BUS_VIEW_DEFAULT_VALUE;
#endif
#if defined(CONFIG_SLP_BUS_CLK_CHECK_LOAD)
static int cpu_bus_clk_load_freq_history_show_cnt = 100;
#endif

static struct cpu_load_freq_history_tag *cpu_load_freq_history;
static struct cpu_load_freq_history_tag *cpu_load_freq_history_view;
#define CPU_LOAD_FREQ_HISTORY_SIZE	\
	(sizeof(struct cpu_load_freq_history_tag) * cpu_load_history_num)

struct cpu_process_runtime_tag temp_process_runtime;

bool cpu_task_history_onoff;

struct list_head process_headlist;

static unsigned int cpu_task_history_cnt[CPU_NUM];
static unsigned int cpu_task_history_show_start_cnt;
static unsigned int cpu_task_history_show_end_cnt;
static  int  cpu_task_history_show_select_cpu;

static unsigned long long  total_time, section_start_time, section_end_time;

struct saved_task_info_tag {
	unsigned int pid;
	char comm[TASK_COMM_LEN];
};
#define SAVED_TASK_INFO_NUM   500
#define SAVED_TASK_INFO_SIZE  (sizeof(struct saved_task_info_tag) * SAVED_TASK_INFO_NUM)
static struct saved_task_info_tag *saved_task_info;
static unsigned int  saved_task_info_cnt;

bool saved_task_info_onoff;

static int store_killed_init(void)
{
	saved_task_info = vmalloc(SAVED_TASK_INFO_SIZE);

	if (saved_task_info == NULL)
		return -ENOMEM;

	return 0;
}

static void store_killed_exit(void)
{
	if (saved_task_info != NULL)
		vfree(saved_task_info);
}

static void store_killed_memset(void)
{
	memset(saved_task_info, 0, SAVED_TASK_INFO_SIZE);
}

void store_killed_task(struct task_struct *tsk)
{
	unsigned int cnt;

	if( saved_task_info_onoff == 0)
		return;

	if (++saved_task_info_cnt >= SAVED_TASK_INFO_NUM)
		saved_task_info_cnt = 0;

	cnt = saved_task_info_cnt;

	saved_task_info[cnt].pid = tsk->pid;
	strncpy(saved_task_info[cnt].comm, tsk->comm, TASK_COMM_LEN);
}

int search_killed_task(unsigned int pid, char *task_name)
{
	unsigned int cnt = saved_task_info_cnt, i;

	for (i = 0; i< SAVED_TASK_INFO_NUM; i++) {
		if (saved_task_info[cnt].pid == pid) {
			strncpy(task_name, saved_task_info[cnt].comm, TASK_COMM_LEN);
			break;
		}
		cnt = get_index(cnt, SAVED_TASK_INFO_NUM, -1);
	}

	if (i == SAVED_TASK_INFO_NUM)
		return -1;

	return 0;
}

static unsigned long long calc_delta_time(unsigned int cpu, unsigned int index)
{
	unsigned long long run_start_time, run_end_time;

	if (index == 0) {
		run_start_time = cpu_task_history_view[cpu_task_history_num-1][cpu].time;
	} else
		run_start_time = cpu_task_history_view[index-1][cpu].time;

	if (run_start_time < section_start_time)
		run_start_time = section_start_time;

	run_end_time = cpu_task_history_view[index][cpu].time;

	if (run_end_time < section_start_time)
		return 0;

	if (run_end_time > section_end_time)
		run_end_time = section_end_time;

	return  run_end_time - run_start_time;
}

static void add_process_to_list(unsigned int cpu, unsigned int index)
{
	struct cpu_process_runtime_tag *new_process;

	new_process = kmalloc(sizeof(struct cpu_process_runtime_tag), GFP_KERNEL);
	new_process->runtime = calc_delta_time(cpu, index);
	new_process->cnt  = 1;
	new_process->task = cpu_task_history_view[index][cpu].task;
	new_process->pid  = cpu_task_history_view[index][cpu].pid;

	if (new_process->runtime != 0) {
		INIT_LIST_HEAD(&new_process->list);
		list_add_tail(&new_process->list, &process_headlist);
	} else
		kfree(new_process);

	return;
}

static void del_process_list(void)
{
	struct cpu_process_runtime_tag *curr;
	struct list_head *p, *n;

	list_for_each_prev_safe(p, n, &process_headlist) {
		curr = list_entry(p, struct cpu_process_runtime_tag, list);
		kfree(curr);
	}

	process_headlist.prev = NULL;
	process_headlist.next = NULL;
}

static int comp_list_runtime(struct list_head *list1, struct list_head *list2)
{
	struct cpu_process_runtime_tag *list1_struct, *list2_struct;

	int ret = 0;

	list1_struct = list_entry(list1, struct cpu_process_runtime_tag, list);
	list2_struct = list_entry(list2, struct cpu_process_runtime_tag, list);

	if (list1_struct->runtime > list2_struct->runtime)
		ret = 1;
	else if (list1_struct->runtime < list2_struct->runtime)
		ret = -1;
	else
		ret = 0;

	return ret;
}

static void swap_process_list(struct list_head *list1, struct list_head *list2)
{
	struct list_head *list1_prev, *list1_next , *list2_prev, *list2_next;

	list1_prev = list1->prev;
	list1_next = list1->next;

	list2_prev = list2->prev;
	list2_next = list2->next;

	list1->prev = list2;
	list1->next = list2_next;

	list2->prev = list1_prev;
	list2->next = list1;

	list1_prev->next = list2;
	list2_next->prev = list1;
}

static unsigned int view_list(int max_list_num, char *buf, unsigned int buf_size, unsigned int ret)
{
	struct list_head *p;
	struct cpu_process_runtime_tag *curr;
	char task_name[80] = {0,};
	char *p_name = NULL;

	unsigned int cnt = 0, list_num = 0;
	unsigned long long t, total_time_for_clc;

	bool found_in_killed_process_list = 0;

	list_for_each(p, &process_headlist) {
		curr = list_entry(p, struct cpu_process_runtime_tag, list);
		list_num++;
	}

	for (cnt = 0; cnt < list_num; cnt++) {
		list_for_each(p, &process_headlist) {
		curr = list_entry(p, struct cpu_process_runtime_tag, list);
			if (p->next != &process_headlist) {
				if (comp_list_runtime(p, p->next) == -1)
					swap_process_list(p, p->next);
			}
		}
	}

	total_time_for_clc = total_time;
	do_div(total_time_for_clc, 1000);

	cnt = 1;

	list_for_each(p, &process_headlist) {
		curr = list_entry(p, struct cpu_process_runtime_tag, list);
		t = curr->runtime * 100;
		do_div(t, total_time_for_clc);
		curr->usage = t + 5;

		if ((curr != NULL) && (curr->task != NULL)
			&& (curr->task->pid == curr->pid)) {
			p_name = curr->task->comm;

		} else {
			if(search_killed_task(curr->pid, task_name) >= 0) {
				found_in_killed_process_list = 1;
			} else {
				snprintf(task_name, sizeof(task_name), "NOT found task");
			}
			p_name = task_name;
		}

		if (ret < buf_size - 1) {
			ret +=  snprintf(buf + ret, buf_size - ret,
				"[%3d] %16s(%5d/%s) %6d[sched] %13lld[ns] %3d.%02d[%%]\n",
				cnt++, p_name, curr->pid,
				((found_in_killed_process_list) == 1) ? "X" : "O",
				curr->cnt, curr->runtime, curr->usage/1000, (curr->usage%1000) /10);
		} else {
			break;
		}

		found_in_killed_process_list = 0;

		if (cnt > max_list_num)
			break;
	}

	return ret;
}

static unsigned int view_list_raw(char *buf,unsigned int buf_size, unsigned int ret)
{
	struct list_head *p;
	struct cpu_process_runtime_tag *curr;
	char task_name[80] = {0,};
	char *p_name = NULL;

	unsigned int cnt = 0, list_num = 0;
	unsigned long long t, total_time_for_clc;

	list_for_each(p, &process_headlist) {
		curr = list_entry(p, struct cpu_process_runtime_tag, list);
		list_num++;
	}

	for (cnt = 0; cnt < list_num; cnt++) {
		list_for_each(p, &process_headlist) {
		curr = list_entry(p, struct cpu_process_runtime_tag, list);
			if (p->next != &process_headlist) {
				if (comp_list_runtime(p, p->next) == -1)
					swap_process_list(p, p->next);
			}
		}
	}

	total_time_for_clc = total_time;
	do_div(total_time_for_clc, 1000);

	cnt = 1;

	list_for_each(p, &process_headlist) {
		curr = list_entry(p, struct cpu_process_runtime_tag, list);
		t = curr->runtime * 100;
		do_div(t, total_time_for_clc);
		curr->usage = t + 5;

		if ((curr != NULL) && (curr->task != NULL)
			&& (curr->task->pid == curr->pid)) {
			p_name = curr->task->comm;

		} else {
			snprintf(task_name, sizeof(task_name), "NOT_FOUND");
			p_name = task_name;
		}

		if (ret < buf_size - 1) {
			ret +=  snprintf(buf + ret, buf_size - ret,
					"%d %s %d %lld %d\n",
					curr->pid, p_name, curr->cnt, curr->runtime, curr->usage);
		} else {
			break;
		}
	}

	return ret;
}

static struct cpu_process_runtime_tag *search_exist_pid(unsigned int pid)
{
	struct list_head *p;
	struct cpu_process_runtime_tag *curr;

	list_for_each(p, &process_headlist) {
		curr = list_entry(p, struct cpu_process_runtime_tag, list);
		if (curr->pid == pid)
			return curr;
	}
	return NULL;
}

static void clc_process_run_time(unsigned int cpu,
		unsigned int start_cnt, unsigned int end_cnt)
{
	unsigned int cnt = 0, start_array_num;
	unsigned int end_array_num, end_array_num_plus1;
	unsigned int i, loop_cnt;
	struct cpu_process_runtime_tag *process_runtime_data;
	unsigned long long t1, t2;

	start_array_num =
		(cpu_load_freq_history_view[start_cnt].task_history_cnt[cpu] + 1) % cpu_task_history_num;

	section_start_time = cpu_load_freq_history_view[start_cnt].time_stamp;
	section_end_time = cpu_load_freq_history_view[end_cnt].time_stamp;

	end_array_num = cpu_load_freq_history_view[end_cnt].task_history_cnt[cpu];
	end_array_num_plus1 =
		(cpu_load_freq_history_view[end_cnt].task_history_cnt[cpu] + 1) % cpu_task_history_num;

	t1 = cpu_task_history_view[end_array_num][cpu].time;
	t2 = cpu_task_history_view[end_array_num_plus1][cpu].time;

	if (t2 < t1)
		end_array_num_plus1 = end_array_num;

	total_time = section_end_time - section_start_time;

	if (process_headlist.next != NULL)
		del_process_list();

	INIT_LIST_HEAD(&process_headlist);

	if (end_array_num_plus1 >= start_array_num)
		loop_cnt = end_array_num_plus1-start_array_num + 1;
	else
		loop_cnt = end_array_num_plus1
				+ cpu_task_history_num - start_array_num + 1;

	for (i = start_array_num, cnt = 0; cnt < loop_cnt; cnt++, i++) {
		if (i >= cpu_task_history_num)
			i = 0;
		process_runtime_data
			= search_exist_pid(cpu_task_history_view[i][cpu].pid);
		if (process_runtime_data == NULL)
			add_process_to_list(cpu, i);
		else {
			process_runtime_data->runtime
				+= calc_delta_time(cpu, i);
			process_runtime_data->cnt++;
		}
	}
}

static unsigned int  process_sched_time_view(unsigned int cpu,
			unsigned int start_cnt, unsigned int end_cnt, char *buf,
			unsigned int buf_size, unsigned int ret)
{
	unsigned int i = 0, start_array_num, data_line, cnt = 0;
	unsigned int end_array_num, start_array_num_for_time;

	start_array_num_for_time = cpu_load_freq_history_view[start_cnt].task_history_cnt[cpu];
	start_array_num =
		(cpu_load_freq_history_view[start_cnt].task_history_cnt[cpu]+1) % cpu_task_history_num;
	end_array_num = cpu_load_freq_history_view[end_cnt].task_history_cnt[cpu];

	total_time = section_end_time - section_start_time;

	if (end_cnt == start_cnt+1) {
		ret +=  snprintf(buf + ret, buf_size - ret,
			"[%d] TOTAL SECTION TIME = %lld[ns]\n[%5d]~[%5d]/(%lld~%lld)\n\n",
			end_cnt, total_time,
			(cpu_load_freq_history_view[start_cnt].task_history_cnt[cpu] + 1) % cpu_task_history_num,
			(cpu_load_freq_history_view[end_cnt].task_history_cnt[cpu] + 1) % cpu_task_history_num,
			cpu_load_freq_history_view[start_cnt].time_stamp,
			cpu_load_freq_history_view[end_cnt].time_stamp);
	} else {
		ret +=  snprintf(buf + ret, buf_size - ret,
			"[%d~%d] TOTAL SECTION TIME = %lld[ns]\n[%5d]~[%5d]/(%lld~%lld)\n\n",
			start_cnt + 1, end_cnt, total_time,
			(cpu_load_freq_history_view[start_cnt].task_history_cnt[cpu] + 1) % cpu_task_history_num,
			(cpu_load_freq_history_view[end_cnt].task_history_cnt[cpu] + 1) % cpu_task_history_num,
			cpu_load_freq_history_view[start_cnt].time_stamp,
			cpu_load_freq_history_view[end_cnt].time_stamp);
	}

	end_array_num = get_index(end_array_num, cpu_task_history_num, 2);

	if (end_array_num >= start_array_num_for_time)
		data_line = end_array_num -start_array_num_for_time + 1;
	else
		data_line = (end_array_num + cpu_task_history_num) - start_array_num_for_time + 1;

	cnt = start_array_num_for_time;

	for (i = 0; i < data_line; i++) {
		u64 delta_time ,cnt_m1;
		char *p_name;
		char task_name[80] = {0,};
		unsigned int pid = 0;

		if (cnt > cpu_task_history_num-1)
			cnt = 0;

		cnt_m1 = get_index(cnt, cpu_task_history_num, -1);
		delta_time = cpu_task_history_view[cnt][cpu].time - cpu_task_history_view[cnt_m1][cpu].time;

		pid = cpu_task_history_view[cnt][cpu].pid;
		if (cpu_task_history_view[cnt][cpu].task->pid == pid) {
			p_name = cpu_task_history_view[cnt][cpu].task->comm;

		} else {
			if(search_killed_task(pid, task_name) < 0)
				snprintf(task_name, sizeof(task_name), "NOT found task");

			p_name = task_name;
		}

		if (ret >= buf_size)
			break;

		ret +=  snprintf(buf + ret, buf_size - ret,
				"[%5d] %lld %16s %5d %10lld[ns]\n", cnt, cpu_task_history_view[cnt][cpu].time,
				p_name, cpu_task_history_view[cnt][cpu].pid, delta_time);

		cnt++;
	}

	return ret;
}

void __slp_store_task_history(unsigned int cpu, struct task_struct *task)
{
	unsigned int cnt;

	if (cpu_task_history_onoff == 0)
		return ;

	if (++cpu_task_history_cnt[cpu] >= cpu_task_history_num)
		cpu_task_history_cnt[cpu] = 0;
	cnt = cpu_task_history_cnt[cpu];

	cpu_task_history[cnt][cpu].time = cpu_clock(UINT_MAX);
	cpu_task_history[cnt][cpu].task = task;
	cpu_task_history[cnt][cpu].pid  = task->pid;
}

void cpu_load_touch_event(unsigned int event)
{
	unsigned int cnt = 0;

	if (cpu_task_history_onoff == 0)
		return;

	cnt = cpu_load_freq_history_cnt;

	if (event == 0)
		cpu_load_freq_history[cnt].touch_event = 100;
	else if (event == 1)
		cpu_load_freq_history[cnt].touch_event = 1;
}

extern int suspending_flag;

static void set_cpu_load_freq_history_array_range(const char *buf)
{
	int show_array_num = 0, select_cpu;
	char cpy_buf[80] = {0,};
	char *p1, *p2, *p_lf;

	p1 = strstr(buf, "-");
	p2 = strstr(buf, "c");
	p_lf = strstr(buf, "\n");

	if (p2 == NULL)
		p2 = strstr(buf, "C");

	if ( (p2 != NULL) && ((p_lf - p2)  > 0)) {
		select_cpu = atoi(p2+1);
		if (select_cpu >= 0 && select_cpu < 4)
			cpu_task_history_show_select_cpu = select_cpu;
		else
			cpu_task_history_show_select_cpu = 0;
	} else
		cpu_task_history_show_select_cpu = -1;

	if (p1 != NULL) {
		strncpy(cpy_buf, buf, sizeof(cpy_buf) - 1);
		*p1 = '\0';
		cpu_task_history_show_start_cnt
			= get_index(atoi(cpy_buf) ,cpu_load_history_num ,-1);
		cpu_task_history_show_end_cnt = atoi(p1+1);

	} else {
		show_array_num = atoi(buf);
		cpu_task_history_show_start_cnt
			= get_index(show_array_num, cpu_load_history_num, -1);
		cpu_task_history_show_end_cnt = show_array_num;
	}

	pr_info("start_cnt=%d end_cnt=%d\n", cpu_task_history_show_start_cnt, cpu_task_history_show_end_cnt);
}

enum {
	WRONG_CPU_NUM = -1,
	WRONG_TIME_STAMP = -2,
	OVERFLOW_ERROR = -3,
};

int check_valid_range(unsigned int cpu, unsigned int start_cnt,
		unsigned int end_cnt)
{
	int ret = 0;
	unsigned long long t1, t2;
	unsigned int end_sched_cnt = 0, end_sched_cnt_margin;
	unsigned int load_cnt, last_load_cnt, overflow = 0;
	unsigned int cnt, search_cnt;
	unsigned int upset = 0;

	unsigned int i;

	if (cpu >= CPU_NUM)
		return WRONG_CPU_NUM;

	t1 = cpu_load_freq_history_view[start_cnt].time_stamp;
	t2 = cpu_load_freq_history_view[end_cnt].time_stamp;

	if ((t2 <= t1) || (t1 == 0) || (t2 == 0)) {
		pr_info("[time error] t1=%lld t2=%lld\n", t1, t2);
		return WRONG_TIME_STAMP;
	}

	last_load_cnt = cpu_load_freq_history_view_cnt;

	cnt = cpu_load_freq_history_view[last_load_cnt].task_history_cnt[cpu];
	t1 = cpu_task_history_view[cnt][cpu].time;
	search_cnt = cnt;

	for (i = 0;  i < cpu_task_history_num; i++) {
		search_cnt = get_index(search_cnt, cpu_task_history_num, 1);
		t2 = cpu_task_history_view[search_cnt][cpu].time;

		if (t2 < t1) {
			end_sched_cnt = search_cnt;
			break;
		}

		if (i >= cpu_task_history_num - 1)
			end_sched_cnt = cnt;
	}

	load_cnt = last_load_cnt;

	for (i = 0;  i < cpu_load_history_num; i++) {
		unsigned int sched_cnt, sched_before_cnt;
		unsigned int sched_before_cnt_margin;

		sched_cnt = cpu_load_freq_history_view[load_cnt].task_history_cnt[cpu];
		load_cnt = get_index(load_cnt, cpu_load_history_num, -1);

		sched_before_cnt = cpu_load_freq_history_view[load_cnt].task_history_cnt[cpu];

		if (sched_before_cnt > sched_cnt)
			upset++;

		end_sched_cnt_margin = get_index(end_sched_cnt, cpu_task_history_num, 1);
		sched_before_cnt_margin = get_index(sched_before_cnt, cpu_task_history_num, -1);

		/* "end_sched_cnt -1" is needed
		  *  because of calulating schedule time */
		if ((upset >= 2) || ((upset == 1)
			&& (sched_before_cnt_margin < end_sched_cnt_margin))) {
			overflow = 1;
			pr_err("[LA] overflow cpu=%d upset=%d sched_before_cnt_margin=%d" \
					"end_sched_cnt_margin=%d end_sched_cnt=%d" \
					"sched_before_cnt=%d sched_cnt=%d load_cnt=%d",
					cpu , upset, sched_before_cnt_margin,
					end_sched_cnt_margin, end_sched_cnt,
					sched_before_cnt, sched_cnt, load_cnt);
			break;
		}

		if (load_cnt == start_cnt)
			break;
	}

	if (overflow == 0) {
		ret = 0;
	} else {
		ret = OVERFLOW_ERROR;
		pr_info("[overflow error]\n");
	}

	return ret;
}

int check_running_buf_print(int max_list_num, char *buf, int buf_size, int ret, int *ret_check_valid);

void cpu_print_buf_to_klog(char *buffer)
{
	#define MAX_PRINT_LINE_NUM	1000
	char *p = NULL;
	int cnt = 0;

	do {
		p = strsep(&buffer, "\n");
		if (p)
			pr_info("%s\n", p);
	} while ((p != NULL) && (cnt++ < MAX_PRINT_LINE_NUM));
}

void cpu_last_load_freq(unsigned int range, int max_list_num)
{
	#define BUF_SIZE (1024 * 1024)

	int ret = 0, cnt = 0;
	int start_cnt = 0, end_cnt = 0;
	char *buf;
	char range_buf[64] = {0,};
	int ret_check_valid = 0;

	buf = vmalloc(BUF_SIZE);

	cpu_load_freq_history_view_cnt = cpu_load_freq_history_cnt;

	memcpy(cpu_load_freq_history_view, cpu_load_freq_history, CPU_LOAD_FREQ_HISTORY_SIZE);
	memcpy(cpu_task_history_view, cpu_task_history, CPU_TASK_HISTORY_SIZE);
#if defined (CONFIG_CHECK_WORK_HISTORY)
	memcpy(cpu_work_history_view, cpu_work_history, CPU_WORK_HISTORY_SIZE);
#endif

	ret +=  snprintf(buf + ret, BUF_SIZE - ret,
			"===============================================================================" \
			"========================================\n");

	ret +=  snprintf(buf + ret, BUF_SIZE - ret,
			"    TIME       CPU0_F  CPU1_F  CPU_LOCK    [INDEX]\tCPU0 \tCPU1\tONLINE\tNR_RUN\n");

	cnt = cpu_load_freq_history_view_cnt - 1;
	ret = show_cpu_load_freq_sub(cnt, range, buf, (BUF_SIZE - ret) ,ret);
	ret +=  snprintf(buf + ret, BUF_SIZE - ret, "\n");

	end_cnt = cnt;
	start_cnt = get_index(end_cnt, cpu_load_history_num, (0 -range + 1) );

	sprintf(range_buf, "%d-%d", start_cnt ,end_cnt);
	set_cpu_load_freq_history_array_range(range_buf);

	ret = check_running_buf_print(max_list_num, buf, (BUF_SIZE - ret), ret, &ret_check_valid);

	cpu_print_buf_to_klog(buf);

	vfree(buf);
}

#if defined(BUILD_ERROR)
static int cpuidle_w_aftr_jig_check_en_read_sub(char *buf, int buf_size)
{
	unsigned int ret = 0;

	ret = sprintf(buf, "%d\n", cpuidle_get_w_aftr_jig_check_enable());

	return ret;
}

static ssize_t cpuidle_w_aftr_jig_check_en_read(struct file *file,
		char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int size_for_copy;

	size_for_copy = wrapper_for_debug_fs(buffer, count, ppos, cpuidle_w_aftr_jig_check_en_read_sub);

	return size_for_copy;
}

static ssize_t cpuidle_w_aftr_jig_check_en_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int input;
	input = atoi(user_buf);

	if ((input == 0 ) || (input == 1 ))
		cpuidle_set_w_aftr_jig_check_enable(input);

	return count;
}
#endif

void load_anlyzer_data_memcpy(void)
{
	cpu_load_freq_history_view_cnt = cpu_load_freq_history_cnt;
	memcpy(cpu_load_freq_history_view, cpu_load_freq_history, CPU_LOAD_FREQ_HISTORY_SIZE);
	memcpy(cpu_task_history_view, cpu_task_history, CPU_TASK_HISTORY_SIZE);
#if defined (CONFIG_CHECK_WORK_HISTORY)
	memcpy(cpu_work_history_view, cpu_work_history, CPU_WORK_HISTORY_SIZE);
#endif

#if defined (CONFIG_SLP_INPUT_REC)
	memcpy(input_rec_history_view, input_rec_history, INPUT_REC_HISTORY_SIZE);
	b_input_load_data = 0;
#endif
}

#define CPU_SHOW_LINE_NUM	30
static ssize_t cpu_load_freq_read(struct file *file,
		char __user *buffer, size_t count, loff_t *ppos)
{
	ssize_t ret = 0;
	char *buf;
	static int cnt = 0, show_line_num = 0,  remained_line = 0;

	if (*ppos < 0 || !count)
		return -EINVAL;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (*ppos == 0) {
		load_anlyzer_data_memcpy();

		remained_line = cpu_load_freq_history_show_cnt;
		cnt = cpu_load_freq_history_view_cnt - 1;
		cnt = get_index(cnt, cpu_load_history_num, (0 - remained_line));

		ret +=  snprintf(buf + ret, PAGE_SIZE - ret, cpu_load_freq_menu);
	}

	if (remained_line >= CPU_SHOW_LINE_NUM) {
		show_line_num = CPU_SHOW_LINE_NUM;
		remained_line -= CPU_SHOW_LINE_NUM;

	} else {
		show_line_num = remained_line;
		remained_line = 0;
	}

	cnt = get_index(cnt, cpu_load_history_num, show_line_num);
	ret = show_cpu_load_freq_sub(cnt, show_line_num, buf, count,ret);

	if (ret >= 0) {
		if (copy_to_user(buffer, buf, ret)) {
			kfree(buf);
			return -EFAULT;
		}
		*ppos += ret;
	}

	kfree(buf);

	return ret;
}

static ssize_t cpu_load_freq_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int show_num = 0;

	show_num = atoi(user_buf);

	if (show_num <= cpu_load_history_num)
		cpu_load_freq_history_show_cnt = show_num;
	else
		return -EINVAL;

	return count;
}

#if defined(CONFIG_SLP_CHECK_BUS_LOAD)
#define CPU_BUS_SHOW_LINE_NUM	10
static ssize_t cpu_bus_load_freq_read(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	ssize_t ret = 0;
	char *buf;
	static int cnt = 0, show_line_num = 0,  remained_line = 0;

	if (*ppos < 0 || !count)
		return -EINVAL;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (*ppos == 0) {
		load_anlyzer_data_memcpy();

		remained_line = cpu_bus_load_freq_history_show_cnt;
		cnt = cpu_load_freq_history_view_cnt - 1;
		cnt = get_index(cnt, cpu_load_history_num, (0 - remained_line));

		ret +=  snprintf(buf + ret, PAGE_SIZE - ret, cpu_bus_load_freq_menu);
	}

	if (remained_line >= CPU_BUS_SHOW_LINE_NUM) {
		show_line_num = CPU_BUS_SHOW_LINE_NUM;
		remained_line -= CPU_BUS_SHOW_LINE_NUM;

	} else {
		show_line_num = remained_line;
		remained_line = 0;
	}
	cnt = get_index(cnt, cpu_load_history_num, show_line_num);

	ret = show_cpu_bus_load_freq_sub(cnt, show_line_num, buf, count, ret);

	if (ret >= 0) {
		if (copy_to_user(buffer, buf, ret)) {
			kfree(buf);
			return -EFAULT;
		}
		*ppos += ret;
	}

	kfree(buf);

	return ret;
}

static int cpu_load_freq_history_view_start_num;
static int cpu_load_freq_history_view_end_num;

#define CPU_BUS_SHOW_LINE_NUM	10
static ssize_t cpu_bus_load_freq_view_read(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	ssize_t ret = 0;
	char *buf;
	static int cnt = 0, show_line_num = 0,  remained_line = 0;

	if (*ppos < 0 || !count)
		return -EINVAL;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (*ppos == 0) {
		if (cpu_load_freq_history_view_end_num >= cpu_load_freq_history_view_start_num) {
			remained_line = cpu_load_freq_history_view_end_num
								- cpu_load_freq_history_view_start_num + 1;
		} else {
			remained_line = cpu_load_freq_history_view_end_num + cpu_load_history_num
								- cpu_load_freq_history_view_start_num + 1;
		}
		cnt = cpu_load_freq_history_view_end_num;
		cnt = get_index(cnt, cpu_load_history_num, (0 - remained_line));

		ret +=  snprintf(buf + ret, PAGE_SIZE - ret, cpu_bus_load_freq_menu);
	}

	if (remained_line >= CPU_BUS_SHOW_LINE_NUM) {
		show_line_num = CPU_BUS_SHOW_LINE_NUM;
		remained_line -= CPU_BUS_SHOW_LINE_NUM;

	} else {
		show_line_num = remained_line;
		remained_line = 0;
	}
	cnt = get_index(cnt, cpu_load_history_num, show_line_num);

	ret = show_cpu_bus_load_freq_sub(cnt, show_line_num, buf, count, ret);

	if (ret >= 0) {
		if (copy_to_user(buffer, buf, ret)) {
			kfree(buf);
			return -EFAULT;
		}
		*ppos += ret;
	}

	kfree(buf);

	return ret;
}

static ssize_t cpu_bus_load_freq_write(struct file *file,
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

static ssize_t cpu_bus_load_freq_view_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	char cpy_buf[80] = {0,};
	char *p1, *p_lf;

	p1 = strstr(user_buf, "-");
	p_lf = strstr(user_buf, "\n");

	if (p1 != NULL) {
		strncpy(cpy_buf, user_buf, sizeof(cpy_buf) - 1);
		*p1 = '\0';
		cpu_load_freq_history_view_start_num = atoi(cpy_buf);
		cpu_load_freq_history_view_end_num = atoi(p1+1);
	} else {
		return -1;
	}

	return count;
}
#endif

#if defined(CONFIG_SLP_BUS_CLK_CHECK_LOAD)
#define CPU_BUS_CLK_SHOW_LINE_NUM	10
static ssize_t cpu_bus_clk_read(struct file *file,
		char __user *buffer, size_t count, loff_t *ppos)
{
	ssize_t ret = 0;
	char *buf;
	static int cnt = 0, show_line_num = 0,  remained_line = 0;

	if (*ppos < 0 || !count)
		return -EINVAL;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (*ppos == 0) {
		load_anlyzer_data_memcpy();

		remained_line = cpu_bus_clk_load_freq_history_show_cnt;
		cnt = cpu_load_freq_history_view_cnt - 1;
		cnt = get_index(cnt, cpu_load_history_num, (0 - remained_line));

		ret +=  snprintf(buf + ret, PAGE_SIZE - ret, cpu_bus_clk_load_freq_menu);
	}

	if (remained_line >= CPU_BUS_CLK_SHOW_LINE_NUM) {
		show_line_num = CPU_BUS_CLK_SHOW_LINE_NUM;
		remained_line -= CPU_BUS_CLK_SHOW_LINE_NUM;

	} else {
		show_line_num = remained_line;
		remained_line = 0;
	}
	cnt = get_index(cnt, cpu_load_history_num, show_line_num);

	ret = show_cpu_bus_clk_load_freq_sub(cnt, show_line_num, buf,  ret);

	if (ret >= 0) {
		if (copy_to_user(buffer, buf, ret)) {
			kfree(buf);
			return -EFAULT;
		}
		*ppos += ret;
	}

	kfree(buf);

	return ret;
}

static ssize_t cpu_bus_clk_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int show_num = 0;

	show_num = atoi(user_buf);
	if (show_num <= cpu_load_history_num)
		cpu_bus_clk_load_freq_history_show_cnt = show_num;
	else
		return -EINVAL;

	return count;
}

static unsigned int checking_clk_index;

static ssize_t check_clk_read(struct file *file,
		char __user *buffer, size_t count, loff_t *ppos)
{
	static char *buf = NULL;
	int buf_size = (PAGE_SIZE * 3);
	unsigned int ret = 0, size_for_copy = count;
	static unsigned int rest_size = 0;

	unsigned int *clk_gates;
	unsigned int *power_domains;

	if (*ppos < 0 || !count)
		return -EINVAL;

	if (*ppos == 0) {
		buf = kmalloc(buf_size, GFP_KERNEL);
		if (!buf)
			return -ENOMEM;

		clk_gates = cpu_load_freq_history_view[checking_clk_index].clk_gates;
		power_domains = cpu_load_freq_history_view[checking_clk_index].power_domains;

		ret = clk_mon_get_clock_info(power_domains, clk_gates, buf);

		if (ret <= count) {
			size_for_copy = ret;
			rest_size = 0;
		} else {
			size_for_copy = count;
			rest_size = ret -size_for_copy;
		}
	} else {
		if (rest_size <= count) {
			size_for_copy = rest_size;
			rest_size = 0;
		} else {
			size_for_copy = count;
			rest_size -= size_for_copy;
		}
	}

	if (size_for_copy >  0) {
		int offset = (int) *ppos;
		if (copy_to_user(buffer, buf + offset , size_for_copy)) {
			kfree(buf);
			return -EFAULT;
		}
		*ppos += size_for_copy;
	} else
		kfree(buf);

	return size_for_copy;
}

static ssize_t check_clk_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int show_num = 0;

	show_num = atoi(user_buf);

	if (show_num <= cpu_load_history_num)
		checking_clk_index = show_num;
	else
		return -EINVAL;

	return count;
}
#endif

int check_running_buf_print(int max_list_num, char *buf, int buf_size,
		int ret, int *ret_check_valid)
{
	int i = 0;
	unsigned long long t;
	unsigned int cpu;

	unsigned int start_cnt = cpu_task_history_show_start_cnt;
	unsigned int end_cnt = cpu_task_history_show_end_cnt;
	unsigned long  msec_rem;
	int check_valid_data = 0;

	*ret_check_valid = 0;

	for (i = 0; i < CPU_NUM ; i++) {
		if (cpu_task_history_show_select_cpu != -1)
			if (i != cpu_task_history_show_select_cpu)
				continue;

		cpu = i;
		check_valid_data = check_valid_range(cpu, start_cnt, end_cnt);

		if (check_valid_data < 0)	{
			*ret_check_valid = check_valid_data;
			ret +=  snprintf(buf + ret, buf_size - ret,
					"[ERROR] cpu[%d] Invalid range !!! err=%d\n", cpu, check_valid_data);
			pr_info("[ERROR] cpu[%d] Invalid range !!! err=%d\n", cpu, check_valid_data);
			continue;
		}

		clc_process_run_time(i, start_cnt, end_cnt);

		t = total_time;
		msec_rem = do_div(t, 1000000);

		if (end_cnt == start_cnt+1) {
			ret += snprintf(buf + ret, buf_size - ret,
					"[%d] TOTAL SECTION TIME = %ld.%ld[ms]\n\n", end_cnt, (unsigned long)t, msec_rem);
		} else {
			ret += snprintf(buf + ret, buf_size - ret,
					"[%d~%d] TOTAL SECTION TIME = %ld.%ld[ms]\n\n",
					start_cnt + 1, end_cnt, (unsigned long)t, msec_rem);
		}

		ret += snprintf(buf + ret, buf_size - ret,
				"####################################"
				" CPU %d ##############################\n", i);

		if (cpu_task_history_show_select_cpu == -1)
			ret = view_list(max_list_num, buf, buf_size, ret);
		else if (i == cpu_task_history_show_select_cpu)
			ret = view_list(max_list_num, buf, buf_size, ret);

		if (ret < buf_size - 1)
			ret +=  snprintf(buf + ret, buf_size - ret, "\n\n");
	}

	return ret;
}

static ssize_t check_running_read(struct file *file,
		char __user *buffer, size_t count, loff_t *ppos)
{
	static char *buf = NULL;
	int buf_size = (PAGE_SIZE * 256);
	unsigned int i, ret = 0, size_for_copy = count;
	int ret_check_valid;
	static unsigned int rest_size = 0;

	unsigned int start_cnt = cpu_task_history_show_start_cnt;
	unsigned int end_cnt = cpu_task_history_show_end_cnt;
	unsigned long  msec_rem;
	unsigned long long t;
	unsigned int cpu;

	if (*ppos < 0 || !count)
		return -EINVAL;

	if (*ppos == 0) {
		buf = vmalloc(buf_size);

		if (!buf)
			return -ENOMEM;

		if ((end_cnt) == (start_cnt + 1)) {
			ret +=  snprintf(buf + ret, PAGE_SIZE - ret, cpu_load_freq_menu);

			ret = show_cpu_load_freq_sub((int)end_cnt+2, 5, buf, buf_size, ret);
			ret +=  snprintf(buf + ret, buf_size - ret, "\n\n");
		}

		for (i = 0; i < CPU_NUM ; i++) {
			if (cpu_task_history_show_select_cpu != -1)
				if (i != cpu_task_history_show_select_cpu)
					continue;

			cpu = i;
			ret_check_valid = check_valid_range(cpu, start_cnt, end_cnt);

			if (ret_check_valid < 0) {
				ret +=  snprintf(buf + ret, buf_size - ret,
						"[ERROR] cpu[%d] Invalid range !!! err=%d\n", cpu, ret_check_valid);
				pr_info("[ERROR] cpu[%d] Invalid range !!! err=%d\n", cpu, ret_check_valid);
				continue;
			}

			clc_process_run_time(i, start_cnt, end_cnt);

			t = total_time;
			msec_rem = do_div(t, 1000000);

			if (end_cnt == start_cnt+1) {
				ret += snprintf(buf + ret, buf_size - ret,
						"[%d] TOTAL SECTION TIME = %ld.%ld[ms]\n\n", end_cnt, (unsigned long)t, msec_rem);
			} else {
				ret += snprintf(buf + ret, buf_size - ret,
						"[%d~%d] TOTAL SECTION TIME = %ld.%ld[ms]\n\n",
						start_cnt + 1, end_cnt, (unsigned long)t, msec_rem);
			}

			ret += snprintf(buf + ret, buf_size - ret,
					"####################################"
				" CPU %d ##############################\n", i);

			if (cpu_task_history_show_select_cpu == -1)
				ret = view_list(INT_MAX, buf, buf_size, ret);
			else if (i == cpu_task_history_show_select_cpu)
				ret = view_list(INT_MAX, buf, buf_size, ret);

			if (ret < buf_size - 1)
				ret +=  snprintf(buf + ret, buf_size - ret, "\n\n");
		}

		if (ret <= count) {
			size_for_copy = ret;
			rest_size = 0;
		} else {
			size_for_copy = count;
			rest_size = ret -size_for_copy;
		}
	} else {
		if (rest_size <= count) {
			size_for_copy = rest_size;
			rest_size = 0;
		} else {
			size_for_copy = count;
			rest_size -= size_for_copy;
		}
	}

	if (size_for_copy > 0) {
		int offset = (int) *ppos;
		if (copy_to_user(buffer, buf + offset , size_for_copy)) {
			vfree(buf);
			return -EFAULT;
		}
		*ppos += size_for_copy;
	} else
		vfree(buf);

	return size_for_copy;
}

static ssize_t check_running_raw_read(struct file *file,
		char __user *buffer, size_t count, loff_t *ppos)
{
	static char *buf = NULL;
	int buf_size = (PAGE_SIZE * 256);
	unsigned int i, ret = 0, size_for_copy = count;
	int ret_check_valid;
	static unsigned int rest_size = 0;

	unsigned int start_cnt = cpu_task_history_show_start_cnt;
	unsigned int end_cnt = cpu_task_history_show_end_cnt;
	unsigned long  msec_rem;
	unsigned long long t;
	unsigned int cpu;

	if (*ppos < 0 || !count)
		return -EINVAL;

	if (*ppos == 0) {
		buf = vmalloc(buf_size);

		if (!buf)
			return -ENOMEM;

		if ((end_cnt) == (start_cnt + 1)) {
			ret +=  snprintf(buf + ret, buf_size - ret
					, "=======================================" \
					"========================================" \
					"========================================\n");

			ret +=  snprintf(buf + ret, buf_size - ret,
					"    TIME       CPU0_F   CPU1_F  CPU_LOCK"
					"    [INDEX]\tCPU0 \tCPU1 \tONLINE\tNR_RUN\n");

			ret = show_cpu_load_freq_sub((int)end_cnt+2, 5, buf, buf_size, ret);
			ret +=  snprintf(buf + ret, buf_size - ret, "\n\n");
		}

		for (i = 0; i < CPU_NUM ; i++) {
			if (cpu_task_history_show_select_cpu != -1)
				if (i != cpu_task_history_show_select_cpu)
					continue;
			cpu = i;
			ret_check_valid = check_valid_range(cpu, start_cnt, end_cnt);
			if (ret_check_valid < 0) {
				ret +=  snprintf(buf + ret, buf_size - ret,
						"[ERROR] cpu[%d] Invalid range !!! err=%d\n", cpu, ret_check_valid);
				pr_info("[ERROR] cpu[%d] Invalid range !!! err=%d\n", cpu, ret_check_valid);
				continue;
			}

			clc_process_run_time(i, start_cnt, end_cnt);

			t = total_time;
			msec_rem = do_div(t, 1000000);

			ret += snprintf(buf + ret, buf_size - ret, "# %d\n", i);

			if (end_cnt == start_cnt+1) {
				ret +=  snprintf(buf + ret, buf_size - ret,
						"[%d] TOTAL SECTION TIME = %ld.%ld[ms]\n"
						, end_cnt, (unsigned long)t, msec_rem);
			}

			if (cpu_task_history_show_select_cpu == -1)
				ret = view_list_raw(buf, buf_size, ret);
			else if (i == cpu_task_history_show_select_cpu)
				ret = view_list_raw(buf, buf_size, ret);

		}

		if (ret <= count) {
			size_for_copy = ret;
			rest_size = 0;
		} else {
			size_for_copy = count;
			rest_size = ret -size_for_copy;
		}
	} else {
		if (rest_size <= count) {
			size_for_copy = rest_size;
			rest_size = 0;
		} else {
			size_for_copy = count;
			rest_size -= size_for_copy;
		}
	}

	if (size_for_copy >  0) {
		int offset = (int) *ppos;
		if (copy_to_user(buffer, buf + offset , size_for_copy)) {
			vfree(buf);
			return -EFAULT;
		}
		*ppos += size_for_copy;
	} else
		vfree(buf);

	return size_for_copy;
}

static ssize_t check_running_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	set_cpu_load_freq_history_array_range(user_buf);

#if defined(CONFIG_CHECK_WORK_HISTORY)
	cpu_work_history_show_select_cpu = cpu_task_history_show_select_cpu;
	cpu_work_history_show_start_cnt= cpu_task_history_show_start_cnt;
	cpu_work_history_show_end_cnt = cpu_task_history_show_end_cnt;
#endif

	return count;
}

static int check_running_detail_sub(char *buf, int buf_size)
{
	int ret = 0, i = 0, ret_check_valid = 0;
	unsigned int start_cnt = cpu_task_history_show_start_cnt;
	unsigned int end_cnt = cpu_task_history_show_end_cnt;
	unsigned int cpu;

	for (i = 0; i < CPU_NUM; i++) {
		if (cpu_task_history_show_select_cpu != -1)
			if (i != cpu_task_history_show_select_cpu)
				continue;

		cpu = i;
		ret_check_valid = check_valid_range(cpu, start_cnt, end_cnt);

		if (ret_check_valid < 0)	{
			ret +=  snprintf(buf + ret, buf_size - ret,
					"[ERROR] cpu[%d] Invalid range !!! err=%d\n", cpu, ret_check_valid);
			pr_info("[ERROR] cpu[%d] Invalid range !!! err=%d\n", cpu, ret_check_valid);
			continue;
		}

		ret += snprintf(buf + ret, buf_size - ret,
				"########################## CPU %d #########################\n", i);

		ret = process_sched_time_view(i, start_cnt, end_cnt, buf, buf_size, ret);

		ret += snprintf(buf + ret, buf_size - ret, "\n\n");
	}

	return ret;
}

static ssize_t check_running_detail(struct file *file,
		char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int size_for_copy;

	size_for_copy = wrapper_for_debug_fs(buffer, count, ppos, check_running_detail_sub);

	return size_for_copy;
}

#if defined(CONFIG_CHECK_NOT_CPUIDLE_CAUSE)
static ssize_t not_lpa_cause_check(struct file *file,
		char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int size_for_copy;

	size_for_copy = wrapper_for_debug_fs(buffer, count, ppos,\
			not_lpa_cause_check_sub);

	return size_for_copy;
}
#endif

int value_for_debug;

static int debug_value_read_sub(char *buf, int buf_size)
{
	int ret = 0;

	ret +=  snprintf(buf + ret, buf_size - ret, "%d\n", value_for_debug);

	return ret;
}

static ssize_t debug_value_read(struct file *file,
		char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int size_for_copy;

	size_for_copy = wrapper_for_debug_fs(buffer, count, ppos, debug_value_read_sub);

	return size_for_copy;
}

static ssize_t debug_value_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	value_for_debug = atoi(user_buf);

	return count;
}

extern unsigned int g_cpu_num_limit;

#if defined(CONFIG_LOAD_ANALYZER_PMQOS)
static struct pm_qos_request pm_qos_min_cpu, pm_qos_max_cpu;
static struct pm_qos_request pm_qos_min_cpu_num, pm_qos_max_cpu_num;

static int la_cpu_freq_min_value;

static int cpu_freq_min_read_sub(char *buf, int buf_size)
{
	int ret = 0;

	ret +=  snprintf(buf + ret, buf_size - ret, "%d\n", la_cpu_freq_min_value);

	return ret;
}

static ssize_t cpu_freq_min_read(struct file *file,
		char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int size_for_copy;

	size_for_copy = wrapper_for_debug_fs(buffer, count, ppos, cpu_freq_min_read_sub);

	return size_for_copy;
}

static ssize_t cpu_freq_min_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int cpu_freq_min_value;

	pr_info("%s user_buf=%s\n", __FUNCTION__, user_buf);

	if ((user_buf[0] == '-') && (user_buf[1] == '1'))
		cpu_freq_min_value = -1;
	else
		cpu_freq_min_value = atoi(user_buf);

	if (cpu_freq_min_value == -1) {
		la_cpu_freq_min_value = cpu_freq_min_value;
		pm_qos_update_request(&pm_qos_min_cpu, 0);
	} else {
		la_cpu_freq_min_value = cpu_freq_min_value;
		pm_qos_update_request(&pm_qos_min_cpu, cpu_freq_min_value);
	}

	return count;
}

static int la_cpu_freq_max_value;

static int cpu_freq_max_read_sub(char *buf, int buf_size)
{
	int ret = 0;

	ret +=  snprintf(buf + ret, buf_size - ret, "%d\n", la_cpu_freq_max_value);

	return ret;
}

static ssize_t cpu_freq_max_read(struct file *file,
		char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int size_for_copy;

	size_for_copy = wrapper_for_debug_fs(buffer, count, ppos, cpu_freq_max_read_sub);

	return size_for_copy;
}

static ssize_t cpu_freq_max_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int cpu_freq_max_value;

	pr_info("%s user_buf=%s\n", __FUNCTION__, user_buf);

	if ((user_buf[0] == '-') && (user_buf[1] == '1'))
		cpu_freq_max_value = -1;
	else
		cpu_freq_max_value = atoi(user_buf);

	if (cpu_freq_max_value == -1) {
		la_cpu_freq_max_value = cpu_freq_max_value;
		pm_qos_update_request(&pm_qos_max_cpu, INT_MAX);
	} else {
		la_cpu_freq_max_value = cpu_freq_max_value;
		pm_qos_update_request(&pm_qos_max_cpu, cpu_freq_max_value);
	}

	return count;
}

static int la_cpu_online_min_value;

static int cpu_online_min_read_sub(char *buf, int buf_size)
{
	int ret = 0;

	ret +=  snprintf(buf + ret, buf_size - ret, "%d\n", la_cpu_online_min_value);

	return ret;
}

static ssize_t cpu_online_min_read(struct file *file,
		char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int size_for_copy;

	size_for_copy = wrapper_for_debug_fs(buffer, count, ppos
			, cpu_online_min_read_sub);

	return size_for_copy;
}

static ssize_t cpu_online_min_write(struct file *file,
		const char __user *user_buf, size_t count,
		loff_t *ppos)
{
	int cpu_online_min_value;

	pr_info("%s user_buf=%s\n", __FUNCTION__, user_buf);

	if ((user_buf[0] == '-') && (user_buf[1] == '1'))
		cpu_online_min_value = -1;
	else
		cpu_online_min_value = atoi(user_buf);

	if (cpu_online_min_value == -1) {
		la_cpu_online_min_value = cpu_online_min_value;
		pm_qos_update_request(&pm_qos_min_cpu_num, 0);
	} else {
		la_cpu_online_min_value = cpu_online_min_value;
		pm_qos_update_request(&pm_qos_min_cpu_num, cpu_online_min_value);
	}

	return count;
}

static int la_cpu_online_max_value;

static int cpu_online_max_read_sub(char *buf, int buf_size)
{
	int ret = 0;

	ret +=  snprintf(buf + ret, buf_size - ret, "%d\n", la_cpu_online_max_value);

	return ret;
}

static ssize_t cpu_online_max_read(struct file *file,
		char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int size_for_copy;

	size_for_copy = wrapper_for_debug_fs(buffer, count, ppos
			, cpu_online_max_read_sub);

	return size_for_copy;
}

static ssize_t cpu_online_max_write(struct file *file,
		const char __user *user_buf, size_t count,
		loff_t *ppos)
{
	int cpu_online_max_value;

	pr_info("%s user_buf=%s\n", __FUNCTION__, user_buf);

	if ((user_buf[0] == '-') && (user_buf[1] == '1'))
		cpu_online_max_value = -1;
	else
		cpu_online_max_value = atoi(user_buf);

	if (cpu_online_max_value == -1) {
		la_cpu_online_max_value = cpu_online_max_value;
		pm_qos_update_request(&pm_qos_max_cpu_num, CPU_NUM);
	} else {
		la_cpu_online_max_value = cpu_online_max_value;
		pm_qos_update_request(&pm_qos_max_cpu_num, cpu_online_max_value);
	}

	return count;
}

static int la_fixed_cpufreq_value;

static int check_valid_cpufreq(unsigned int cpu_freq)
{
	unsigned int i = 0, found = 0;
	struct cpufreq_frequency_table *table;

	table = cpufreq_frequency_get_table(0);

	for (i = 0; (table[i].frequency != CPUFREQ_TABLE_END); i++) {
		if (table[i].frequency == CPUFREQ_ENTRY_INVALID)
			continue;
		if (cpu_freq == table[i].frequency) {
			found = 1;
			break;
		}
	}

	return found;
}

static int fixed_cpu_freq_read_sub(char *buf, int buf_size)
{
	int ret = 0;

	ret +=  snprintf(buf + ret, buf_size - ret, "%d\n", la_fixed_cpufreq_value);

	return ret;
}

static ssize_t fixed_cpu_freq_read(struct file *file,
		char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int size_for_copy;

	size_for_copy = wrapper_for_debug_fs(buffer, count, ppos,\
			fixed_cpu_freq_read_sub);

	return size_for_copy;
}

static ssize_t fixed_cpu_freq_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct cpufreq_policy *policy;

	int fixed_cpufreq_value;

	pr_info("%s user_buf=%s\n", __FUNCTION__, user_buf);

	if ((user_buf[0] == '-') && (user_buf[1] == '1'))
		fixed_cpufreq_value = -1;
	else
		fixed_cpufreq_value = atoi(user_buf);

	if (fixed_cpufreq_value == -1) {
		la_fixed_cpufreq_value = fixed_cpufreq_value;

		pm_qos_update_request(&pm_qos_min_cpu, 0);
		pm_qos_update_request(&pm_qos_max_cpu, INT_MAX);
	} else {
		policy = cpufreq_cpu_get(0);

		if (check_valid_cpufreq(fixed_cpufreq_value)!=1) {
			pr_err("Invalid cpufreq : %d\n", fixed_cpufreq_value);
			return -EINVAL;
		}

		la_fixed_cpufreq_value = fixed_cpufreq_value;

		pm_qos_update_request(&pm_qos_min_cpu, fixed_cpufreq_value);
		pm_qos_update_request(&pm_qos_max_cpu, fixed_cpufreq_value);
	}

	return count;
}
#endif

static int available_cpu_freq_read_sub(char *buf, int buf_size)
{
	int ret = 0;
	struct cpufreq_frequency_table *table;
	int i = 0;

	table = cpufreq_frequency_get_table(0);

	for (i = 0; (table[i].frequency != CPUFREQ_TABLE_END); i++) {
		if (table[i].frequency == CPUFREQ_ENTRY_INVALID)
			continue;
		ret += snprintf(buf + ret, buf_size - ret, "%d ", table[i].frequency);
	}
	ret += snprintf(buf + ret, buf_size - ret, "\n");

	return ret;
}

static ssize_t available_cpu_freq_read(struct file *file,
		char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int size_for_copy;

	size_for_copy = wrapper_for_debug_fs(buffer, count, ppos ,available_cpu_freq_read_sub);

	return size_for_copy;
}

#if defined(CONFIG_SLP_CHECK_BUS_LOAD)
int saved_range_num=100, saved_max_list_num=100;
static int last_cpu_load_read_sub(char *buf, int buf_size)
{
	int start_ret=0, ret = 0, cnt = 0;
	int start_cnt = 0, end_cnt = 0;
	char range_buf[64] = {0,};
	int ret_check_valid = -1;
	int range_num;
	int loop_cnt = 0;
	#define TRY_MAX_LOOP_CNT	100

	cpu_load_freq_history_view_cnt = cpu_load_freq_history_cnt;
	memcpy(cpu_load_freq_history_view, cpu_load_freq_history
			, CPU_LOAD_FREQ_HISTORY_SIZE);
	memcpy(cpu_task_history_view, cpu_task_history
			, CPU_TASK_HISTORY_SIZE);

	range_num = saved_range_num;

	ret +=  snprintf(buf + ret, buf_size - ret, "\n\n===== [LAST CPU LOAD] ===== \n");

	start_ret = ret;

	do {
		ret = start_ret; /* Initialize ret value */
		cnt = cpu_load_freq_history_view_cnt - 1;
		end_cnt = cnt;
		start_cnt = get_index(end_cnt, cpu_load_history_num, (0 -range_num + 1) );
		sprintf(range_buf, "%d-%d", start_cnt ,end_cnt);

		set_cpu_load_freq_history_array_range(range_buf);

		/* start_cnt+1 mean that real start from start_cnt+1 */
		if (cpu_load_freq_history_view[start_cnt+1].time_stamp == 0) {
			range_num--;
			continue;
		}

		ret +=  snprintf(buf + ret, buf_size - ret, cpu_bus_load_freq_menu);
		ret = show_cpu_bus_load_freq_sub(cnt, range_num, buf, (buf_size - ret) ,ret);
		ret +=  snprintf(buf + ret, buf_size - ret, "\n");


		ret = check_running_buf_print(saved_max_list_num, buf, (buf_size - ret), ret, &ret_check_valid);

		if (ret_check_valid < 0) {
			ret +=  snprintf(buf + ret, buf_size - ret, "\n\n=== Need to retry! === \n\n");
			range_num = range_num - 2;
		}
	} while ((ret_check_valid < 0) && (++loop_cnt <= TRY_MAX_LOOP_CNT));

	return ret;
}

static ssize_t last_cpu_load_read(struct file *file,
		char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int size_for_copy;

	size_for_copy = wrapper_for_debug_fs(buffer, count
			, ppos ,last_cpu_load_read_sub);

	return size_for_copy;
}

static ssize_t last_cpu_load_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
#define MAX_RANGE_NUM	1000
	int range_num, max_list_num;

	char *p1 = NULL, *p_lf = NULL;

	if (cpu_task_history_onoff == 0)
		return -EINVAL;

	p_lf = strstr(user_buf, "\n");

	p1= strstr(user_buf, "TOP");

	if (p1 == NULL)
		p1= strstr(user_buf, "top");

	if (p_lf != NULL) {
		if (p1 - p_lf > 0)
			p1 = NULL;
	}

	if (p1 != NULL)
		max_list_num = atoi(p1+3);
	else
		max_list_num = INT_MAX;

	range_num = atoi(user_buf);

	if ((range_num > 0) && (range_num <= MAX_RANGE_NUM)) {
		saved_range_num = range_num;
		saved_max_list_num = max_list_num;
	} else {
		return -EINVAL;
	}

	return count;
}
#endif

struct cpuidle_device *cpu_idle_dev;
unsigned long long cpuidle_time_now[3];
unsigned long long cpuidle_time_before[3];

unsigned int cpuidle_get_idle_residency_time(int idle_state)
{
	static int first_time[3] = {1, 1, 1};
	int cpuidle_residency_time;

	if(cpu_idle_dev == NULL)
		return 0;

	if ((idle_state < 0) || (idle_state > 2)) {
		pr_info("idle_state ERROR idle_state=%d\n", idle_state);
		return 0;
	}

	cpuidle_time_now[idle_state] = cpu_idle_dev->states_usage[idle_state].time;

	if (first_time[idle_state]== 1) {
		first_time[idle_state] = 0;
		cpuidle_time_before[idle_state] = cpuidle_time_now[idle_state];
	}

	cpuidle_residency_time = (unsigned int)(cpuidle_time_now[idle_state] -cpuidle_time_before[idle_state]);

	cpuidle_time_before[idle_state] = cpuidle_time_now[idle_state];

	return cpuidle_residency_time;
}

static const struct file_operations available_cpu_freq_fops = {
	.owner = THIS_MODULE,
	.read  = available_cpu_freq_read,
};

static const struct file_operations cpu_load_freq_fops = {
	.owner = THIS_MODULE,
	.read  = cpu_load_freq_read,
	.write = cpu_load_freq_write,
};

#if defined(CONFIG_SLP_CHECK_BUS_LOAD)
static const struct file_operations last_cpu_load_fops = {
	.owner = THIS_MODULE,
	.write = last_cpu_load_write,
	.read  = last_cpu_load_read,
};
#endif

#if defined(BUILD_ERROR)
static const struct file_operations cpuidle_w_aftr_jig_check_en_fops = {
	.owner = THIS_MODULE,
	.read  = cpuidle_w_aftr_jig_check_en_read,
	.write = cpuidle_w_aftr_jig_check_en_write,
};
#endif

#if defined(CONFIG_SLP_CHECK_BUS_LOAD)
static const struct file_operations cpu_bus_load_freq_fops = {
	.owner = THIS_MODULE,
	.read  = cpu_bus_load_freq_read,
	.write = cpu_bus_load_freq_write,
};

static const struct file_operations cpu_bus_load_freq_view_fops = {
	.owner = THIS_MODULE,
	.read  = cpu_bus_load_freq_view_read,
	.write = cpu_bus_load_freq_view_write,
};
#endif

#if defined(CONFIG_SLP_BUS_CLK_CHECK_LOAD)
static const struct file_operations cpu_bus_clk_fops = {
	.owner = THIS_MODULE,
	.read  = cpu_bus_clk_read,
	.write = cpu_bus_clk_write,
};
#endif

#if defined(CONFIG_SLP_BUS_CLK_CHECK_LOAD)
static const struct file_operations check_clk_fops = {
	.owner = THIS_MODULE,
	.read  = check_clk_read,
	.write = check_clk_write,
};
#endif

static const struct file_operations check_running_fops = {
	.owner = THIS_MODULE,
	.read  = check_running_read,
	.write = check_running_write,
};

static const struct file_operations check_running_raw_fops = {
	.owner = THIS_MODULE,
	.read  = check_running_raw_read,
	.write = check_running_write,
};

static const struct file_operations check_running_detail_fops = {
	.owner = THIS_MODULE,
	.read  = check_running_detail,
};

#if defined(CONFIG_LOAD_ANALYZER_PMQOS)
static const struct file_operations cpu_freq_min_fops = {
	.owner = THIS_MODULE,
	.read  = cpu_freq_min_read,
	.write = cpu_freq_min_write,
};

static const struct file_operations cpu_freq_max_fops = {
	.owner = THIS_MODULE,
	.read  = cpu_freq_max_read,
	.write = cpu_freq_max_write,
};

static const struct file_operations fixed_cpu_freq_fops = {
	.owner = THIS_MODULE,
	.read  = fixed_cpu_freq_read,
	.write = fixed_cpu_freq_write,
};

static const struct file_operations cpu_online_min_fops = {
	.owner = THIS_MODULE,
	.read  = cpu_online_min_read,
	.write = cpu_online_min_write,
};

static const struct file_operations cpu_online_max_fops = {
	.owner = THIS_MODULE,
	.read  = cpu_online_max_read,
	.write = cpu_online_max_write,
};
#endif

void debugfs_cpu_bus(struct dentry *d)
{
	if (!debugfs_create_file("available_cpu_freq", 0400, d, NULL, &available_cpu_freq_fops))
		pr_err("%s : debugfs_create_file, error\n", "available_cpu_freq");

	if (!debugfs_create_file("cpu_load_freq", 0600, d, NULL, &cpu_load_freq_fops))
		pr_err("%s : debugfs_create_file, error\n", "cpu_load_freq");

#if defined(CONFIG_SLP_CHECK_BUS_LOAD)
	if (!debugfs_create_file("last_cpu_load", 0200, d, NULL, &last_cpu_load_fops))
		pr_err("%s : debugfs_create_file, error\n", "last_cpu_load");
#endif

	if (!debugfs_create_file("check_running", 0600, d, NULL, &check_running_fops))
		pr_err("%s : debugfs_create_file, error\n", "check_running");

	if (!debugfs_create_file("check_running_detail", 0600, d, NULL, &check_running_detail_fops))
		pr_err("%s : debugfs_create_file, error\n", "check_running_detail");

	if (!debugfs_create_file("check_running_raw", 0600, d, NULL, &check_running_raw_fops))
		pr_err("%s : debugfs_create_file, error\n", "check_running_raw");

#if defined(BUILD_ERROR)
	if (!debugfs_create_file("cpuidle_w_aftr_jig_check_en", 0600, d, NULL, &cpuidle_w_aftr_jig_check_en_fops))
		pr_err("%s : debugfs_create_file, error\n", "cpuidle_w_aftr_jig_check_en");
#endif

#if defined(CONFIG_SLP_CHECK_BUS_LOAD)
	if (!debugfs_create_file("cpu_bus_load_freq", 0600, d, NULL, &cpu_bus_load_freq_fops))
		pr_err("%s : debugfs_create_file, error\n", "cpu_bus_load_freq");

	if (!debugfs_create_file("cpu_bus_load_freq_view", 0600, d, NULL, &cpu_bus_load_freq_view_fops))
		pr_err("%s : debugfs_create_file, error\n", "cpu_bus_load_freq_view");
#endif

#if defined(CONFIG_SLP_BUS_CLK_CHECK_LOAD)
	if (!debugfs_create_file("cpu_bus_clk_freq", 0600, d, NULL, &cpu_bus_clk_fops))
		pr_err("%s : debugfs_create_file, error\n", "cpu_bus_clk_freq");

	if (!debugfs_create_file("check_clk", 0600, d, NULL, &check_clk_fops))
		pr_err("%s : debugfs_create_file, error\n", "check_clk");
#endif

#if defined(CONFIG_LOAD_ANALYZER_PMQOS)
	if (!debugfs_create_file("cpu_freq_min", 0600, d, NULL, &cpu_freq_min_fops))
		pr_err("%s : debugfs_create_file, error\n", "cpu_freq_min");

	if (!debugfs_create_file("cpu_freq_max", 0600, d, NULL, &cpu_freq_max_fops))
		pr_err("%s : debugfs_create_file, error\n", "cpu_freq_max");

	if (!debugfs_create_file("fixed_cpu_freq", 0600, d, NULL, &fixed_cpu_freq_fops))
		pr_err("%s : debugfs_create_file, error\n", "fixed_cpu_freq");

	if (!debugfs_create_file("cpu_online_min", 0600, d, NULL, &cpu_online_min_fops))
		pr_err("%s : debugfs_create_file, error\n", "cpu_online_min");

	if (!debugfs_create_file("cpu_online_max", 0600, d, NULL, &cpu_online_max_fops))
		pr_err("%s : debugfs_create_file, error\n", "cpu_online_max");
#endif
}
