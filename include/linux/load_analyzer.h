
#ifndef _LINUX_SYSTEM_LOAD_ANALYZER_H
#define _LINUX_SYSTEM_LOAD_ANALYZER_H

#define CPU_NUM	NR_CPUS

#if defined(CONFIG_SLUGGISH_ANALYZER)
#include <linux/sluggish_analyzer.h>
#endif

/*** FUNCTION OPTION ***/
#define CPU_LOAD_HISTORY_NUM_MAX	2000
#define CPU_LOAD_HISTORY_NUM_MIN	300

#define CPU_TASK_HISTORY_NUM_MAX	30000
#define CPU_TASK_HISTORY_NUM_MIN	30


#define CONFIG_SLP_CHECK_BUS_LOAD	1
#define CONFIG_SLP_MINI_TRACER		1

#define CONFIG_SLP_CPU_TESTER		1

#if 0
#define CONFIG_SLP_BUS_CLK_CHECK_LOAD	1
#endif

#define CONFIG_SLP_CHECK_RESOURCE	1

#define CONFIG_LOAD_ANALYZER_PMQOS	1


#if 0
#define CONFIG_CHECK_WORK_HISTORY	1
#if defined(CONFIG_SLP_KERNEL_ENG) || defined (CONFIG_TIZEN_SEC_KERNEL_ENG)
#define CPU_WORK_HISTORY_NUM	1000
#else
#define CPU_WORK_HISTORY_NUM	30
#endif

#if defined(CONFIG_SLP_KERNEL_ENG) || defined (CONFIG_TIZEN_SEC_KERNEL_ENG)
#define CONFIG_SLP_CURRENT_MONITOR	1
#endif
#define CONFIG_SLP_CHECK_PHY_ADDR	1
#define CONFIG_CHECK_NOT_CPUIDLE_CAUSE	1
#define CONFIG_SLP_INPUT_REC		1

#if defined(CONFIG_SLP_KERNEL_ENG) || defined (CONFIG_TIZEN_SEC_KERNEL_ENG)
#define CONFIG_SLP_BUSY_LEVEL	1
#endif
#endif

/* You should choose one of them */
//#define CONFIG_SLP_MSM_BUS	1
#define CONFIG_SLP_EXYNOS_BUS	1


#if defined(CONFIG_SLP_EXYNOS_BUS)
#define PWR_DOMAINS_NUM	1
#define CLK_GATES_NUM		4
#elif defined(CONFIG_SLP_MSM_BUS)
#define PWR_DOMAINS_NUM	3
#define CLK_GATES_NUM		5
#else
#define PWR_DOMAINS_NUM	3
#define CLK_GATES_NUM		5
#endif


enum {
	NR_RUNNING_TASK,
	MIF_BUS_FREQ,
	MIF_BUS_LOAD,
	INT_BUS_FREQ,
	INT_BUS_LOAD,
	GPU_FREQ,
	GPU_UTILIZATION,
	BATTERY_SOC,
	LCD_BRIGHTNESS,
	ACTIVE_APP_PID,
	SUSPEND_STATE,
	SUSPEND_COUNT,
#ifdef CONFIG_SLP_CHECK_RESOURCE
	CONN_BT_ENABLED,
	CONN_BT_TX,
	CONN_BT_RX,
	CONN_WIFI_ENABLED,
	CONN_WIFI_TX,
	CONN_WIFI_RX,
#endif
};

struct saved_load_factor_tag {
	unsigned int nr_running_task;
	unsigned int mif_bus_freq;
	unsigned int mif_bus_load;
	unsigned int int_bus_freq;
	unsigned int int_bus_load;
	unsigned int gpu_freq;
	unsigned int gpu_utilization;
	unsigned int active_app_pid;
	unsigned int battery_soc;
	unsigned int lcd_brightness;
	unsigned int suspend_state;
	unsigned int suspend_count;
#ifdef CONFIG_SLP_CHECK_RESOURCE
	unsigned int bt_tx_bytes;
	unsigned int bt_rx_bytes;
	unsigned int bt_enabled;
	unsigned int wifi_tx_bytes;
	unsigned int wifi_rx_bytes;
	unsigned int wifi_enabled;
#endif
};

