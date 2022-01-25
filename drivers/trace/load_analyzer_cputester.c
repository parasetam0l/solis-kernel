
int cpu_tester_en;

int cpufreq_tester_en;
int cpuidle_tester_en;
int cpuload_tester_en;

int cpu_idle_trace_en;

bool b_cpuload_work_queue_stop;

static struct workqueue_struct *cpu_tester_wq;

static struct workqueue_struct *cpu_load_wq[CPU_NUM];

struct delayed_work cpu_tester_work;
struct delayed_work cpu_idle_tester_work;
struct delayed_work cpu_freq_tester_work;
struct delayed_work cpu_load_tester_work[CPU_NUM];

int cpu_tester_get_cpu_idletest_list_num(void)
{
	return (sizeof(cpu_idletest_list) / sizeof(struct cpu_test_list_tag)) -1;
}

int cpu_tester_get_cpu_freqtest_list_num(void)
{
	return (sizeof(cpu_freqtest_list) / sizeof(struct cpu_test_list_tag)) -1;
}

int cpu_tester_get_cpu_test_freq_table_num(void)
{
	return (sizeof(cpu_test_freq_table) / sizeof(struct cpu_test_freq_table_tag));
}

int cpu_tester_get_cpu_test_idle_table_num(void)
{
	return (sizeof(cpu_test_idle_table) / sizeof(struct cpu_test_idle_table_tag));
}

int cpufreq_force_state = -1;

static int cpufreq_force_set_read_sub(char *buf, int buf_size)
{
	int ret = 0;

	ret +=  snprintf(buf + ret, buf_size - ret, "%d\n", cpufreq_force_state);

	return ret;
}

static ssize_t cpufreq_force_set_read(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int size_for_copy;

	size_for_copy = wrapper_for_debug_fs(buffer, count, ppos,
			cpufreq_force_set_read_sub);

	return size_for_copy;
}

static ssize_t cpufreq_force_set_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct cpufreq_policy *policy;

	if (strstr(user_buf, "-1") != NULL)
		cpufreq_force_state = -1;
	else
		cpufreq_force_state = atoi(user_buf);

	policy = cpufreq_cpu_get(0);
	if (policy != NULL) {
		__cpufreq_driver_target(policy, cpufreq_force_state, CPUFREQ_RELATION_L);
		cpufreq_cpu_put(policy);
	}

	return count;
}

int cpufreq_force_set(int *force_state, int target_freq)
{
	if (cpufreq_force_state == -1) {
		*force_state = target_freq;
	} else {
		*force_state = cpufreq_force_state;
	}

	return 0;
}

static int cpuidle_force_state = -1;

static int cpuidle_force_set_read_sub(char *buf, int buf_size)
{
	int ret = 0;

	ret +=  snprintf(buf + ret, buf_size - ret, "%d\n", cpuidle_force_state);

	return ret;
}

static ssize_t cpuidle_force_set_read(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int size_for_copy;

	size_for_copy = wrapper_for_debug_fs(buffer, count, ppos
					,cpuidle_force_set_read_sub);

	return size_for_copy;
}

static ssize_t cpuidle_force_set_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{

	if (strstr(user_buf, "-1") != NULL)
		cpuidle_force_state = -1;
	else
		cpuidle_force_state = atoi(user_buf);

	return count;
}

int cpuidle_force_set(int *force_state, int next_state)
{

	if (cpuidle_force_state == -1)
		*force_state = next_state;
	else
		*force_state = cpuidle_force_state;

	return 0;
}

void cpu_idle_tester_work_start(struct work_struct *work)
{
	static int cnt = -1;
	int operating_msec;
	int setting_value;

	if (cpuidle_tester_en != 1)
		return;

	if (cpu_idletest_list[++cnt].test_item == END_OF_LIST)
		cnt = 0;

	setting_value = cpu_idletest_list[cnt].setting_value;

	if (setting_value == CPUIDLE_RANDOM)
		setting_value = jiffies % CPUIDLE_RANDOM;

	cpuidle_force_state = setting_value;

	operating_msec = cpu_idletest_list[cnt].test_time;

	cpu_idletest_list[cnt].test_cnt++;

	queue_delayed_work(cpu_tester_wq, &cpu_idle_tester_work,
			msecs_to_jiffies(operating_msec));
}

