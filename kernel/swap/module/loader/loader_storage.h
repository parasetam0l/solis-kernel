#ifndef __LOADER_STORAGE_H__
#define __LOADER_STORAGE_H__

struct list_head;
struct dentry;

struct bin_info {
	char *path;
	/* ghot */
	struct dentry *dentry;
};

struct bin_info_el {
	struct list_head list;
	char *path;
	/* ghot */
	struct dentry *dentry;
};



int ls_add_handler(const char *path);
struct list_head *ls_get_handlers(void);
void ls_put_handlers(void);

int ls_set_linker_info(char *path);
struct bin_info *ls_get_linker_info(void);
void ls_put_linker_info(struct bin_info *info);

int ls_init(void);
void ls_exit(void);

#endif /* __LOADER_HANDLERS_H__ */
