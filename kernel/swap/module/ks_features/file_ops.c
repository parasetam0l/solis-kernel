#include <linux/kconfig.h>

#ifndef CONFIG_SWAP_HOOK_SYSCALL

#include <linux/module.h>
#include <linux/dcache.h>
#include <linux/percpu.h>
#include <linux/namei.h>
#include <linux/fcntl.h>
#include <linux/stat.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <kprobe/swap_kprobes.h>
#include <writer/event_filter.h>
#include "ks_map.h"
#include "ksf_msg.h"
#include "file_ops.h"

#define FOPS_PREFIX "[FILE_OPS] "

#define PT_FILE 0x4 /* probe type FILE(04) */

/* path buffer size */
enum { PATH_LEN = 512 };

struct file_probe {
	int id;
	const char *args;
	int subtype;
	struct kretprobe rp;
};

#define to_file_probe(_rp) container_of(_rp, struct file_probe, rp)

/* common private data */
struct file_private {
	struct dentry *dentry;
};

/* open/creat private data */
struct open_private {
	int dfd;
	const char __user *name;
	int ret;
};

/* locks private data */
struct flock_private {
	struct dentry *dentry;
	int subtype;
};

#define DECLARE_HANDLER(_name) \
	int _name(struct kretprobe_instance *, struct pt_regs *)

/* generic handlers forward declaration */
static DECLARE_HANDLER(generic_entry_handler);
static DECLARE_HANDLER(generic_ret_handler);
/* open/creat handlers */
static DECLARE_HANDLER(open_entry_handler);
static DECLARE_HANDLER(open_ret_handler);
/* lock handlers */
static DECLARE_HANDLER(lock_entry_handler);
static DECLARE_HANDLER(lock_ret_handler);
/* filp_close helper handlers */
static DECLARE_HANDLER(filp_close_entry_handler);
static DECLARE_HANDLER(filp_close_ret_handler);

#define FILE_OPS_OPEN_LIST \
	X(sys_open, sdd), \
	X(sys_openat, dsdd), \
	X(sys_creat, sd)

#define FILE_OPS_CLOSE_LIST \
	X(sys_close, d)

#define FILE_OPS_READ_LIST \
	X(sys_read, dpd), \
	X(sys_readv, dpd), \
	X(sys_pread64, dpxx), \
	X(sys_preadv, dpxxx)

#define FILE_OPS_WRITE_LIST \
	X(sys_write, dpd), \
	X(sys_writev, dpd), \
	X(sys_pwrite64, dpxx), \
	X(sys_pwritev, dpxxx)

#define FILE_OPS_LOCK_LIST \
	X(sys_fcntl, ddd), \
	X(sys_fcntl64, ddd), \
	X(sys_flock, dd)

#define FILE_OPS_LIST \
	FILE_OPS_OPEN_LIST, \
	FILE_OPS_CLOSE_LIST, \
	FILE_OPS_READ_LIST, \
	FILE_OPS_WRITE_LIST, \
	FILE_OPS_LOCK_LIST

#define X(_name, _args) \
	id_##_name
enum {
	FILE_OPS_LIST
};
#undef X

#define __FILE_PROBE_INITIALIZER(_name, _args, _subtype, _dtype, _entry, _ret) \
	{ \
		.id = id_##_name, \
		.args = #_args, \
		.subtype = _subtype, \
		.rp = { \
			.kp.symbol_name = #_name, \
			.data_size = sizeof(_dtype), \
			.entry_handler = _entry, \
			.handler = _ret, \
		} \
	}