void cpu_freq_tester_work_start(struct work_struct *work)
{
	static int cnt = -1;
	int operating_msec;
	struct cpufreq_policy *policy;
	int setting_value;

	if (cpufreq_tester_en != 1)
		return;

	if (cpu_freqtest_list[++cnt].test_item == END_OF_LIST)
		cnt = 0;

	setting_value = cpu_freqtest_list[cnt].setting_value;

	if (setting_value == CPUFREQ_RANDOM)
		setting_value = jiffies % CPUFREQ_RANDOM;

	set_cpufreq_force_state(setting_value);

	operating_msec = cpu_freqtest_list[cnt].test_time;

	policy = cpufreq_cpu_get(0);
	if (policy != NULL) {
		__cpufreq_driver_target(policy, cpufreq_force_state, CPUFREQ_RELATION_L);
		cpufreq_cpu_put(policy);
	}

	cpu_freqtest_list[cnt].test_cnt++;

	queue_delayed_work(cpu_tester_wq, &cpu_freq_tester_work,
			msecs_to_jiffies(operating_msec));
}

static int cpuload_working[CPU_NUM];

void cpu_load_tester_work_start(struct work_struct *work)
{
	struct delayed_work *my_delayed_work;
	int num;

	if (cpuload_tester_en > 0) {
		mdelay(200);
		my_delayed_work= to_delayed_work(work);
		num = my_delayed_work -&(cpu_load_tester_work[0]);

		if (cpuload_working[num] == 1) {
			if (cpu_online(num))
				queue_delayed_work_on(num, cpu_load_wq[num], to_delayed_work(work), 0);
			else
				queue_delayed_work(cpu_load_wq[num], to_delayed_work(work), 0);
		}
	}
}

void cpu_tester_work_start(struct work_struct *work)
{
	int i, cpuload_new_off, cpuload_new_on;
	int cpu_freq_table_num, cpu_idle_table_num;
	static int pre_cpuload_tester_en = 0;

	if ((cpufreq_tester_en) && (!delayed_work_pending(&cpu_freq_tester_work))){
		cpu_freq_table_num = cpu_tester_get_cpu_test_freq_table_num();
		for(i =0; i<cpu_freq_table_num; i++) {
			cpu_test_freq_table[i].enter_count = 0;
			cpu_test_freq_table[i].exit_count = 0;
		}
		queue_delayed_work(cpu_tester_wq, &cpu_freq_tester_work, (1)*HZ);
	}

	if ((cpuidle_tester_en) && (!delayed_work_pending(&cpu_idle_tester_work))){
		cpu_idle_table_num = cpu_tester_get_cpu_test_idle_table_num();
		for(i =0; i<cpu_idle_table_num; i++) {
			cpu_test_idle_table[i].enter_count = 0;
			cpu_test_idle_table[i].exit_count = 0;
		}
		queue_delayed_work(cpu_tester_wq, &cpu_idle_tester_work, (1)*HZ);
	}

	cpuload_new_off = pre_cpuload_tester_en - cpuload_tester_en;
	cpuload_new_on  = cpuload_tester_en - pre_cpuload_tester_en;

	pr_info("cpuload_new_off=%d cpuload_new_on=%d\n", cpuload_new_off, cpuload_new_on);

	if (cpuload_new_on > 0){
		for (i = pre_cpuload_tester_en; i < cpuload_tester_en; i ++) {
			cpuload_working[i] = 1;
			if (!delayed_work_pending(&cpu_load_tester_work[i]))
				queue_delayed_work(cpu_load_wq[i], &(cpu_load_tester_work[i]), HZ/100);
		}
	} else if (cpuload_new_off > 0) {
		for (i=1; cpuload_new_off--; i++)
			cpuload_working[pre_cpuload_tester_en-i] = 0;
	}

	pre_cpuload_tester_en = cpuload_tester_en;
}

