#ifndef __PRELOAD_PROCESS_H__
#define __PRELOAD_PROCESS_H__

int pp_add_by_path(const char *path);
int pp_add_by_pid(pid_t pid);
int pp_add_by_id(const char *id);

int pp_del_by_path(const char *path);
int pp_del_by_pid(pid_t pid);
int pp_del_by_id(const char *id);
void pp_del_all(void);

int pp_enable(void);
void pp_disable(void);

int pp_set_pthread_path(const char *path);
int pp_set_init_offset(unsigned long off);

#endif /* __PRELOAD_PROCESS_H__ */
