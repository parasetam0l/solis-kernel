#ifndef __PRELOAD_MODULE_H__
#define __PRELOAD_MODULE_H__

struct sspt_ip;
struct dentry;


enum preload_status {
	PRELOAD_ON,
	PRELOAD_OFF
};

enum preload_status pm_status(void);
int pm_switch(enum preload_status stat);

int pm_uprobe_init(struct sspt_ip *ip);
void pm_uprobe_exit(struct sspt_ip *ip);

int pm_get_caller_init(struct sspt_ip *ip);
void pm_get_caller_exit(struct sspt_ip *ip);
int pm_get_call_type_init(struct sspt_ip *ip);
void pm_get_call_type_exit(struct sspt_ip *ip);
int pm_write_msg_init(struct sspt_ip *ip);
void pm_write_msg_exit(struct sspt_ip *ip);

#endif /* __PRELOAD_MODULE_H__ */
