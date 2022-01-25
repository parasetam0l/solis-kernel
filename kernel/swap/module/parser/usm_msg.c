/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) Samsung Electronics, 2015
 *
 * 2015         Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 */


#include <linux/fs.h>
#include <linux/net.h>
#include <linux/stat.h>
#include <linux/sched.h>
#include <linux/dcache.h>
#include <linux/fdtable.h>
#include <linux/file.h>
#include <linux/version.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/net.h>
#include <writer/swap_msg.h>
#include <master/swap_deps.h>
#include <us_manager/sspt/sspt.h>	/* ... check_vma() */


#define USM_PREFIX      KERN_INFO "[USM] "


struct kmem_info {
	const char *name;
	unsigned long start;
	unsigned long end;
};

#if defined(CONFIG_ARM64) || defined(CONFIG_X86_32)
static void kmem_info_fill_common(struct kmem_info *info, struct task_struct *task)
{
	unsigned long vdso;
	struct vm_area_struct *vma_vdso;

	vdso = (unsigned long)task->mm->context.vdso;
	vma_vdso = find_vma_intersection(task->mm, vdso, vdso + 1);
	if (vma_vdso) {
		info->name = "[vdso]";
		info->start = vma_vdso->vm_start;
		info->end = vma_vdso->vm_end;
	} else {
		pr_err(USM_PREFIX "Cannot get VDSO mapping\n");
		info->name = NULL;
		info->start = 0;
		info->end = 0;
	}
}
#endif /* defined(CONFIG_ARM64) || defined(CONFIG_X86_32) */

static void kmem_info_fill(struct kmem_info *info, struct task_struct *task)
{
#if defined(CONFIG_ARM)
	info->name = "[vectors]";
	info->start = CONFIG_VECTORS_BASE;
	info->end = CONFIG_VECTORS_BASE + PAGE_SIZE;
#elif defined(CONFIG_ARM64)
	if (test_ti_thread_flag(task_thread_info(task), TIF_32BIT)) {
		info->name = "[vectors]";
		info->start = AARCH32_VECTORS_BASE;
		info->end = AARCH32_VECTORS_BASE + PAGE_SIZE;
	} else {
		kmem_info_fill_common(info, task);
	}
#elif defined(CONFIG_X86_32)
	kmem_info_fill_common(info, task);
#endif /* CONFIG_arch */
}

static inline struct timespec get_task_start_time(struct task_struct *task)
{
	return ns_to_timespec(task->real_start_time);
}



static int pack_path(void *data, size_t size, struct file *file)
{
	enum { TMP_BUF_LEN = 512 };
	char tmp_buf[TMP_BUF_LEN];
	const char NA[] = "N/A";
	const char *filename;
	size_t len = sizeof(NA);

	if (file == NULL) {
		filename = NA;
		goto cp2buf;
	}

	path_get(&file->f_path);
	filename = d_path(&file->f_path, tmp_buf, TMP_BUF_LEN);
	path_put(&file->f_path);

	if (IS_ERR_OR_NULL(filename)) {
		filename = NA;
		goto cp2buf;
	}

	len = strlen(filename) + 1;

cp2buf:
	if (size < len)
		return -ENOMEM;

	memcpy(data, filename, len);
	return len;
}





/* ============================================================================
 * =                             MSG_PROCESS_INFO                             =
 * ============================================================================
 */
struct proc_info_top {
	u32 pid;
	char comm[0];
} __packed;

struct proc_info_bottom {
	u32 ppid;
	u64 start_time;
	u64 low_addr;
	u64 high_addr;
	char bin_path[0];
} __packed;

struct lib_obj {
	u64 low_addr;
	u64 high_addr;
	char lib_path[0];
} __packed;

static int pack_lib_obj(void *data, size_t size, struct vm_area_struct *vma)
{
	int ret;
	struct lib_obj *obj = (struct lib_obj *)data;

	if (size < sizeof(*obj))
		return -ENOMEM;

	obj->low_addr = vma->vm_start;
	obj->high_addr = vma->vm_end;
	size -= sizeof(*obj);

	ret = pack_path(obj->lib_path, size, vma->vm_file);
	if (ret < 0)
		return ret;

	return ret + sizeof(*obj);
}

