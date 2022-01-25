
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/trm.h>

extern struct list_head input_get_input_dev_list(void);

int input_rec_store_input_dev_info(void)
{
	int i = 0, cnt=0;
	struct input_dev *dev, *head_dev = NULL;
	struct list_head input_dev_head;

	struct list_head *p, *n;
	input_dev_head = input_get_input_dev_list();

	list_for_each_safe(p, n, &input_dev_head) {
		dev = list_entry(p, struct input_dev, node);

		if (head_dev == dev)
			break;

		if (i == 0)
			head_dev = dev;

		if (i >= (MAX_INPUT_DEVICES - 10))
			break;

		input_dev_info_current[i].dev = dev;
		strncpy(input_dev_info_current[i].name, dev->name, MAX_INPUT_DEV_NAME_LEN-1);
		i++;
	}

	input_dev_info_current[i].dev = &touch_booster;
	strncpy(input_dev_info_current[i].name, touch_booster.name, MAX_INPUT_DEV_NAME_LEN-1);
	i++;

	input_dev_info_current[i].dev = &rotary_booster;
	strncpy(input_dev_info_current[i].name, rotary_booster.name, MAX_INPUT_DEV_NAME_LEN-1);
	i++;

	input_dev_info_current_num = i;

	cnt = i;

	for (i = 0; i < cnt; i++) {
		pr_info("Current [%d]dev=%p, name=%s\n",
				i, input_dev_info_current[i].dev, input_dev_info_current[i].name);
	}

	return 0;
}

int input_rec_save_data(void)
{
	//int save_type;
	char file_path[100] = {0,};
	void *data = NULL;
	int size = 0;

	strcpy(file_path, "/opt/usr/media/input_history.dat");
	data = input_rec_history_view;
	size = INPUT_REC_HISTORY_SIZE;
	save_data_to_file(file_path, data, size);

	input_rec_store_input_dev_info();

	strcpy(file_path, "/opt/usr/media/input_info.dat");
	data = input_dev_info_current;
	size = sizeof(struct input_dev_info_tag) * input_dev_info_current_num;
	save_data_to_file(file_path, data, size);

	return size;
}

int input_rec_load_data(void)
{
	//int save_type;
	char file_path[100] = {0,};
	void *data = NULL;
	int size = 0, i, read_size = 0;

	strcpy(file_path, "/opt/usr/media/input_history.dat");
	data = input_rec_history_view;
	size = INPUT_REC_HISTORY_SIZE;
	read_size = load_data_from_file(data, file_path, size);

	input_rec_store_input_dev_info();

	strcpy(file_path, "/opt/usr/media/input_info.dat");
	data = input_dev_info_saved;
	size = sizeof(struct input_dev_info_tag) * MAX_INPUT_DEVICES;

	input_dev_info_saved_num = load_data_from_file(data, file_path, size) /sizeof(struct input_dev_info_tag);

	for (i=0; i<input_dev_info_saved_num; i++) {
		pr_info("Load [%d]dev=%p, name=%s\n", i
				, input_dev_info_saved[i].dev, input_dev_info_saved[i].name);
	}

	if ((read_size > 0) && (input_dev_info_saved_num > 0)) {
		return 0;
	} else {
		pr_err("read_size=%d input_dev_info_saved_num=%d", read_size, input_dev_info_saved_num);
		return -1;
	}
}

