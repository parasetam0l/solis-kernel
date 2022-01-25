#include <linux/debugfs.h>
#include <linux/module.h>

#include <master/swap_debugfs.h>
#include <master/swap_initializer.h>
#include <us_manager/sspt/sspt_proc.h>

#include "debugfs_us_manager.h"

#define MAX_APPS_COUNT  8   /* According to daemon defenitions */
#define PID_STRING      21  /* Maximum pid string = 20 (max digits count in
			      * unsigned int on 64-bit arch) + 1 (for \n) */

/* ============================================================================
 * =                          FOPS_TASKS                                      =
 * ============================================================================
 */

struct read_buf {
	char *begin;
	char *ptr;
	char *end;
};

static void on_each_proc_callback(struct sspt_proc *proc, void *data)
{
	struct read_buf *rbuf = (struct read_buf *)data;
	char pid_str[PID_STRING];
	int len;

	/* skip process */
	if (!sspt_proc_is_send_event(proc))
		return;

	snprintf(pid_str, sizeof(pid_str), "%d", proc->tgid);

	len = strlen(pid_str);

	if (rbuf->end - rbuf->ptr < len + 2)
		return;

	memcpy(rbuf->ptr, pid_str, len);
	rbuf->ptr += len;

	*rbuf->ptr = ' ';
	++rbuf->ptr;
}

static ssize_t read_tasks(struct file *file, char __user *user_buf,
			  size_t count, loff_t *ppos)
{
	char buf[PID_STRING * MAX_APPS_COUNT];
	struct read_buf rbuf = {
		.begin = buf,
		.ptr = buf,
		.end = buf + sizeof(buf)
	};

	on_each_proc_no_lock(on_each_proc_callback, (void *)&rbuf);

	if (rbuf.ptr != rbuf.begin)
		rbuf.ptr--;

	*rbuf.ptr = '\n';

	return simple_read_from_buffer(user_buf, count, ppos, rbuf.begin,
				   rbuf.ptr - rbuf.begin);
}

static const struct file_operations fops_tasks = {
	.owner = THIS_MODULE,
	.open = swap_init_simple_open,
	.release = swap_init_simple_release,
	.read = read_tasks,
	.llseek = default_llseek
};

/* ============================================================================
 * =                          INIT/EXIT                                       =
 * ============================================================================
 */

static struct dentry *us_manager_dir;

/**
 * @brief Destroy debugfs for us_manager
 *
 * @return Void
 */
void exit_debugfs_us_manager(void)
{
	if (us_manager_dir)
		debugfs_remove_recursive(us_manager_dir);

	us_manager_dir = NULL;
}

/**
 * @brief Create debugfs for us_manager
 *
 * @return Error code
 */
int init_debugfs_us_manager(void)
{
	struct dentry *swap_dir, *dentry;

	swap_dir = swap_debugfs_getdir();
	if (swap_dir == NULL)
		return -ENOENT;

	us_manager_dir = swap_debugfs_create_dir(US_MANAGER_DFS_DIR, swap_dir);
	if (us_manager_dir == NULL)
		return -ENOMEM;

	dentry = swap_debugfs_create_file(US_MANAGER_TASKS, 0600,
					  us_manager_dir, NULL, &fops_tasks);
	if (dentry == NULL)
		goto fail;

	return 0;

fail:
	exit_debugfs_us_manager();
	return -ENOMEM;
}