static int pack_shared_kmem(void *data, size_t size, struct task_struct *task)
{
	struct lib_obj *obj = (struct lib_obj *)data;
	struct kmem_info info;
	size_t name_len, obj_size;

	if (size < sizeof(*obj))
		return -ENOMEM;

	kmem_info_fill(&info, task);

	if (info.name == NULL)
		return 0;

	obj->low_addr = (u64)info.start;
	obj->high_addr = (u64)info.end;

	name_len = strlen(info.name) + 1;
	obj_size = sizeof(*obj) + name_len;
	if (size < obj_size)
		return -ENOMEM;

	memcpy(obj->lib_path, info.name, name_len);

	return obj_size;
}

static int pack_libs(void *data, size_t size, struct task_struct *task)
{
	int ret;
	struct vm_area_struct *vma;
	u32 *lib_cnt = (u32 *)data;
	const size_t old_size = size;

	if (size < sizeof(*lib_cnt))
		return -ENOMEM;

	/* packing libraries count */
	*lib_cnt = 0;
	data += sizeof(*lib_cnt);
	size -= sizeof(*lib_cnt);

	/* packing libraries */
	for (vma = task->mm->mmap; vma; vma = vma->vm_next) {
		if (check_vma(vma)) {
			ret = pack_lib_obj(data, size, vma);
			if (ret < 0)
				return ret;

			data += ret;
			size -= ret;
			++(*lib_cnt);
		}
	}

	/* packing shared kernel memory */
	ret = pack_shared_kmem(data, size, task);
	if (ret < 0)
		return ret;

	*lib_cnt += !!ret;
	size -= ret;

	return old_size - size;
}

static struct vm_area_struct *find_vma_exe_by_dentry(struct mm_struct *mm,
						     struct dentry *dentry)
{
	struct vm_area_struct *vma;

	for (vma = mm->mmap; vma; vma = vma->vm_next) {
		if (vma->vm_file && (vma->vm_flags & VM_EXEC) &&
		   (vma->vm_file->f_path.dentry == dentry))
			goto out;
	}

	vma = NULL;
out:

	return vma;
}

static int pack_proc_info_top(void *data, size_t size,
			      struct task_struct *task)
{
	struct proc_info_top *pit = (struct proc_info_top *)data;

	if (size < sizeof(*pit) + sizeof(task->comm))
		return -ENOMEM;

	pit->pid = task->tgid;
	get_task_comm(pit->comm, task);

	return sizeof(*pit) + strlen(pit->comm) + 1;
}

static int pack_proc_info_bottom(void *data, size_t size,
				 struct task_struct *task,
				 struct dentry *dentry)
{
	struct proc_info_bottom *pib = (struct proc_info_bottom *)data;
	struct vm_area_struct *vma = find_vma_exe_by_dentry(task->mm, dentry);
	struct timespec boot_time;
	struct timespec start_time;
	int ret;

	if (size < sizeof(*pib))
		return -ENOMEM;

	getboottime(&boot_time);
	start_time = timespec_add(boot_time, get_task_start_time(task));

	pib->ppid = task->real_parent->tgid;
	pib->start_time = swap_msg_spec2time(&start_time);

	if (vma) {
		pib->low_addr = vma->vm_start;
		pib->high_addr = vma->vm_end;
		ret = pack_path(pib->bin_path, size, vma->vm_file);
	} else {
		pib->low_addr = 0;
		pib->high_addr = 0;
		ret = pack_path(pib->bin_path, size, NULL);
	}

	if (ret < 0)
		return ret;

	return sizeof(*pib) + ret;
}

static int pack_proc_info(void *data, size_t size, struct task_struct *task,
			    struct dentry *dentry)
{
	int ret;
	const size_t old_size = size;

	ret = pack_proc_info_top(data, size, task);
	if (ret < 0)
		return ret;

	data += ret;
	size -= ret;

	ret = pack_proc_info_bottom(data, size, task, dentry);
	if (ret < 0)
		return ret;

	data += ret;
	size -= ret;

	ret = pack_libs(data, size, task);
	if (ret < 0)
		return ret;

	return old_size - size + ret;
}