static unsigned int  input_rec_time_list_view(unsigned int start_cnt, unsigned int end_cnt,
		char *buf, unsigned int buf_size, unsigned int ret)
{
	unsigned int i = 0, start_array_num, data_line, cnt=0;
	unsigned int end_array_num, start_array_num_for_time;

	start_array_num_for_time = cpu_load_freq_history_view[start_cnt].input_rec_history_cnt;
	start_array_num = (cpu_load_freq_history_view[start_cnt].input_rec_history_cnt + 1) % input_rec_history_num;
	end_array_num   = cpu_load_freq_history_view[end_cnt].input_rec_history_cnt;

	total_time = section_end_time - section_start_time;

	if (end_cnt == start_cnt+1) {
		ret +=  snprintf(buf + ret, buf_size - ret,
			"[%d] TOTAL SECTION TIME = %lld[ns]\n[%5d]~[%5d]/(%lld ~ %lld)\n\n",
			end_cnt, total_time,
			(cpu_load_freq_history_view[start_cnt].input_rec_history_cnt + 1) % input_rec_history_num,
			(cpu_load_freq_history_view[end_cnt].input_rec_history_cnt + 1) % input_rec_history_num,
			cpu_load_freq_history_view[start_cnt].time_stamp,
			cpu_load_freq_history_view[end_cnt].time_stamp);
	} else {
		ret +=  snprintf(buf + ret, buf_size - ret,
			"[%d~%d] TOTAL SECTION TIME = %lld[ns]\n[%5d]~[%5d]/(%lld ~ %lld)\n\n",
			get_index(start_cnt, input_rec_history_num, 1), end_cnt, total_time,
			(cpu_load_freq_history_view[start_cnt].input_rec_history_cnt + 1) % input_rec_history_num,
			(cpu_load_freq_history_view[end_cnt].input_rec_history_cnt + 1) % input_rec_history_num,
			cpu_load_freq_history_view[start_cnt].time_stamp,
			cpu_load_freq_history_view[end_cnt].time_stamp);
	}

	end_array_num = get_index(end_array_num, input_rec_history_num, 2);

	if (end_array_num >= start_array_num_for_time)
		data_line = end_array_num -start_array_num_for_time + 1;
	else
		data_line = (end_array_num + input_rec_history_num) - start_array_num_for_time + 1;

	cnt = start_array_num_for_time;

	for (i = 0; i < data_line; i++) {
		char *name;
		struct input_dev *dev;

		if (cnt > input_rec_history_num-1)
			cnt = 0;

		if (ret >= buf_size)
			break;

		if (b_input_load_data == 0) {
			name = (char *)input_rec_history_view[cnt].dev->name;
		} else {
			dev = input_rec_change_dev(input_rec_history_view[cnt].dev);
			name = (char *)dev->name;
		}

		ret +=  snprintf(buf + ret, buf_size - ret,
			"[%d] %lld %s [TYPE]%d [CODE]%d [value]%X\n", cnt,
			input_rec_history_view[cnt].time, name, input_rec_history_view[cnt].type,
			input_rec_history_view[cnt].code, input_rec_history_view[cnt].value);
		cnt++;
	}

	return ret;
}

void __slp_store_input_history(void *dev,
		unsigned int type, unsigned int code, int value)
{
	unsigned int cnt ;
	struct input_dev *idev = dev;

	if (input_rec_history_onoff == 0)
		return ;

	if (idev->name != NULL) {
		if (strstr(idev->name, "accelerometer") != NULL)
			return;
	} else {
		return;
	}

	if (++input_rec_history_cnt >= input_rec_history_num)
		input_rec_history_cnt = 0;
	cnt = input_rec_history_cnt;

	input_rec_history[cnt].time = cpu_clock(UINT_MAX);
	input_rec_history[cnt].dev = dev;
	input_rec_history[cnt].type = type;
	input_rec_history[cnt].code = code;
	input_rec_history[cnt].value = value;
}