extern struct saved_load_factor_tag	saved_load_factor;

extern bool cpu_task_history_onoff;

extern int value_for_debug;

extern int current_monitor_en;

extern struct cpuidle_device *cpu_idle_dev;

extern int kernel_mini_tracer_i2c_log_on;

void cpu_print_buf_to_klog(char *buffer);

void store_external_load_factor(int type, unsigned int data);

void store_cpu_load(unsigned int cpu_load[]);

void cpu_load_touch_event(unsigned int event);

void __slp_store_task_history(unsigned int cpu, struct task_struct *task);

static inline void slp_store_task_history(unsigned int cpu
					, struct task_struct *task)
{
	__slp_store_task_history(cpu, task);
}

u64  get_load_analyzer_time(void);

void __slp_store_work_history(struct work_struct *work, work_func_t func
						, u64 start_time, u64 end_time);

void __slp_store_input_history(void *dev,
			       unsigned int type, unsigned int code, int value);

void store_killed_task(struct task_struct *tsk);
int search_killed_task(unsigned int pid, char *task_name);

void cpu_last_load_freq(unsigned int range, int max_list_num);






#if defined(CONFIG_LOAD_ANALYZER_INTERNAL)

struct cpu_process_runtime_tag {
	struct list_head list;
	unsigned long long runtime;
	struct task_struct *task;
	unsigned int pid;
	unsigned int cnt;
	unsigned int usage;
};

#if defined (CONFIG_CHECK_WORK_HISTORY)
struct cpu_work_runtime_tag {
	struct list_head list;
	u64 start_time;
	u64 end_time;
	u64 occup_time;

	struct task_struct *task;
	unsigned int pid;
	unsigned int cnt;

	struct work_struct *work;
	work_func_t func;
};
#endif

struct cpu_load_freq_history_tag {
	char time[16];
	unsigned long long time_stamp;
	unsigned int cpufreq[CPU_NUM];
	int cpu_max_locked_freq;
	int cpu_min_locked_freq;
	int cpu_max_locked_online;
	int cpu_min_locked_online;

	unsigned int cpu_load[CPU_NUM];
	unsigned int cpu_idle_time[3];

	unsigned int touch_event;
	unsigned int nr_onlinecpu;
	unsigned int nr_run_avg;
	unsigned int task_history_cnt[CPU_NUM];
	unsigned int gpu_freq;
	unsigned int gpu_utilization;

	char status;
	unsigned int suspend_count;
	unsigned int pid;
	unsigned int battery_soc;
	unsigned int lcd_brightness;
#if defined (CONFIG_CHECK_WORK_HISTORY)
	unsigned int work_history_cnt[CPU_NUM];
#endif

//#if defined (CONFIG_SLP_INPUT_REC)
	unsigned int input_rec_history_cnt;
//#endif

#if defined(CONFIG_SLP_CHECK_BUS_LOAD)

#if defined(CONFIG_SLP_EXYNOS_BUS)
	unsigned int mif_bus_freq;
	unsigned int mif_bus_load;
	unsigned int int_bus_freq;
	unsigned int int_bus_load;
#elif defined(CONFIG_SLP_MSM_BUS)
	unsigned int bimc_clk;
	unsigned int cnoc_clk;
	unsigned int pnoc_clk;
	unsigned int snoc_clk;
#endif
#endif

#if defined(CONFIG_SLP_BUS_CLK_CHECK_LOAD)
	unsigned int power_domains[PWR_DOMAINS_NUM];
	unsigned int clk_gates[CLK_GATES_NUM];
#endif
#if defined(CONFIG_SLP_CHECK_RESOURCE)
	unsigned int bt_tx_bytes;
	unsigned int bt_rx_bytes;
	unsigned int bt_enabled;

