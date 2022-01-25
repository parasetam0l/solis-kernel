
static unsigned long long calc_delta_time_work(unsigned int cpu, unsigned int index)
{
	unsigned long long run_start_time, run_end_time;


	run_start_time = cpu_work_history_view[index][cpu].start_time;
	if (run_start_time < section_start_time)
		run_start_time = section_start_time;

	run_end_time = cpu_work_history_view[index][cpu].end_time;

	if (run_end_time < section_start_time)
		return 0;

	if (run_end_time > section_end_time)
		run_end_time = section_end_time;

	return  run_end_time - run_start_time;
}

static void add_work_to_list(unsigned int cpu, unsigned int index)
{
	struct cpu_work_runtime_tag *new_work;

	new_work
		= kmalloc(sizeof(struct cpu_work_runtime_tag), GFP_KERNEL);

	new_work->occup_time =  calc_delta_time_work(cpu, index);

	new_work->cnt = 1;
	new_work->task = cpu_work_history_view[index][cpu].task;
	new_work->pid = cpu_work_history_view[index][cpu].pid;

	new_work->work = cpu_work_history_view[index][cpu].work;
	new_work->func = cpu_work_history_view[index][cpu].func;

	pr_info("%s %d\n", __func__, __LINE__);

	if (new_work->occup_time != 0) {
		INIT_LIST_HEAD(&new_work->list);
		list_add_tail(&new_work->list, &work_headlist);
	} else
		kfree(new_work);

	return;
}

static void del_work_list(void)
{
	struct cpu_work_runtime_tag *curr;
	struct list_head *p, *n;

	list_for_each_prev_safe(p, n, &work_headlist) {
		curr = list_entry(p, struct cpu_work_runtime_tag, list);
		kfree(curr);
	}
	work_headlist.prev = NULL;
	work_headlist.next = NULL;

}

static int comp_list_occuptime(struct list_head *list1, struct list_head *list2)
{
	struct cpu_work_runtime_tag *list1_struct, *list2_struct;

	int ret = 0;
	list1_struct = list_entry(list1, struct cpu_work_runtime_tag, list);
	list2_struct = list_entry(list2, struct cpu_work_runtime_tag, list);

	if (list1_struct->occup_time > list2_struct->occup_time)
		ret = 1;
	else if (list1_struct->occup_time < list2_struct->occup_time)
		ret = -1;
	else
		ret  = 0;

	return ret;
}

static unsigned int view_workfn_list(char *buf, unsigned int buf_size, unsigned int ret)
{
	struct list_head *p;
	struct cpu_work_runtime_tag *curr;
	unsigned int cnt = 0, list_num = 0;

	list_for_each(p, &work_headlist) {
		curr = list_entry(p, struct cpu_work_runtime_tag, list);
		list_num++;
	}

	for (cnt = 0; cnt < list_num; cnt++) {
		list_for_each(p, &work_headlist) {
		curr = list_entry(p, struct cpu_work_runtime_tag, list);
			if (p->next != &work_headlist) {
				if (comp_list_occuptime(p, p->next) == -1)
					swap_process_list(p, p->next);
			}
		}
	}

	cnt = 1;

	list_for_each(p, &work_headlist) {
		curr = list_entry(p, struct cpu_work_runtime_tag, list);
		if (ret < buf_size - 1) {
			ret +=  snprintf(buf + ret, buf_size - ret,
					"[%2d] %32pf(%4d) %16s %11lld[ns]\n",
					cnt++, curr->func, curr->cnt ,curr->task->comm ,curr->occup_time);
		}
	}

	return ret;
}

static struct cpu_work_runtime_tag *search_exist_workfn(work_func_t func)
{
	struct list_head *p;
	struct cpu_work_runtime_tag *curr;

	list_for_each(p, &work_headlist) {
		curr = list_entry(p, struct cpu_work_runtime_tag, list);
		if (curr->func == func)
			return curr;
	}

	return NULL;
}