static struct file_probe fprobes[] = {
#define X(_name, _args) \
	[id_##_name] = __FILE_PROBE_INITIALIZER(_name, _args, FOPS_OPEN, \
						struct open_private, \
						open_entry_handler, \
						open_ret_handler)
	FILE_OPS_OPEN_LIST,
#undef X

#define X(_name, _args) \
	[id_##_name] = __FILE_PROBE_INITIALIZER(_name, _args, FOPS_CLOSE, \
						struct file_private, \
						generic_entry_handler, \
						generic_ret_handler)
	FILE_OPS_CLOSE_LIST,
#undef X

#define X(_name, _args) \
	[id_##_name] = __FILE_PROBE_INITIALIZER(_name, _args, FOPS_READ, \
						struct file_private, \
						generic_entry_handler, \
						generic_ret_handler)
	FILE_OPS_READ_LIST,
#undef X

#define X(_name, _args) \
	[id_##_name] = __FILE_PROBE_INITIALIZER(_name, _args, FOPS_WRITE, \
						struct file_private, \
						generic_entry_handler, \
						generic_ret_handler)
	FILE_OPS_WRITE_LIST,
#undef X

#define X(_name, _args) \
	[id_##_name] = __FILE_PROBE_INITIALIZER(_name, _args, FOPS_OTHER, \
						struct flock_private, \
						lock_entry_handler, \
						lock_ret_handler)
	FILE_OPS_LOCK_LIST
#undef X
};

static void *fops_key_func(void *);
static int fops_cmp_func(void *, void *);

/* percpu buffer to hold the filepath inside handlers */
static DEFINE_PER_CPU(char[PATH_LEN], __path_buf);

/* map to hold 'interesting' files */
static DEFINE_MAP(__map, fops_key_func, fops_cmp_func);
static DEFINE_RWLOCK(__map_lock);

/* enabled/disabled flag */
static int fops_enabled;
static DEFINE_MUTEX(fops_lock);

/* GET/PUT debug stuff */
static int file_get_put_balance;
static int dentry_get_put_balance;

/* helper probe */
static struct kretprobe filp_close_krp = {
	.kp.symbol_name = "filp_close",
	.data_size = 0,
	.entry_handler = filp_close_entry_handler,
	.handler = filp_close_ret_handler
};

/* should be called only from handlers (with preemption disabled) */
static inline char *fops_path_buf(void)
{
	return __get_cpu_var(__path_buf);
}

static inline unsigned fops_dcount(const struct dentry *dentry)
{
	return d_count(dentry);
}

/* kernel function args */
#define fops_karg(_type, _regs, _idx) ((_type)swap_get_karg(_regs, _idx))
/* syscall args */
#define fops_sarg(_type, _regs, _idx) ((_type)swap_get_sarg(_regs, _idx))
/* retval */
#define fops_ret(_type, _regs) ((_type)regs_return_value(_regs))

#define F_ADDR(_rp) ((unsigned long)(_rp)->kp.addr) /* function address */
#define R_ADDR(_ri) ((unsigned long)(_ri)->ret_addr) /* return adress */

static void *fops_key_func(void *data)
{
	/* use ((struct dentry *)data)->d_inode pointer as map key to handle
	 * symlinks/hardlinks the same way as the original file */
	return data;
}

static int fops_cmp_func(void *key_a, void *key_b)
{
	return key_a - key_b;
}

static inline struct map *__get_map(void)
{
	return &__map;
}

static inline struct map *get_map_read(void)
{
	read_lock(&__map_lock);

	return __get_map();
}

static inline void put_map_read(struct map *map)
{
	read_unlock(&__map_lock);
}

static inline struct map *get_map_write(void)
{
	write_lock(&__map_lock);

	return __get_map();
}

static inline void put_map_write(struct map *map)
{
	write_unlock(&__map_lock);
}

static struct file *__fops_fget(int fd)
{
	struct file *file;

	file = fget(fd);
	if (IS_ERR_OR_NULL(file))
		file = NULL;
	else
		file_get_put_balance++;

	return file;
}

static void __fops_fput(struct file *file)
{
	file_get_put_balance--;
	fput(file);
}

static struct dentry *__fops_dget(struct dentry *dentry)
{
	dentry_get_put_balance++;

	return dget(dentry);
}

static void __fops_dput(struct dentry *dentry)
{
	dentry_get_put_balance--;
	dput(dentry);
}

static int fops_dinsert(struct dentry *dentry)
{
	struct map *map;
	int ret;

	map = get_map_write();
	ret = insert(map, __fops_dget(dentry));
	put_map_write(map);

	if (ret)
		__fops_dput(dentry);

	/* it's ok if dentry is already inserted */
	return ret == -EEXIST ? 0 : ret;
}

