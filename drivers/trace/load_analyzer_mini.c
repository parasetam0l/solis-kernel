
#include <asm/cacheflush.h>

/*
 * TIZEN kernel trace queue system
*/
#if defined(CONFIG_SLP_KERNEL_ENG) || defined (CONFIG_TIZEN_SEC_KERNEL_ENG)
#define KENEL_TRACER_QUEUE_SIZE (128 * 1024)
#else
#define KENEL_TRACER_QUEUE_SIZE (1 * 1024)
#endif

#define MINI_TRACER_MAX_SIZE_STRING 512
atomic_t kenel_tracer_queue_cnt = ATOMIC_INIT(0);
static bool kernel_mini_trace_enable;
static char *kernel_tracer_queue;
int kernel_mini_tracer_i2c_log_on;

static char * kernel_mini_tracer_get_addr(void)
{
	unsigned int cnt;
	cnt = atomic_read(&kenel_tracer_queue_cnt);

	return (kernel_tracer_queue + cnt);
}

static void kernel_mini_tracer_char(char character)
{
	unsigned int cnt;

	atomic_inc(&kenel_tracer_queue_cnt);
	cnt = atomic_read(&kenel_tracer_queue_cnt);

	if (cnt >= KENEL_TRACER_QUEUE_SIZE) {
		atomic_set(&kenel_tracer_queue_cnt, 0);
		cnt = 0;
	}

	kernel_tracer_queue[cnt] = character;
}

static void kenel_mini_tracer_get_time(char *time_str)
{
	unsigned long long t;
	unsigned long  nanosec_rem;

	t = cpu_clock(UINT_MAX);
	nanosec_rem = do_div(t, 1000000000);

	sprintf(time_str, "%2lu.%04lu", (unsigned long) t, nanosec_rem / 100000);
}

static int kernel_mini_tracer_saving_str(char *input_string)
{
	unsigned int  max_size_of_tracer = 1024;
	unsigned int tracer_cnt = 0;

	while ((*input_string) != 0 &&  tracer_cnt < max_size_of_tracer) {
		kernel_mini_tracer_char(*(input_string++));
		tracer_cnt++;
	}

	return tracer_cnt;
}

void kernel_mini_tracer(char *input_string, int option)
{
	char time_str[32];
	char *start_addr, *end_addr;
	int saved_char_cnt = 0;

	if (kernel_mini_trace_enable == 0)
		return;

	start_addr = kernel_mini_tracer_get_addr() +1;

	if ((option & TIME_ON) == TIME_ON) {
		saved_char_cnt += kernel_mini_tracer_saving_str("[");
		kenel_mini_tracer_get_time(time_str);
		saved_char_cnt += kernel_mini_tracer_saving_str(time_str);
		saved_char_cnt += kernel_mini_tracer_saving_str("] ");
	}

	saved_char_cnt += kernel_mini_tracer_saving_str(input_string);

	end_addr = kernel_mini_tracer_get_addr();

#if 0 /* Not support in arm64 */
	if ((option & FLUSH_CACHE) == FLUSH_CACHE) {
		if (end_addr > start_addr)
			clean_dcache_area(start_addr, saved_char_cnt);
		else
			clean_dcache_area(kernel_tracer_queue, KENEL_TRACER_QUEUE_SIZE);
	}
#endif
}

void kernel_mini_tracer_smp(char *input_string)
{
	unsigned int this_cpu;

	this_cpu = raw_smp_processor_id();

	if (this_cpu == 0)
		kernel_mini_tracer("[C0] ", TIME_ON | FLUSH_CACHE);
	else if (this_cpu == 1)
		kernel_mini_tracer("[C1] ", TIME_ON | FLUSH_CACHE);
	else if (this_cpu == 2)
		kernel_mini_tracer("[C2] ", TIME_ON | FLUSH_CACHE);
	else if (this_cpu == 3)
		kernel_mini_tracer("[C3] ", TIME_ON | FLUSH_CACHE);

	kernel_mini_tracer(input_string, FLUSH_CACHE);

}

static int kernel_mini_tracer_init(void)
{
	kernel_tracer_queue = (char *)__get_free_pages(GFP_KERNEL
					, get_order(KENEL_TRACER_QUEUE_SIZE));
	if (kernel_tracer_queue == NULL)
		return -ENOMEM;

	memset(kernel_tracer_queue, 0, KENEL_TRACER_QUEUE_SIZE);
	kernel_mini_trace_enable = 1;

	return 0;
}

static int kernel_mini_tracer_exit(void)
{
	kernel_mini_trace_enable = 0;
	if (kernel_tracer_queue != NULL)  {
		__free_pages(virt_to_page(kernel_tracer_queue),
				get_order(KENEL_TRACER_QUEUE_SIZE));
	}

	return 0;
}

static int mini_tracer_view_read_sub(char *buf, int buf_size)
{
	int ret = 0, i, cnt = 0;
	char *p_copied_mem;


	p_copied_mem = vmalloc(KENEL_TRACER_QUEUE_SIZE);

	memcpy(p_copied_mem, kernel_tracer_queue, KENEL_TRACER_QUEUE_SIZE);

	cnt = atomic_read(&kenel_tracer_queue_cnt);

	cnt = cnt + 1;

	if (cnt >= KENEL_TRACER_QUEUE_SIZE)
		cnt = 0;

	for (i=cnt; i< KENEL_TRACER_QUEUE_SIZE; i++)
		ret +=  snprintf(buf + ret, buf_size - ret, "%c", p_copied_mem[i]);

	for (i=0; i< cnt; i++)
		ret +=  snprintf(buf + ret, buf_size - ret, "%c", p_copied_mem[i]);

	vfree(p_copied_mem);

	return ret;
}

static ssize_t mini_tracer_view_read(struct file *file,
		char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int size_for_copy;

	size_for_copy = wrapper_for_debug_fs(buffer, count, ppos,
				mini_tracer_view_read_sub);

	return size_for_copy;
}

static const struct file_operations mini_tracer_view_fops = {
	.owner = THIS_MODULE,
	.read  = mini_tracer_view_read,
};

void debugfs_mini(struct dentry *d)
{
	if (!debugfs_create_file("mini_tracer_view", 0200, d, NULL, &mini_tracer_view_fops))
		pr_err("%s : debugfs_create_file, error\n", "mini_tracer_view");
}