static int cpu_tester_state_read_sub(char *buf, int buf_size)
{
	int ret = 0, i = 0;
	char cpuidle_state_name[10] = {0, };
	char cpufreq_state_name[10] = {0, };

	int cpu_idletest_list_num, cpu_freqtest_list_num;

	cpu_idletest_list_num = cpu_tester_get_cpu_idletest_list_num();
	cpu_freqtest_list_num = cpu_tester_get_cpu_freqtest_list_num();

	ret +=  snprintf(buf + ret, buf_size - ret,
			"========= CPU FREQ TEST =========\n"
			"   CPUFREQ  TEST_COUNT\n");

	for(i = 0; i < cpu_freqtest_list_num; i++) {
		cpu_tester_enum_to_str(cpufreq_state_name, CPU_FREQ_TEST,
				cpu_freqtest_list[i].setting_value);

		ret +=  snprintf(buf + ret, buf_size - ret,
				"%10s\t%d\n", cpufreq_state_name, cpu_freqtest_list[i].test_cnt);
	}

	ret +=  snprintf(buf + ret, buf_size - ret, "\n");

	ret +=  snprintf(buf + ret, buf_size - ret,
			"========= CPU IDLE TEST =========\n"
			"   CPUIDLE  TEST_COUNT\n");

	for(i = 0; i < cpu_idletest_list_num; i++) {
		cpu_tester_enum_to_str(cpuidle_state_name, CPU_IDLE_TEST,
				cpu_idletest_list[i].setting_value);

		ret += snprintf(buf + ret, buf_size - ret,
				"%10s\t%d\n", cpuidle_state_name, cpu_idletest_list[i].test_cnt);
	}

	ret += snprintf(buf + ret, buf_size - ret, "\n");

	return ret;
}

static ssize_t cpu_tester_state_read(struct file *file,
		char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int size_for_copy;

	size_for_copy = wrapper_for_debug_fs(buffer, count, ppos, cpu_tester_state_read_sub);

	return size_for_copy;
}

static int cpu_test_result_read_sub(char *buf, int buf_size)
{
	int ret = 0, i = 0;
	int cpu_freq_table_num, cpu_idle_table_num;
	char cpuidle_state_name[10] = {0, };
	char cpufreq_state_name[10] = {0, };

	cpu_freq_table_num = cpu_tester_get_cpu_test_freq_table_num();
	cpu_idle_table_num = cpu_tester_get_cpu_test_idle_table_num();

	ret +=  snprintf(buf + ret, buf_size - ret,
			"========= CPU FREQ TEST =========\n"
		  "   CPUFREQ  TEST_COUNT\n");

	for(i = 0; i < cpu_freq_table_num; i++) {
		cpu_tester_enum_to_str(cpufreq_state_name,
				CPU_FREQ_TEST, cpu_test_freq_table[i].cpufreq);

		ret +=  snprintf(buf + ret, buf_size - ret,
				"%10s\t%d\n", cpufreq_state_name, cpu_test_freq_table[i].exit_count);
	}

	ret +=  snprintf(buf + ret, buf_size - ret, "\n");

	ret +=  snprintf(buf + ret, buf_size - ret,
			"========= CPU IDLE TEST =========\n"
		  "   CPUIDLE  TEST_COUNT\n");

	for(i = 0; i < cpu_idle_table_num; i++) {
		cpu_tester_enum_to_str(cpuidle_state_name,
				CPU_IDLE_TEST, cpu_test_idle_table[i].cpuidle);

		ret += snprintf(buf + ret, buf_size - ret,
				"%10s\t%d\n", cpuidle_state_name, cpu_test_idle_table[i].exit_count);
	}

	ret += snprintf(buf + ret, buf_size - ret, "\n");

	return ret;
}

static ssize_t cpu_test_result_read(struct file *file,
		char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int size_for_copy;

	size_for_copy = wrapper_for_debug_fs(buffer, count, ppos, cpu_test_result_read_sub);

	return size_for_copy;
}