static struct dentry *fops_dsearch(struct dentry *dentry)
{
	struct dentry *found;
	struct map *map;

	map = get_map_read();
	found = search(map, map->key_f(dentry));
	put_map_read(map);

	return found;
}

static struct dentry *fops_dremove(struct dentry *dentry)
{
	struct dentry *removed;
	struct map *map;

	map = get_map_write();
	removed = remove(map, map->key_f(dentry));
	put_map_write(map);

	if (removed)
		__fops_dput(removed);

	return removed;
}

static int fops_fcheck(struct task_struct *task, struct file *file)
{
	struct dentry *dentry;

	if (!task || !file)
		return -EINVAL;

	dentry = file->f_path.dentry;

	/* check if it is a regular file */
	if (!S_ISREG(dentry->d_inode->i_mode))
		return -EBADF;

	if (check_event(task))
		/* it is 'our' task: just add the dentry to the map */
		return fops_dinsert(dentry) ? : -EAGAIN;
	else
		/* not 'our' task: check if the file is 'interesting' */
		return fops_dsearch(dentry) ? 0 : -ESRCH;
}

static char *fops_fpath(struct file *file, char *buf, int buflen)
{
	char *filename;

	path_get(&file->f_path);
	filename = d_path(&file->f_path, buf, buflen);
	path_put(&file->f_path);

	if (IS_ERR_OR_NULL(filename)) {
		printk(FOPS_PREFIX "d_path FAILED: %ld\n", PTR_ERR(filename));
		buf[0] = '\0';
		filename = buf;
	}

	return filename;
}

static int generic_entry_handler(struct kretprobe_instance *ri,
				 struct pt_regs *regs)
{
	struct kretprobe *rp = ri->rp;

	if (rp) {
		struct file_probe *fprobe = to_file_probe(rp);
		struct file_private *priv = (struct file_private *)ri->data;
		int fd = fops_sarg(int, regs, 0);
		struct file *file = __fops_fget(fd);

		if (fops_fcheck(current, file) == 0) {
			char *buf = fops_path_buf();

			ksf_msg_file_entry(fd, fprobe->subtype,
					   fops_fpath(file, buf, PATH_LEN));

			priv->dentry = file->f_path.dentry;
		} else {
			priv->dentry = NULL;
		}

		if (file)
			__fops_fput(file);
	}

	return 0;
}

static int generic_ret_handler(struct kretprobe_instance *ri,
			       struct pt_regs *regs)
{
	struct kretprobe *rp = ri->rp;
	struct file_private *priv = (struct file_private *)ri->data;

	if (rp && priv->dentry)
		ksf_msg_file_exit(regs, 'x');

	return 0;
}

static int open_private_init(const char *args, struct pt_regs *regs,
			     struct open_private *priv)
{
	int ret = 0;

	switch (args[0]) {
	case 'd': /* file name: relative to fd */
		if (args[1] != 's') {
			ret = -EINVAL;
			break;
		}
		priv->dfd = fops_sarg(int, regs, 0);
		priv->name = fops_sarg(const char __user *, regs, 1);
		break;
	case 's': /* file name: absolute or relative to CWD */
		priv->dfd = AT_FDCWD;
		priv->name = fops_sarg(const char __user *, regs, 0);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	priv->ret = ret;

	return ret;
}

static int open_entry_handler(struct kretprobe_instance *ri,
			      struct pt_regs *regs)
{
	struct kretprobe *rp = ri->rp;

	if (rp) {
		struct file_probe *fprobe = to_file_probe(rp);
		struct open_private *priv = (struct open_private *)ri->data;

		open_private_init(fprobe->args, regs, priv);
		/* FIXME entry event will be sent in open_ret_handler: cannot
		 * perform a file lookup in atomic context */
	}

	return 0;
}

static int open_ret_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	struct kretprobe *rp = ri->rp;
	struct open_private *priv = (struct open_private *)ri->data;