/* Called with down\up\_read(&task->mm->mmap_sem). */
void usm_msg_info(struct task_struct *task, struct dentry *dentry)
{
	int ret;
	struct swap_msg *m;
	void *p;
	size_t size;

	m = swap_msg_get(MSG_PROC_INFO);
	p = swap_msg_payload(m);
	size = swap_msg_size(m);

	ret = pack_proc_info(p, size, task, dentry);
	if (ret < 0) {
		printk(USM_PREFIX "ERROR: message process info packing, "
		       "ret=%d\n", ret);
		goto put_msg;
	}

	swap_msg_flush(m, ret);

put_msg:
	swap_msg_put(m);
}





/* ============================================================================
 * =                              MSG_TERMINATE                               =
 * ============================================================================
 */
struct proc_terminate {
	u32 pid;
} __packed;

void usm_msg_term(struct task_struct *task)
{
	struct swap_msg *m;
	struct proc_terminate *term;

	m = swap_msg_get(MSG_TERMINATE);

	term = swap_msg_payload(m);
	term->pid = task->pid;

	swap_msg_flush(m, sizeof(*term));
	swap_msg_put(m);
}





/* ============================================================================
 * =                              MSG_PROCESS_MAP                             =
 * ============================================================================
 */
struct proc_map {
	u32 pid;
	u64 low_addr;
	u64 high_addr;
	char bin_path[0];
} __packed;

static int pack_proc_map(void *data, size_t size, struct vm_area_struct *vma)
{
	struct proc_map *map = (struct proc_map *)data;
	int ret;

	map->pid = current->tgid;
	map->low_addr = vma->vm_start;
	map->high_addr = vma->vm_end;

	ret = pack_path(map->bin_path, size - sizeof(*map), vma->vm_file);
	if (ret < 0)
		return ret;

	return ret + sizeof(*map);
}

void usm_msg_map(struct vm_area_struct *vma)
{
	int ret;
	struct swap_msg *m;
	void *p;
	size_t size;

	m = swap_msg_get(MSG_PROC_MAP);
	p = swap_msg_payload(m);
	size = swap_msg_size(m);

	ret = pack_proc_map(p, size, vma);
	if (ret < 0) {
		printk(USM_PREFIX "ERROR: message process mapping packing, "
		       "ret=%d\n", ret);
		goto put_msg;
	}

	swap_msg_flush(m, ret);

put_msg:
	swap_msg_put(m);
}





/* ============================================================================
 * =                             MSG_PROCESS_UNMAP                            =
 * ============================================================================
 */
struct proc_unmap {
	u32 pid;
	u64 low_addr;
	u64 high_addr;
} __packed;

void usm_msg_unmap(unsigned long start, unsigned long end)
{
	struct swap_msg *m;
	struct proc_unmap *unmap;

	m = swap_msg_get(MSG_PROC_UNMAP);

	unmap = swap_msg_payload(m);
	unmap->pid = current->tgid;
	unmap->low_addr = (u64)start;
	unmap->high_addr = (u64)end;

	swap_msg_flush(m, sizeof(*unmap));
	swap_msg_put(m);
}





/* ============================================================================
 * =                             MSG_PROCESS_COMM                             =
 * ============================================================================
 */
struct proc_comm {
	u32 pid;
	char comm[0];
} __packed;

void usm_msg_comm(struct task_struct *task)
{
	struct swap_msg *m;
	struct proc_comm *c;

	m = swap_msg_get(MSG_PROC_COMM);

	c = swap_msg_payload(m);
	c->pid = task->tgid;
	get_task_comm(c->comm, task);

	swap_msg_flush(m, sizeof(*c) + strlen(c->comm) + 1);
	swap_msg_put(m);
}





/* ============================================================================
 * =                          MSG_PROCESS_STATUS_INFO                         =
 * ============================================================================
 */
struct ofile {
	u32 fd;
	u64 size;
	char path[0];
} __packed;

struct osock {
	u32 fd;
	u32 ip;
	u32 port;
} __packed;

static int pack_ofile(void *data, size_t size, int fd, struct file *file,
		      loff_t fsize)
{
	int ret;
	struct ofile *ofile;

	if (size < sizeof(*ofile))
		return -ENOMEM;

	ofile = (struct ofile *)data;
	ofile->fd = (u32)fd;
	ofile->size = (u64)fsize;

	ret = pack_path(ofile->path, size - sizeof(*ofile), file);
	if (ret < 0)
		return ret;

	return sizeof(*ofile) + ret;
}