static int cpu_tester_en_read_sub(char *buf, int buf_size)
{
	int ret = 0;

	ret +=  snprintf(buf + ret, buf_size - ret,
			"cpufreq:%d cpuidle:%d cpuload:%d\n",
			cpufreq_tester_en, cpuidle_tester_en, cpuload_tester_en);

	return ret;
}

static ssize_t cpu_tester_en_read(struct file *file,
		char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int size_for_copy;

	size_for_copy = wrapper_for_debug_fs(buffer, count, ppos, cpu_tester_en_read_sub);

	return size_for_copy;
}

int cpu_tester_check_onoff(char *str, char *tagname)
{
	char *p_tagstart;
	int tagname_size;
	int onoff = -1;

	tagname_size = strlen(tagname);

	p_tagstart = strstr(str, tagname);

	if (p_tagstart != NULL)
		onoff = atoi(p_tagstart + tagname_size);

	return onoff;
}

static ssize_t cpu_tester_en_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int cpufreq_input, cpuidle_input, cpuload_input, all_input;
	bool b_cpufreq_result = 0, b_cpuidle_result = 0, b_cpu_all_result = 0;

	/* STEP 1 : Get input value */
	cpufreq_input = cpu_tester_check_onoff((char *)user_buf, "cpufreq:");
	cpuidle_input = cpu_tester_check_onoff((char *)user_buf, "cpuidle:");
	cpuload_input = cpu_tester_check_onoff((char *)user_buf, "cpuload:");
	all_input = cpu_tester_check_onoff((char *)user_buf, "all:");

	pr_info("cpufreq_input=%d cpuidle_input=%d cpuload_input=%d all_input=%d\n",
			cpufreq_input, cpuidle_input, cpuload_input, all_input);

	/* STEP 2 : checking need to print result or not*/
	if ((all_input == 0) || (all_input ==1) || (all_input == -1)) {
		if ((cpu_tester_en == 1) && (all_input == 0))
			b_cpu_all_result = 1;

		if ((all_input == 0) || (all_input ==1)) {
			cpufreq_input = all_input;
			cpuidle_input = all_input;
		}
		cpu_tester_en = all_input;
	} else {
		goto fail;
	}

	if ((cpufreq_input == 0) || (cpufreq_input ==1) || (cpufreq_input == -1)) {
		if ((cpufreq_tester_en == 1) && (cpufreq_input == 0))
			b_cpufreq_result = 1;

		if (cpufreq_input == 0)
			cpufreq_force_state =-1;

		cpufreq_tester_en = cpufreq_input;
	} else {
		goto fail;
	}

	if ((cpuidle_input == 0) || (cpuidle_input ==1) || (cpuidle_input == -1)) {
		if ((cpuidle_tester_en == 1) && (cpuidle_input == 0))
			b_cpuidle_result = 1;

		if (cpuidle_input == 0)
			cpuidle_force_state =-1;

		cpuidle_tester_en = cpuidle_input;
	} else {
		goto fail;
	}

	if ((cpuload_input >=  -1) && (cpuload_input <= CPU_NUM)) {
		if ((cpuload_input >= 0) && (cpuload_input <= CPU_NUM))
			cpuload_tester_en = cpuload_input;
	} else {
		pr_err("cpuload is wrong %d", cpuload_input);
		goto fail;
	}

	/* STEP 3 : print result to kernel log */
	if ((b_cpufreq_result == 1) || (b_cpuidle_result == 1) || (b_cpu_all_result == 1)) {
		#define BUF_SIZE (1024 * 1024)
		char *buf;

		buf = vmalloc(BUF_SIZE);
		cpu_test_result_read_sub(buf, BUF_SIZE);
		cpu_print_buf_to_klog(buf);
		vfree(buf);
	}

	/* STEP 4 : cpu tester workqueue start  */
	cpu_tester_work_start(NULL);

fail:

	return count;
}