static void clc_work_run_time(unsigned int cpu,
		unsigned int start_cnt, unsigned int end_cnt)
{
	unsigned  int cnt = 0,  start_array_num;
	unsigned int end_array_num, end_array_num_plus1;
	unsigned int i, loop_cnt;
	struct cpu_work_runtime_tag *work_runtime_data;
	unsigned long long t1, t2;

	start_array_num =
		(cpu_load_freq_history_view[start_cnt].work_history_cnt[cpu] + 1) % cpu_work_history_num;

	section_start_time = cpu_load_freq_history_view[start_cnt].time_stamp;
	section_end_time = cpu_load_freq_history_view[end_cnt].time_stamp;

	end_array_num = cpu_load_freq_history_view[end_cnt].work_history_cnt[cpu];
	end_array_num_plus1 = (cpu_load_freq_history_view[end_cnt].work_history_cnt[cpu] + 1) % cpu_work_history_num;

	t1 = cpu_work_history_view[end_array_num][cpu].end_time;
	t2 = cpu_work_history_view[end_array_num_plus1][cpu].end_time;

	if (t2 < t1)
		end_array_num_plus1 = end_array_num;

	total_time = section_end_time - section_start_time;

	if (work_headlist.next != NULL)
		del_work_list();

	INIT_LIST_HEAD(&work_headlist);

	if (end_array_num_plus1 >= start_array_num)
		loop_cnt = end_array_num_plus1-start_array_num + 1;
	else
		loop_cnt = end_array_num_plus1 + cpu_work_history_num - start_array_num + 1;

	for (i = start_array_num, cnt = 0; cnt < loop_cnt; cnt++, i++) {
		if (i >= cpu_work_history_num)
			i = 0;

		work_runtime_data = search_exist_workfn(cpu_work_history_view[i][cpu].func);

		if (work_runtime_data == NULL)
			add_work_to_list(cpu, i);
		else {
			work_runtime_data->occup_time += calc_delta_time_work(cpu, i);
			work_runtime_data->cnt++;
		}
	}
}

static unsigned int work_time_list_view(unsigned int cpu,
		unsigned int start_cnt, unsigned int end_cnt,
		char *buf, unsigned int buf_size, unsigned int ret)
{
	unsigned  int i = 0, start_array_num, data_line, cnt=0;
	unsigned int end_array_num, start_array_num_for_time;

	start_array_num_for_time = cpu_load_freq_history_view[start_cnt].work_history_cnt[cpu];
	start_array_num = (cpu_load_freq_history_view[start_cnt].work_history_cnt[cpu]+1) % cpu_work_history_num;
	end_array_num = cpu_load_freq_history_view[end_cnt].work_history_cnt[cpu];

	total_time = section_end_time - section_start_time;

	if (end_cnt == start_cnt+1) {
		ret +=  snprintf(buf + ret, buf_size - ret,
			"[%d] TOTAL SECTION TIME = %lld[ns]\n[%5d]~[%5d]/(%lld ~ %lld)\n\n",
			end_cnt, total_time,
			(cpu_load_freq_history_view[start_cnt].work_history_cnt[cpu] + 1) % cpu_task_history_num,
			(cpu_load_freq_history_view[end_cnt].work_history_cnt[cpu] + 1) % cpu_task_history_num,
			cpu_load_freq_history_view[start_cnt].time_stamp,
			cpu_load_freq_history_view[end_cnt].time_stamp);
	} else {
		ret +=  snprintf(buf + ret, buf_size - ret,
			"[%d~%d] TOTAL SECTION TIME = %lld[ns]\n[%5d]~[%5d]/(%lld ~ %lld)\n\n",
			get_index(start_cnt, cpu_work_history_num, 1),
			end_cnt, total_time,
			(cpu_load_freq_history_view[start_cnt].work_history_cnt[cpu] + 1) % cpu_task_history_num,
			(cpu_load_freq_history_view[end_cnt].work_history_cnt[cpu] + 1) % cpu_task_history_num,
			cpu_load_freq_history_view[start_cnt].time_stamp,
			cpu_load_freq_history_view[end_cnt].time_stamp);
	}

	end_array_num = get_index(end_array_num, cpu_work_history_num, 2);

	if (end_array_num >= start_array_num_for_time)
		data_line = end_array_num -start_array_num_for_time + 1;
	else
		data_line = (end_array_num + cpu_work_history_num) - start_array_num_for_time + 1;

	cnt = start_array_num_for_time;

	for (i = 0; i < data_line; i++) {
		u64 delta_time;
		char *p_name;
		char task_name[80] = {0,};
		unsigned int pid = 0;

		if (cnt > cpu_work_history_num-1)
			cnt = 0;

		delta_time = cpu_work_history_view[cnt][cpu].end_time \
				-cpu_work_history_view[cnt][cpu].start_time;

		pid = cpu_work_history_view[cnt][cpu].pid;
		if (cpu_work_history_view[cnt][cpu].task->pid == pid) {
			p_name = cpu_work_history_view[cnt][cpu].task->comm;

		} else {
			if(search_killed_task(pid, task_name) < 0) {
				snprintf(task_name, sizeof(task_name), "NOT found task");
			}
			p_name = task_name;
		}

		if (ret >= buf_size)
			break;

		ret +=  snprintf(buf + ret, buf_size - ret,
			"[%d] %32pf @ %24pf  [%16s]   %lld ~ %lld %10lld[ns] \n",
			cnt, cpu_work_history_view[cnt][cpu].func,
			cpu_work_history_view[cnt][cpu].work,
			p_name,
			cpu_work_history_view[cnt][cpu].start_time,
			cpu_work_history_view[cnt][cpu].end_time, delta_time );
		cnt++;
	}

	return ret;
}