static int is_file_mode(umode_t mode)
{
	return S_ISREG(mode);
}

static int pack_osock(void *data, size_t size, int fd, struct file *file,
		      loff_t fsize)
{
	struct osock *pack_sock;
	struct socket *sock;
	struct sockaddr_storage addr;
	struct sockaddr *saddr = (struct sockaddr *)&addr;
	struct sockaddr_in *sin = (struct sockaddr_in *)&addr;
	int ret_size = sizeof(*pack_sock);
	int addrlen;
	int err;

	if (size < sizeof(*pack_sock))
		return -ENOMEM;

	sock = sock_from_file(file, &err);
	if (!sock)
		return 0;

	kernel_getsockname(sock, saddr, &addrlen);

	pack_sock = (struct osock *)data;
	pack_sock->fd = (u32)fd;
	pack_sock->ip = (u32)0x0;
	pack_sock->port = 0;

	switch (saddr->sa_family) {
	case AF_INET:
		pack_sock->ip = (u32)(sin->sin_addr.s_addr);
		pack_sock->port = ntohs(sin->sin_port);
		break;
	default:
		pr_info("[USM] ignored unknown address type: %u\n",
			(unsigned int)saddr->sa_family);
		ret_size = 0;
		break;
	}


	return ret_size;
}

static int is_sock_mode(umode_t mode)
{
	return S_ISSOCK(mode);
}

typedef int (*pack_func_t)(void *data, size_t size, int fd, struct file *file,
			    loff_t fsize);
typedef int (*check_mode_func_t)(umode_t mode);

static int pack_descriptors(void *data, size_t size, pack_func_t pack_func,
			    check_mode_func_t check_mode,
			    struct files_struct *files)
{
	int ret, fd;
	const size_t old_size = size;
	u32 *desc_cnt;

	/* pack descriptor count */
	desc_cnt = (u32 *)data;
	*desc_cnt = 0;
	data += 4;
	size -= 4;

	/* pack descriptors list */
	rcu_read_lock();
	for (fd = 0; fd < files_fdtable(files)->max_fds; ++fd) {
		struct file *file;
		struct inode *inode;
		loff_t fsize;

		file = fcheck_files(files, fd);
		if (file == NULL)
			continue;

		inode = file->f_path.dentry->d_inode;
		/* check inode and if it is a regular file */
		if (!inode || !check_mode(inode->i_mode))
			continue;

		fsize = inode->i_size;
		rcu_read_unlock();

		ret = pack_func(data, size, fd, file, fsize);
		if (ret < 0) {
			goto exit;
		} else if (ret > 0) {
			/* increment if data packed only */
			data += ret;
			size -= ret;
			++(*desc_cnt);
		}

		rcu_read_lock();
	}

	rcu_read_unlock();
	ret = old_size - size;

exit:
	return ret;
}

static int pack_status_info(void *data, size_t size, struct task_struct *task)
{
	int ret;
	const size_t old_size = size;
	struct files_struct *files;

	files = swap_get_files_struct(task);
	if (!files)
		return -EIO;

	/* pack pid */
	*((u32 *)data) = (u32)task->tgid;
	data += 4;
	size -= 4;

	/* pack file descriptors */
	ret = pack_descriptors(data, size, pack_ofile, is_file_mode, files);
	if (ret < 0)
		goto put_fstruct;
	data += ret;
	size -= ret;

	/* pack sock descriptors */
	ret = pack_descriptors(data, size, pack_osock, is_sock_mode, files);
	if (ret < 0)
		goto put_fstruct;
	data += ret;
	size -= ret;

	ret = old_size - size;

put_fstruct:
	swap_put_files_struct(files);
	return ret;
}

void usm_msg_status_info(struct task_struct *task)
{
	int ret;
	void *data;
	size_t size;
	struct swap_msg *m;

	m = swap_msg_get(MSG_PROCESS_STATUS_INFO);

	data = swap_msg_payload(m);
	size = swap_msg_size(m);
	ret = pack_status_info(data, size, task);
	if (ret < 0) {
		printk(USM_PREFIX "ERROR: MSG_PROCESS_STATUS_INFO "
		       "packing, ret=%d\n", ret);
		goto put_msg;
	}

	swap_msg_flush(m, ret);

put_msg:
	swap_msg_put(m);
}