static const struct file_operations cpuidle_force_state_fops = {
	.owner = THIS_MODULE,
	.read  = cpuidle_force_set_read,
	.write = cpuidle_force_set_write,
};

static const struct file_operations cpufreq_force_state_fops = {
	.owner = THIS_MODULE,
	.read  = cpufreq_force_set_read,
	.write = cpufreq_force_set_write,
};

static const struct file_operations cpu_test_result_fops = {
	.owner = THIS_MODULE,
	.read  = cpu_test_result_read,
};

static const struct file_operations cpu_tester_state_fops = {
	.owner = THIS_MODULE,
	.read  = cpu_tester_state_read,
};

static const struct file_operations cpu_tester_en_fops = {
	.owner = THIS_MODULE,
	.read  = cpu_tester_en_read,
	.write = cpu_tester_en_write,
};

static int cpu_idle_test_read_sub(char *buf, int buf_size)
{
	int ret = 0, i = 0;
	char cpuidle_state_name[10] = {0, };
	int cpu_idle_table_num;

	cpu_idle_table_num = cpu_tester_get_cpu_test_idle_table_num();

	ret +=  snprintf(buf + ret, buf_size - ret,
			"========= CPU IDLE STATUS COUNT =========\n"
		  "   CPUIDLE  COUNT\n");

	for(i = 0; i < cpu_idle_table_num; i++) {
		cpu_tester_enum_to_str(cpuidle_state_name,
				CPU_IDLE_TEST, cpu_test_idle_table[i].cpuidle);

		ret += snprintf(buf + ret, buf_size - ret,
				"%10s\t%d\n", cpuidle_state_name, cpu_test_idle_table[i].exit_count);
	}

	ret += snprintf(buf + ret, buf_size - ret, "\n");

	return ret;
}

static ssize_t cpu_idle_trace_en_read(struct file *file,
		char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int size_for_copy;

	size_for_copy = wrapper_for_debug_fs(buffer, count, ppos, cpu_idle_test_read_sub);

	return size_for_copy;
}

static ssize_t cpu_idle_trace_en_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	bool input;
	int i;
	int cpu_idle_table_num;

	input = (atoi(user_buf) > 0);

	if (input && !cpu_idle_trace_en) {
		cpu_idle_table_num = cpu_tester_get_cpu_test_idle_table_num();

		for (i = 0; i < cpu_idle_table_num; ++i) {
			cpu_test_idle_table[i].enter_count = 0;
			cpu_test_idle_table[i].exit_count = 0;
		}
	}

	cpu_idle_trace_en = input;

	return count;
}

static const struct file_operations cpu_trace_idle_fops = {
	.owner = THIS_MODULE,
	.read  = cpu_idle_trace_en_read,
	.write = cpu_idle_trace_en_write,
};

void debugfs_cpu_tester(struct dentry *d)
{
	if (!debugfs_create_file("cpuidle_force_state", 0600, d, NULL, &cpuidle_force_state_fops))
		pr_err("%s : debugfs_create_file, error\n", "cpuidle_force_state");

	if (!debugfs_create_file("cpufreq_force_state", 0600, d, NULL, &cpufreq_force_state_fops))
		pr_err("%s : debugfs_create_file, error\n", "cpufreq_force_state");

	if (!debugfs_create_file("cpu_tester_state", 0600, d, NULL, &cpu_tester_state_fops))
		pr_err("%s : debugfs_create_file, error\n", "cpu_tester_state");

	if (!debugfs_create_file("test_result", 0600, d, NULL, &cpu_test_result_fops))
		pr_err("%s : debugfs_create_file, error\n", "cpu_test_result");

	if (!debugfs_create_file("cpu_tester_en", 0600, d, NULL, &cpu_tester_en_fops))
		pr_err("%s : debugfs_create_file, error\n", "cpu_tester_en");

	if (!debugfs_create_file("cpu_idle_trace_en", 0600, d, NULL, &cpu_trace_idle_fops))
		pr_err("%s : debugfs_create_file, error\n", "cpu_idle_trace_en");
}