	unsigned int wifi_tx_bytes;
	unsigned int wifi_rx_bytes;
	unsigned int wifi_enabled;

/* for the 3G Models
	unsigned int modem_tx_bytes;
	unsigned int modem_rx_bytes;
*/
#endif
#if defined(CONFIG_SLUGGISH_ANALYZER)
	struct sluggish_load_factor_tag sl_factor;
#endif
};

struct cpu_task_history_tag {
	unsigned long long time;
	struct task_struct *task;
	unsigned int pid;
};
static struct cpu_task_history_tag (*cpu_task_history)[CPU_NUM];
static struct cpu_task_history_tag	 (*cpu_task_history_view)[CPU_NUM];
#define CPU_TASK_HISTORY_SIZE	(sizeof(struct cpu_task_history_tag) \
					* cpu_task_history_num * CPU_NUM)


unsigned int show_cpu_load_freq_sub(int cnt, int show_cnt, char *buf, unsigned int buf_size, int ret);

extern char cpu_load_freq_menu[];

#if defined(CONFIG_SLP_CHECK_BUS_LOAD)
extern char cpu_bus_load_freq_menu[];
unsigned int show_cpu_bus_load_freq_sub(int cnt, int show_cnt
					, char *buf, unsigned int buf_size, int ret);
#endif

#if defined(CONFIG_SLP_BUS_CLK_CHECK_LOAD)
extern char cpu_bus_clk_load_freq_menu[];
unsigned int show_cpu_bus_clk_load_freq_sub(int cnt
					, int show_cnt, char *buf, int ret);
#endif



/******** +CONFIG_SLP_CURRENT_MONITOR+  ********/

#if defined (CONFIG_SLP_CURRENT_MONITOR)
void current_monitor_manager(int cnt);
unsigned int show_current_monitor_read_sub(int cnt, int show_cnt
					, char *buf, unsigned int buf_size, int ret);
#endif
/******** -CONFIG_SLP_CURRENT_MONITOR- ********/




/******** +CONFIG_CHECK_NOT_CPUIDLE_CAUSE+  ********/
#if defined(CONFIG_CHECK_NOT_CPUIDLE_CAUSE)
static ssize_t not_lpa_cause_check(struct file *file,
			char __user *buffer, size_t count, loff_t *ppos);
static int not_lpa_cause_check_sub(char *buf, int buf_size);
#endif
/******** -CONFIG_CHECK_NOT_CPUIDLE_CAUSE- ********/




/******** +CONFIG_CHECK_WORK_HISTORY+  ********/
#if defined (CONFIG_CHECK_WORK_HISTORY)
struct cpu_work_history_tag {
	u64 start_time;
	u64 end_time;
//	u64 occup_time;

	struct task_struct *task;
	unsigned int pid;
	struct work_struct *work;
	work_func_t	func;
};

#define CPU_WORK_HISTORY_SIZE	(sizeof(struct cpu_work_history_tag) \
					* cpu_work_history_num * CPU_NUM)

static unsigned int  cpu_work_history_cnt[CPU_NUM];
static struct cpu_work_history_tag (*cpu_work_history)[CPU_NUM];
static struct cpu_work_history_tag	 (*cpu_work_history_view)[CPU_NUM];
unsigned int cpu_work_history_num = CPU_WORK_HISTORY_NUM;
static unsigned int  cpu_work_history_show_start_cnt;
static unsigned int  cpu_work_history_show_end_cnt;
static  int  cpu_work_history_show_select_cpu;

static struct list_head work_headlist;

bool cpu_work_history_onoff;
#endif
/******** -CONFIG_CHECK_WORK_HISTORY- ********/


/******** +CONFIG_SLP_INPUT_REC+  ********/
#if defined (CONFIG_SLP_INPUT_REC)
#if defined(CONFIG_SLP_KERNEL_ENG) || defined (CONFIG_TIZEN_SEC_KERNEL_ENG)
#define INPUT_REC_HISTORY_NUM	20000
#else
#define INPUT_REC_HISTORY_NUM	20
#endif
#define MAX_INPUT_DEVICES	256