u64 get_load_analyzer_time(void)
{
	return cpu_clock(UINT_MAX);
}

void __slp_store_work_history(struct work_struct *work, work_func_t func,
		u64 start_time, u64 end_time)
{
	unsigned int cnt, cpu;
	struct task_struct *task;

	if (cpu_work_history_onoff == 0)
		return;

	cpu = raw_smp_processor_id();
	task = current;

	if (++cpu_work_history_cnt[cpu] >= cpu_work_history_num)
		cpu_work_history_cnt[cpu] = 0;
	cnt = cpu_work_history_cnt[cpu];

	cpu_work_history[cnt][cpu].start_time = start_time;
	cpu_work_history[cnt][cpu].end_time = end_time;
//	cpu_work_history[cnt][cpu].occup_time = end_time - start_time;
	cpu_work_history[cnt][cpu].task = task;
	cpu_work_history[cnt][cpu].pid = task->pid;
	cpu_work_history[cnt][cpu].work = work;
	cpu_work_history[cnt][cpu].func = func;
}

int check_work_valid_range(unsigned int cpu, unsigned int start_cnt, unsigned int end_cnt)
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

	cnt = cpu_load_freq_history_view[last_load_cnt].work_history_cnt[cpu];
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

		sched_cnt = cpu_load_freq_history_view[load_cnt].work_history_cnt[cpu];
		load_cnt = get_index(load_cnt, cpu_load_history_num, -1);

		sched_before_cnt = cpu_load_freq_history_view[load_cnt].work_history_cnt[cpu];

		if (sched_before_cnt > sched_cnt)
			upset++;

		end_sched_cnt_margin = get_index(end_sched_cnt, cpu_work_history_num, 1);
		sched_before_cnt_margin = get_index(sched_before_cnt, cpu_work_history_num, -1);

		/* "end_sched_cnt -1" is needed
		  *  because of calulating schedule time */
		if ((upset >= 2) || ((upset == 1)
			&& (sched_before_cnt_margin < end_sched_cnt_margin))) {
			overflow = 1;
			pr_err("[LA] overflow cpu=%d upset=%d sched_before_cnt_margin=%d" \
				"end_sched_cnt_margin=%d end_sched_cnt=%d" \
				"sched_before_cnt=%d sched_cnt=%d load_cnt=%d",
				cpu , upset, sched_before_cnt_margin, end_sched_cnt_margin, end_sched_cnt,
				sched_before_cnt, sched_cnt, load_cnt);
			break;
		}

		if (load_cnt == start_cnt)
			break;
	}

	if (overflow == 0)
		ret = 0;
	else {
		ret = OVERFLOW_ERROR;
		pr_info("[overflow error]\n");
	}

	return ret;
}

