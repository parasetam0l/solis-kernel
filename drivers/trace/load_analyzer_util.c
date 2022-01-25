
static int atoi(const char *str)
{
	int result = 0;
	int count = 0;

	if (str == NULL)
		return -1;

	while (str[count] && str[count] >= '0' && str[count] <= '9') {
		result = result * 10 + str[count] - '0';
		++count;
	}

	return result;
}

static int get_index(int cnt, int ring_size, int diff)
{
	int ret = 0, modified_diff;

	if ((diff > ring_size) || (diff * (-1) > ring_size))
		modified_diff = diff % ring_size;
	else
		modified_diff = diff;

	ret = (ring_size + cnt + modified_diff) % ring_size;

	return ret;
}

static int wrapper_for_debug_fs(char __user *buffer,
		size_t count, loff_t *ppos, int (*fn)(char *, int))
{
	static char *buf = NULL;
	int buf_size = (PAGE_SIZE * 512);
	unsigned int ret = 0, size_for_copy = count;
	static unsigned int rest_size = 0;

	if (*ppos < 0 || !count)
		return -EINVAL;

	if (*ppos == 0) {
		buf = vmalloc(buf_size);

		if (!buf)
			return -ENOMEM;

		ret = fn(buf, buf_size - PAGE_SIZE); /* PAGE_SIZE mean margin */

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

struct task_info_tag {
	struct task_struct *p_task;
	char comm[TASK_COMM_LEN];
	int pid;
};

int get_name_from_pid(char *task_name, int pid)
{
	int ret = 0;
	int found = 0;
	struct task_struct *p_task;
	static struct task_info_tag last_task_info = {NULL, };

	if(pid == last_task_info.pid) {
		strcpy(task_name, last_task_info.comm);
		ret = 0;
		goto end;
	}

	rcu_read_lock();
	p_task = find_task_by_vpid(pid);
	if (p_task) {
			strcpy(task_name, p_task->comm);
			pr_info("pid %d = %s\n", pid, task_name);
			found = 1;
			last_task_info.p_task = p_task;
			last_task_info.pid =p_task->pid;
			strcpy(last_task_info.comm, p_task->comm);
	}
	rcu_read_unlock();

	if (found == 0) {
		if(search_killed_task(pid, task_name) >= 0) {
			found = 1;
			last_task_info.pid = pid;
			pr_info("killed pid %d = %s\n", pid, task_name);
			strcpy(last_task_info.comm, task_name);
		} else {
			sprintf(task_name, "NOT found %d", pid);
			pr_info("NOT found pid %d\n", pid);
		}
	}

	if (found == 1)
		ret = 0;
	else
		ret = -1;

end:
	return ret;
}

int save_data_to_file(char *path, void *data, int size) {

	struct file *fp;
	mm_segment_t old_fs;
	int written_size;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(path, O_RDWR | O_CREAT,S_IRWXU | S_IRWXG | S_IRWXO);
	if (IS_ERR(fp)) {
		pr_err("%s %s fail to open file to save\n", __func__, path);
		set_fs(old_fs);
		return -1;
	}

	written_size = vfs_write(fp, (const char __user *)data, size, &fp->f_pos);

	filp_close(fp, current->files);
	set_fs(old_fs);

	return written_size;
}

int load_data_from_file(void *data, char *path, int size) {

	struct file *fp;
	mm_segment_t old_fs;
	int read_size;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(path, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		pr_err("%s %s fail to open file to save\n", __func__, path);
		set_fs(old_fs);
		return -1;
	}

	read_size = vfs_read(fp, (char __user *)data, size, &fp->f_pos);

	filp_close(fp, current->files);
	set_fs(old_fs);

	return read_size;
}

struct saved_load_factor_tag saved_load_factor;

void store_external_load_factor(int type, unsigned int data)
{
	switch (type) {

	case ACTIVE_APP_PID:
		saved_load_factor.active_app_pid = data;
		break;
	case NR_RUNNING_TASK:
		saved_load_factor.nr_running_task = data;
		break;
	case MIF_BUS_FREQ:
		saved_load_factor.mif_bus_freq = data;
		break;
	case MIF_BUS_LOAD:
		saved_load_factor.mif_bus_load = data;
		break;
	case INT_BUS_FREQ:
		saved_load_factor.int_bus_freq = data;
		break;
	case INT_BUS_LOAD:
		saved_load_factor.int_bus_load = data;
		break;
	case GPU_FREQ:
		saved_load_factor.gpu_freq = data;
		break;
	case GPU_UTILIZATION:
		saved_load_factor.gpu_utilization = data;
		break;
	case BATTERY_SOC:
		saved_load_factor.battery_soc = data;
		break;
	case LCD_BRIGHTNESS:
		saved_load_factor.lcd_brightness = data;
		break;
	case SUSPEND_STATE:
		saved_load_factor.suspend_state = data;
		break;
	case SUSPEND_COUNT:
		saved_load_factor.suspend_count = data;
		break;
#ifdef CONFIG_SLP_CHECK_RESOURCE
	case CONN_BT_ENABLED:
		saved_load_factor.bt_enabled = data;
		break;
	case CONN_BT_TX:
		saved_load_factor.bt_tx_bytes += data;
		break;
	case CONN_BT_RX:
		saved_load_factor.bt_rx_bytes += data;
		break;
	case CONN_WIFI_ENABLED:
		saved_load_factor.wifi_enabled = data;
		break;
	case CONN_WIFI_TX:
		saved_load_factor.wifi_tx_bytes += data;
		break;
	case CONN_WIFI_RX:
		saved_load_factor.wifi_rx_bytes += data;
		break;
#endif
	default:
		break;
	}
}