int check_input_rec_valid_range(unsigned int start_idx, unsigned int end_idx)
{
	int ret = 0;
	unsigned long long t1, t2;
	unsigned int end_sched_cnt = 0, end_sched_cnt_margin;
	unsigned int load_cnt, last_load_cnt, overflow = 0;
	unsigned int cnt, search_cnt;
	unsigned int upset = 0;
	int cpu = 0;
	unsigned int i;

	t1 = cpu_load_freq_history_view[start_idx].time_stamp;
	t2 = cpu_load_freq_history_view[end_idx].time_stamp;

	if ((t2 <= t1) || (t1 == 0) || (t2 == 0)) {
		pr_info("[time error] t1=%lld t2=%lld\n", t1, t2);
		return WRONG_TIME_STAMP;
	}

	last_load_cnt = cpu_load_freq_history_view_cnt;

	cnt = cpu_load_freq_history_view[last_load_cnt].input_rec_history_cnt;
	t1 = input_rec_history_view[cnt].time;
	search_cnt = cnt;

	for (i = 0;  i < input_rec_history_num; i++) {
		search_cnt = get_index(search_cnt, input_rec_history_num, 1);
		t2 = input_rec_history_view[search_cnt].time;

		if (t2 < t1) {
			end_sched_cnt = search_cnt;
			break;
		}

		if (i >= input_rec_history_num - 1)
			end_sched_cnt = cnt;
	}

	load_cnt = last_load_cnt;

	for (i = 0;  i < cpu_load_history_num; i++) {
		unsigned int sched_cnt, sched_before_cnt;
		unsigned int sched_before_cnt_margin;

		sched_cnt = cpu_load_freq_history_view[load_cnt].input_rec_history_cnt;
		load_cnt  = get_index(load_cnt, cpu_load_history_num, -1);

		sched_before_cnt = cpu_load_freq_history_view[load_cnt].input_rec_history_cnt;

		if (sched_before_cnt > sched_cnt)
			upset++;

		end_sched_cnt_margin = get_index(end_sched_cnt, input_rec_history_num, 1);
		sched_before_cnt_margin = get_index(sched_before_cnt, input_rec_history_num, -1);

		/* "end_sched_cnt -1" is needed
		  *  because of calulating schedule time */
		if ((upset >= 2) || ((upset == 1)
			&& (sched_before_cnt_margin < end_sched_cnt_margin))) {
			overflow = 1;
			pr_err("[LA] overflow cpu=%d upset=%d sched_before_cnt_margin=%d" \
				"end_sched_cnt_margin=%d end_sched_cnt=%d" \
				"sched_before_cnt=%d sched_cnt=%d load_cnt=%d" \
				, cpu , upset, sched_before_cnt_margin
				, end_sched_cnt_margin, end_sched_cnt
				, sched_before_cnt, sched_cnt, load_cnt);
			break;
		}

		if (load_cnt == start_idx)
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

static ssize_t check_input_rec_read(struct file *file,
		char __user *buffer, size_t count, loff_t *ppos)
{
	return 0;
}

static ssize_t check_input_rec_write(struct file *file,
		const char __user *user_buf, size_t count,
		loff_t *ppos)
{
	set_cpu_load_freq_history_array_range(user_buf);

	input_rec_history_show_select_cpu =cpu_task_history_show_select_cpu;
	input_rec_history_show_start_cnt=cpu_task_history_show_start_cnt;
	input_rec_history_show_end_cnt = cpu_task_history_show_end_cnt;

	return count;
}

static int check_input_rec_detail_sub(char *buf, int buf_size)
{
	int ret = 0, i = 0, ret_check_valid = 0;
	unsigned int start_idx = input_rec_history_show_start_cnt;
	unsigned int end_idx = input_rec_history_show_end_cnt;

	ret_check_valid = check_input_rec_valid_range(start_idx, end_idx);
	if (ret_check_valid < 0)	{
		ret +=  snprintf(buf + ret, buf_size - ret,
				"[ERROR] Invalid range !!! err=%d\n", ret_check_valid);
		pr_info("[ERROR] Invalid range !!! err=%d\n", ret_check_valid);
	}

	ret += snprintf(buf + ret, buf_size - ret,
		"###########################################"
		"################ CPU %d ######################"
		"##########################################\n", i);

	ret = input_rec_time_list_view(start_idx, end_idx, buf, buf_size, ret);

	ret += snprintf(buf + ret, buf_size - ret ,"\n\n");

	return ret;
}

static ssize_t check_input_rec_detail(struct file *file,
		char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int size_for_copy;

	size_for_copy = wrapper_for_debug_fs(buffer, count, ppos,check_input_rec_detail_sub);

	return size_for_copy;
}


void input_rec_index_to_cnt(int *start_cnt, int *end_cnt, int start_index, int end_index)
{
	*start_cnt = get_index(cpu_load_freq_history_view[start_index].input_rec_history_cnt,
			input_rec_history_num, 1);

	*end_cnt = cpu_load_freq_history_view[end_index].input_rec_history_cnt;

	pr_info("*start_cnt=%d *end_cnt=%d\n", *start_cnt, *end_cnt);
}

void input_rec_str_to_index(int *start_index, int *end_index, char *str)
{
	int show_array_num = 0;
	char *p1;
	char cpy_buf[80] = {0,};

	p1 = strstr(str, "-");

	if (p1 != NULL) {
		strncpy(cpy_buf, str, sizeof(cpy_buf) - 1);
		*p1 = '\0';
		*start_index = get_index(atoi(cpy_buf) ,cpu_load_history_num ,-1);
		*end_index = atoi(p1+1);
	} else {
		show_array_num = atoi(str);
		*start_index = get_index(show_array_num, cpu_load_history_num, -1);
		*end_index = show_array_num;
	}

	pr_info("*start_index=%d *end_index=%d\n", *start_index, *end_index);
}

struct input_dev *input_rec_change_dev(struct input_dev *old_dev)
{
	int i, old_dev_index, new_dev_index;

	/* STEP 1: find old_dev's name */
	for(i =0 ; i < input_dev_info_saved_num; i++) {
		//pr_info("[%d]old_dev=%p input_dev_info_saved[i].dev=%p\n", i, old_dev, input_dev_info_saved[i].dev);
		if (input_dev_info_saved[i].dev == old_dev)
			break;
	}
	if (i >= input_dev_info_saved_num)
		goto fail;

	old_dev_index = i;
	//pr_info("old_dev_index=%d\n", old_dev_index);

	/* STEP 2: find new_dev's index from old_dev's name*/
	for (i = 0 ; i < input_dev_info_current_num; i++) {
		if (strcmp(input_dev_info_current[i].name, input_dev_info_saved[old_dev_index].name) == 0) {
			break;
		}
	}

	if (i >= input_dev_info_current_num)
		goto fail;

	new_dev_index = i;
	//pr_info("new_dev_index=%d\n", new_dev_index);

	//pr_info("old name=%s new name=%s\n"
	//	, input_dev_info_saved[old_dev_index].name, input_dev_info_saved[new_dev_index].name);

	return input_dev_info_current[new_dev_index].dev;

fail:
	pr_err("%s fail", __FUNCTION__);
	return NULL;
}

void input_rec_reproduce_exec(int usec, struct input_dev *dev, unsigned int type,
		unsigned int code, int value)
{
	if (b_input_load_data == 1) {
		dev = input_rec_change_dev(dev);
	}

	/* If interval time is shorter than 50us, don't have sleep time.
	    usleep_range that takes time than over 50us */
	if (usec > 50) {
		usleep_range(usec - 50, usec - 50);
	}

	if (dev == NULL)
		return;

#if 0// defined(CONFIG_SLP_MINI_TRACER)
	{
		char str[128]={0,};
		sprintf(str, "usec=%d name=%s type=%d code=%d value=%d\n"
				,usec, dev->name, type, code, value);
		kernel_mini_tracer_smp(str);
	}
#endif

	if (strcmp(dev->name, touch_booster_name) == 0) {
		//pr_info("TOUCH BOOSER type=%d", type);
		switch (type) {

		case TOUCH_BOOSTER_PRESS:
			touch_booster_press();
			break;
		case TOUCH_BOOSTER_RELEASE:
			touch_booster_release();
			break;
		case TOUCH_BOOSTER_RELEASE_ALL:
			touch_booster_release_all();
			break;
		}
	} else if (strcmp(dev->name, rotary_booster_name) == 0) {
		switch(type) {
			case ROTORY_BOOSTER_TURN:
				rotary_booster_turn_on();
				break;
		}
	} else {
		input_event(dev, type, code, value);
	}
}

static ssize_t input_rec_reproduce(struct file *file,
		const char __user *user_buf, size_t count,
		loff_t *ppos)
{
	int start_cnt, end_cnt, cnt, before_cnt;
	int start_index, end_index;
	int data_num, i;
	u64 time;
	int ret_check_valid;

	input_rec_str_to_index(&start_index, &end_index, (char *)user_buf);

	input_rec_index_to_cnt(&start_cnt, &end_cnt, start_index, end_index);

	if (end_cnt >= start_cnt)
		data_num = end_cnt - start_cnt + 1;
	else
		data_num = end_cnt + input_rec_history_num - start_cnt + 1;

	//pr_info("data_num=%d", data_num);

	ret_check_valid = check_input_rec_valid_range(start_index, end_index);
	if (ret_check_valid < 0)	{
		pr_info("[ERROR] Invalid range !!! err=%d\n", ret_check_valid);
		return count;
	}

	cnt = start_cnt;

	for (i = 0; i < data_num; i++) {
		before_cnt = get_index(cnt, input_rec_history_num, -1);

#if 0 //defined(CONFIG_SLP_MINI_TRACER)
		{
			char str[64]={0,};
			sprintf(str, "cnt=%d before_cnt=%d\n", cnt, before_cnt);
			kernel_mini_tracer_smp(str);
		}
#endif
		time = input_rec_history_view[cnt].time -input_rec_history_view[before_cnt].time;
		do_div(time , 1000);

		input_rec_reproduce_exec((int)time, input_rec_history_view[cnt].dev,
				input_rec_history_view[cnt].type, input_rec_history_view[cnt].code,
				input_rec_history_view[cnt].value);

		cnt = get_index(cnt, input_rec_history_num, 1);
	}

	return count;
}

static const struct file_operations check_input_rec_fops = {
	.owner = THIS_MODULE,
	.read  = check_input_rec_read,
	.write = check_input_rec_write,
};

static const struct file_operations check_input_rec_detail_fops = {
	.owner = THIS_MODULE,
	.read  = check_input_rec_detail,
};

static const struct file_operations input_rec_reproduce_fops = {
	.owner = THIS_MODULE,
	.write = input_rec_reproduce,
};

void debugfs_input_rec(struct dentry *d)
{
	if (!debugfs_create_file("check_input_rec", 0600, d, NULL, &check_input_rec_fops))
		pr_err("%s: Failed to create debugfs 'check_input_rec'\n", __func__);

	if (!debugfs_create_file("check_input_rec_detail", 0600, d, NULL, &check_input_rec_detail_fops))
		pr_err("%s: Failed to create debugfs 'check_input_rec_detail'\n", __func__);

	if (!debugfs_create_file("input_rec_reproduce", 0600, d, NULL, &input_rec_reproduce_fops))
		pr_err("%s: Failed to create debugfs 'input_rec_reproduce'\n", __func__);
}
