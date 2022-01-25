#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <ks_features/ks_map.h>
#include <us_manager/us_common_file.h>
#include "loader_defs.h"
#include "loader_module.h"
#include "loader_storage.h"

static struct bin_info __linker_info = { NULL, NULL };

static LIST_HEAD(handlers_list);


static bool __check_dentry_already_exist(struct dentry *dentry)
{
	struct bin_info_el *bin;
	bool ret = false;

	list_for_each_entry(bin, &handlers_list, list) {
		if (bin->dentry == dentry) {
			ret = true;
			goto out;
		}
	}

out:
	return ret;
}

static inline int __add_handler(const char *path)
{
	struct dentry *dentry;
	size_t len = strnlen(path, PATH_MAX);
	struct bin_info_el *bin;
	int ret = 0;

	dentry = swap_get_dentry(path);
	if (!dentry) {
		ret = -ENOENT;
		goto add_handler_out;
	}

	if (__check_dentry_already_exist(dentry)) {
		ret = 1;
		goto add_handler_fail_release_dentry;
	}

	bin = kmalloc(sizeof(*bin), GFP_KERNEL);
	if (bin == NULL) {
		ret = -ENOMEM;
		goto add_handler_fail_release_dentry;
	}

	bin->path = kmalloc(len + 1, GFP_KERNEL);
	if (bin->path == NULL) {
		ret = -ENOMEM;
		goto add_handler_fail_free_bin;
	}

	INIT_LIST_HEAD(&bin->list);
	strncpy(bin->path, path, len);
	bin->path[len] = '\0';
	bin->dentry = dentry;
	list_add_tail(&bin->list, &handlers_list);

	return ret;

add_handler_fail_free_bin:
	kfree(bin);

add_handler_fail_release_dentry:
	swap_put_dentry(dentry);

add_handler_out:
	return ret;
}

static inline void __remove_handler(struct bin_info_el *bin)
{
	list_del(&bin->list);
	swap_put_dentry(bin->dentry);
	kfree(bin->path);
	kfree(bin);
}

static inline void __remove_handlers(void)
{
	struct bin_info_el *bin, *tmp;

	list_for_each_entry_safe(bin, tmp, &handlers_list, list)
		__remove_handler(bin);
}

static inline struct bin_info *__get_linker_info(void)
{
	return &__linker_info;
}

static inline bool __check_linker_info(void)
{
	return (__linker_info.dentry != NULL); /* TODO */
}

static inline int __init_linker_info(char *path)
{
	struct dentry *dentry;
	size_t len = strnlen(path, PATH_MAX);
	int ret = 0;


	__linker_info.path = kmalloc(len + 1, GFP_KERNEL);
	if (__linker_info.path == NULL) {
		ret = -ENOMEM;
		goto init_linker_fail;
	}

	dentry = swap_get_dentry(path);
	if (!dentry) {
		ret = -ENOENT;
		goto init_linker_fail_free;
	}

	strncpy(__linker_info.path, path, len);
	__linker_info.path[len] = '\0';
	__linker_info.dentry = dentry;

	return ret;

init_linker_fail_free:
	kfree(__linker_info.path);

init_linker_fail:

	return ret;
}

static inline void __drop_linker_info(void)
{
	kfree(__linker_info.path);
	__linker_info.path = NULL;

	if (__linker_info.dentry)
		swap_put_dentry(__linker_info.dentry);
	__linker_info.dentry = NULL;
}




int ls_add_handler(const char *path)
{
	int ret;

	/* If ret is positive - handler was not added, because it is
	 * already exists */
	ret = __add_handler(path);
	if (ret < 0)
		return ret;

	return 0;
}

struct list_head *ls_get_handlers(void)
{
	/* TODO counter, syncs */
	return &handlers_list;
}

void ls_put_handlers(void)
{
	/* TODO dec counter, release sync */
}

int ls_set_linker_info(char *path)
{
	return __init_linker_info(path);
}

struct bin_info *ls_get_linker_info(void)
{
	struct bin_info *info = __get_linker_info();

	if (__check_linker_info())
		return info;

	return NULL;
}

void ls_put_linker_info(struct bin_info *info)
{
}

int ls_init(void)
{
	return 0;
}

void ls_exit(void)
{
	__drop_linker_info();
	__remove_handlers();
}