struct input_rec_history_tag {
	u64 time;
	struct input_dev *dev;
	unsigned int type;
	unsigned int code;
	int value;
};

#define MAX_INPUT_DEV_NAME_LEN	64
struct input_dev_info_tag {
	struct input_dev *dev;
	char name[MAX_INPUT_DEV_NAME_LEN];
};


unsigned int input_rec_history_num = INPUT_REC_HISTORY_NUM;
#define INPUT_REC_HISTORY_SIZE	(sizeof(struct input_rec_history_tag) \
					* input_rec_history_num)

static unsigned int  input_rec_history_cnt;
static struct input_rec_history_tag (*input_rec_history);
static struct input_rec_history_tag (*input_rec_history_view);
static unsigned int  input_rec_history_show_start_cnt;
static unsigned int  input_rec_history_show_end_cnt;
static  int  input_rec_history_show_select_cpu;
bool input_rec_history_onoff;
int b_input_load_data;
int input_dev_info_current_num;
unsigned int input_dev_info_saved_num;
struct input_dev_info_tag input_dev_info_current[MAX_INPUT_DEVICES];
struct input_dev_info_tag input_dev_info_saved[MAX_INPUT_DEVICES];

struct input_dev * input_rec_change_dev(struct input_dev *old_dev);
#endif
/******** -CONFIG_SLP_INPUT_REC- ********/


#endif /* end of CONFIG_LOAD_ANALYZER_INTERNAL */


/******** +CONFIG_SLP_MINI_TRACER+  ********/
#if defined(CONFIG_SLP_MINI_TRACER)

#define TIME_ON		(1 << 0)
#define FLUSH_CACHE	(1 << 1)

void kernel_mini_tracer(char *input_string, int option);
void kernel_mini_tracer_smp(char *input_string);
#define mini_trace_log {  \
	char str[64];   \
	sprintf(str, "%s %d\n", __FUNCTION__, __LINE__); \
	kernel_mini_tracer(str, TIME_ON | FLUSH_CACHE); \
}
#else
#define mini_trace_log
#endif
/******** -CONFIG_SLP_MINI_TRACER- ********/


/******** +CONFIG_SLP_CPU_TESTER+ ********/
#if defined (CONFIG_SLP_CPU_TESTER)
#define CPU_TEST_START_TIME	50 /* 50 sec after boot */


enum {
	CPU_IDLE_TEST,
	CPU_FREQ_TEST,
	END_OF_LIST,
};

struct cpu_test_list_tag {
	int test_item;
	int setting_value;
	int test_time;   /* ms */
	unsigned int test_cnt;
};


struct cpu_test_freq_table_tag {
	int cpufreq;
	int enter_count;
	int exit_count;
};

struct cpu_test_idle_table_tag {
	int cpuidle;
	int enter_count;
	int exit_count;
};

extern struct cpu_test_freq_table_tag cpu_test_freq_table[];
extern struct cpu_test_idle_table_tag cpu_test_idle_table[];

extern int cpu_tester_en;
extern int cpufreq_tester_en;
extern int cpuidle_tester_en;

extern int cpu_idle_trace_en;
extern int cpufreq_force_state;

int cpu_freq_to_enum(int cpufreq);
int cpuidle_force_set(int *force_state, int next_state);
int cpufreq_force_set(int *force_state, int target_freq);
void set_cpufreq_force_state(int cpufreq_enum);
#endif


/******** +CONFIG_SLP_BUSY_LEVEL+ ********/
#if defined (CONFIG_SLP_BUSY_LEVEL)
enum {
	BUSY_LOAD,
	NOT_BUSY_LOAD,
};

extern int cpu_busy_level;
int la_get_cpu_busy_level(void);
void la_set_cpu_busy_level(int set_cpu_busy_level);
int check_load_level(unsigned int current_cnt);
#endif


#endif