static ssize_t check_work_read(struct file *file,
		char __user *buffer, size_t count, loff_t *ppos)
{
	static char *buf = NULL;
	int buf_size = (PAGE_SIZE * 256);
	unsigned int i, ret = 0, size_for_copy = count;
	int ret_check_valid;
	static unsigned int rest_size = 0;

	unsigned int start_cnt = cpu_work_history_show_start_cnt;
	unsigned int end_cnt = cpu_work_history_show_end_cnt;
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
					"    TIME       CPU0_F  CPU1_F   CPU_LOCK"
					"    [INDEX]\tCPU0 \tCPU1 \tONLINE \tNR_RUN\n");

			ret = show_cpu_load_freq_sub((int)end_cnt + 2, 5, buf, buf_size, ret);
			ret += snprintf(buf + ret, buf_size - ret, "\n\n");
		}

		for (i = 0; i < CPU_NUM ; i++) {
			if (cpu_work_history_show_select_cpu != -1)
				if (i != cpu_work_history_show_select_cpu)
					continue;

			cpu = i;
			ret_check_valid = check_work_valid_range(cpu, start_cnt, end_cnt);

			if (ret_check_valid < 0) {
				ret +=  snprintf(buf + ret, buf_size - ret,
						"[ERROR] cpu[%d] Invalid range !!! err=%d\n", cpu, ret_check_valid);
				pr_info("[ERROR] cpu[%d] Invalid range !!! err=%d\n", cpu, ret_check_valid);
				continue;
			}

			clc_work_run_time(i, start_cnt, end_cnt);

			t = total_time;
			msec_rem = do_div(t, 1000000);

			if (end_cnt == start_cnt+1) {
				ret +=  snprintf(buf + ret, buf_size - ret,
						"[%d] TOTAL SECTION TIME = %ld.%ld[ms]\n\n",
						end_cnt, (unsigned long)t, msec_rem);
			} else {
				ret +=  snprintf(buf + ret, buf_size - ret,
						"[%d~%d] TOTAL SECTION TIME = %ld.%ld[ms]\n\n",
						start_cnt+1, end_cnt, (unsigned long)t, msec_rem);
			}

			ret += snprintf(buf + ret, buf_size - ret,
				"######################################"
				" CPU %d ###############################\n", i);

			if (cpu_work_history_show_select_cpu == -1)
				ret = view_workfn_list(buf, buf_size, ret);
			else if (i == cpu_work_history_show_select_cpu)
				ret = view_workfn_list(buf, buf_size, ret);

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

static ssize_t check_work_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	set_cpu_load_freq_history_array_range(user_buf);

	cpu_work_history_show_select_cpu = cpu_task_history_show_select_cpu;
	cpu_work_history_show_start_cnt= cpu_task_history_show_start_cnt;
	cpu_work_history_show_end_cnt = cpu_task_history_show_end_cnt;

	return count;
}


static int check_work_detail_sub(char *buf, int buf_size)
{
	int ret = 0, i = 0, ret_check_valid = 0;
	unsigned int start_cnt = cpu_work_history_show_start_cnt;
	unsigned int end_cnt = cpu_work_history_show_end_cnt;
	unsigned int cpu;

	for (i = 0; i < CPU_NUM; i++) {
		if (cpu_work_history_show_select_cpu != -1)
			if (i != cpu_work_history_show_select_cpu)
				continue;
		cpu = i;
		ret_check_valid = check_work_valid_range(cpu, start_cnt, end_cnt);
		if (ret_check_valid < 0)	{
			ret +=  snprintf(buf + ret, buf_size - ret,
					"[ERROR] cpu[%d] Invalid range !!! err=%d\n", cpu, ret_check_valid);
			pr_info("[ERROR] cpu[%d] Invalid range !!! err=%d\n", cpu, ret_check_valid);
			continue;
		}

		ret += snprintf(buf + ret, buf_size - ret,
			"###########################################"
			"################ CPU %d ######################"
			"##########################################\n", i);

		ret = work_time_list_view(i, start_cnt, end_cnt, buf, buf_size, ret);

		ret += snprintf(buf + ret, buf_size - ret ,"\n\n");
	}

	return ret;
}

static ssize_t check_work_detail(struct file *file,
		char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int size_for_copy;

	size_for_copy = wrapper_for_debug_fs(buffer, count, ppos, check_work_detail_sub);

	return size_for_copy;
}

static const struct file_operations check_work_fops = {
	.owner = THIS_MODULE,
	.read  = check_work_read,
	.write = check_work_write,
};

static const struct file_operations check_work_detail_fops = {
	.owner = THIS_MODULE,
	.read  = check_work_detail,
};

void debugfs_workqueue(struct dentry *d)
{
	if (!debugfs_create_file("check_work", 0600, d, NULL, &check_work_fops))
		pr_err("%s : debugfs_create_file, error\n", "check_work");

	if (!debugfs_create_file("check_work_detail", 0600, d, NULL, &check_work_detail_fops))
		pr_err("%s : debugfs_create_file, error\n", "check_work_detail");
}
