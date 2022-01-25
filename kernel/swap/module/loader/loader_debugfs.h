#ifndef __LOADER_DEBUGFS_H__
#define __LOADER_DEBUGFS_H__

struct dentry;

int ld_init(void);
void ld_exit(void);

struct dentry *ld_get_loader_dentry(void);
unsigned long ld_get_loader_offset(void);

unsigned long ld_r_state_offset(void);

#endif /* __LOADER_DEBUGFS_H__ */
