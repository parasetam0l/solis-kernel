#ifndef __LOADER_MODULE_H__
#define __LOADER_MODULE_H__

#include <linux/types.h>

struct dentry;

bool loader_module_is_ready(void);
bool loader_module_is_running(void);
bool loader_module_is_not_ready(void);
void loader_module_set_ready(void);
void loader_module_set_running(void);
void loader_module_set_not_ready(void);

struct dentry *get_dentry(const char *filepath);
void put_dentry(struct dentry *dentry);



#endif /* __LOADER_MODULE_H__ */