	if (rp && priv->ret == 0) {
		struct file_probe *fprobe = to_file_probe(rp);
		int fd = fops_ret(int, regs);
		struct file *file = __fops_fget(fd);

		if (fops_fcheck(current, file) == 0) {
			char *buf = fops_path_buf();
			const char *path = fops_fpath(file, buf, PATH_LEN);

			ksf_msg_file_entry_open(fd, fprobe->subtype,
						path, priv->name);
			ksf_msg_file_exit(regs, 'x');
		}

		if (file)
			__fops_fput(file);
	}

	return 0;
}

/* wrapper for 'struct flock*' data */
struct lock_arg {
	int type;
	int whence;
	s64 start;
	s64 len;
};

/* TODO copy_from_user */
#define __lock_arg_init(_type, _regs, _arg) \
	do { \
		_type __user *flock = fops_sarg(_type __user *, _regs, 2); \
		_arg->type = flock->l_type; \
		_arg->whence = flock->l_whence; \
		_arg->start = flock->l_start; \
		_arg->len = flock->l_len; \
	} while (0)

static int lock_arg_init(int id, struct pt_regs *regs, struct lock_arg *arg)
{
	unsigned int cmd = fops_sarg(unsigned int, regs, 1);
	int ret = 0;

	switch (id) {
	case id_sys_fcntl:
		if (cmd == F_SETLK || cmd == F_SETLKW)
			__lock_arg_init(struct flock, regs, arg);
		else
			ret = -EINVAL;
		break;
	case id_sys_fcntl64:
		if (cmd == F_SETLK64 || cmd == F_SETLKW64)
			__lock_arg_init(struct flock64, regs, arg);
		else if (cmd == F_SETLK || cmd == F_SETLKW)
			__lock_arg_init(struct flock, regs, arg);
		else
			ret = -EINVAL;
		break;
	case id_sys_flock: /* TODO is it really needed? */
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int lock_entry_handler(struct kretprobe_instance *ri,
			      struct pt_regs *regs)
{
	struct kretprobe *rp = ri->rp;

	if (rp) {
		struct file_probe *fprobe = to_file_probe(rp);
		struct flock_private *priv = (struct flock_private *)ri->data;
		int fd = fops_sarg(int, regs, 0);
		struct file *file = __fops_fget(fd);

		if (fops_fcheck(current, file) == 0) {
			int subtype = fprobe->subtype;
			struct lock_arg arg;
			char *buf, *filepath;

			buf = fops_path_buf();
			filepath = fops_fpath(file, buf, PATH_LEN);

			if (lock_arg_init(fprobe->id, regs, &arg) == 0) {
				subtype = arg.type == F_UNLCK ?
						FOPS_LOCK_RELEASE :
						FOPS_LOCK_START;
				ksf_msg_file_entry_lock(fd, subtype, filepath,
							arg.type, arg.whence,
							arg.start, arg.len);
			} else {
				ksf_msg_file_entry(fd, subtype, filepath);
			}

			priv->dentry = file->f_path.dentry;
			priv->subtype = subtype;
		} else {
			priv->dentry = NULL;
		}

		if (file)
			__fops_fput(file);
	}

	return 0;
}

static int lock_ret_handler(struct kretprobe_instance *ri,
			    struct pt_regs *regs)
{
	struct kretprobe *rp = ri->rp;
	struct flock_private *priv = (struct flock_private *)ri->data;

	if (rp && priv->dentry)
		ksf_msg_file_exit(regs, 'x');

	return 0;
}

static int filp_close_entry_handler(struct kretprobe_instance *ri,
				    struct pt_regs *regs)
{
	struct kretprobe *rp = ri->rp;
	struct file *file = fops_karg(struct file *, regs, 0);

	if (rp && file && file_count(file)) {
		struct dentry *dentry = file->f_path.dentry;

		/* release the file if it is going to be removed soon */
		if (dentry && fops_dcount(dentry) == 2)
			fops_dremove(dentry);
	}

	return 0;
}

static int filp_close_ret_handler(struct kretprobe_instance *ri,
				  struct pt_regs *regs)
{
	return 0;
}

static void fops_unregister_probes(struct file_probe *fprobes, int cnt)
{
	int i = cnt;

	/* probes are unregistered in reverse order */
	while (--i >= 0) {
		struct kretprobe *rp = &fprobes[i].rp;

		swap_unregister_kretprobe(rp);
		printk(FOPS_PREFIX "'%s/%08lx' kretprobe unregistered  (%d)\n",
		       rp->kp.symbol_name, F_ADDR(rp), i);
	}

	/* unregister helper probes */
	swap_unregister_kretprobe(&filp_close_krp);
}

static int fops_register_probes(struct file_probe *fprobes, int cnt)
{
	struct kretprobe *rp = &filp_close_krp;
	int ret, i = 0;

	/* register helper probes */
	ret = swap_register_kretprobe(rp);
	if (ret)
		goto fail;

	/* register syscalls */
	for (i = 0; i < cnt; i++) {
		rp = &fprobes[i].rp;

		if (!rp->entry_handler)
			rp->entry_handler = generic_entry_handler;

		if (!rp->handler)
			rp->handler = generic_ret_handler;

		ret = swap_register_kretprobe(rp);
		if (ret)
			goto fail_unreg;

		printk(FOPS_PREFIX "'%s/%08lx' kretprobe registered (%d)\n",
		       rp->kp.symbol_name, F_ADDR(rp), i);
	}

	return 0;

fail_unreg:
	fops_unregister_probes(fprobes, i);

fail:
	printk(FOPS_PREFIX "Failed to register probe: %s\n",
	       rp->kp.symbol_name);

	return ret;
}

static char *__fops_dpath(struct dentry *dentry, char *buf, int buflen)
{
	static const char *NA = "N/A";
	char *filename = dentry_path_raw(dentry, buf, buflen);

	if (IS_ERR_OR_NULL(filename)) {
		printk(FOPS_PREFIX "dentry_path_raw FAILED: %ld\n",
		       PTR_ERR(filename));
		strncpy(buf, NA, buflen);
		filename = buf;
	}

	return filename;
}

/* just a simple wrapper for passing to clear function */
static int __fops_dput_wrapper(void *data, void *arg)
{
	static char buf[PATH_LEN]; /* called under write lock => static is ok */
	struct dentry *dentry = data;
	struct inode *inode = dentry->d_inode;

	printk(FOPS_PREFIX "Releasing dentry(%p/%p/%d): %s\n",
	       dentry, inode, inode ? inode->i_nlink : 0,
	      __fops_dpath(dentry, buf, PATH_LEN));
	__fops_dput(dentry);

	return 0;
}

bool file_ops_is_init(void)
{
	return fops_enabled;
}

int file_ops_init(void)
{
	int ret = -EINVAL;

	mutex_lock(&fops_lock);

	if (fops_enabled) {
		printk(FOPS_PREFIX "Handlers already enabled\n");
		goto unlock;
	}

	ret = fops_register_probes(fprobes, ARRAY_SIZE(fprobes));
	if (ret == 0)
		fops_enabled = 1;

unlock:
	mutex_unlock(&fops_lock);

	return ret;
}

void file_ops_exit(void)
{
	struct map *map;

	mutex_lock(&fops_lock);

	if (!fops_enabled) {
		printk(FOPS_PREFIX "Handlers not enabled\n");
		goto unlock;
	}

	/* 1. unregister probes */
	fops_unregister_probes(fprobes, ARRAY_SIZE(fprobes));

	/* 2. clear the map */
	map = get_map_write();
	printk(FOPS_PREFIX "Clearing map: entries(%d)\n", map->size);
	clear(map, __fops_dput_wrapper, NULL);
	WARN(file_get_put_balance, "File GET/PUT balance: %d\n",
	     file_get_put_balance);
	WARN(dentry_get_put_balance, "Dentry GET/PUT balance: %d\n",
	     dentry_get_put_balance);
	put_map_write(map);

	/* 3. drop the flag */
	fops_enabled = 0;

unlock:
	mutex_unlock(&fops_lock);
}

#endif /* CONFIG_SWAP_HOOK_SYSCALL */
