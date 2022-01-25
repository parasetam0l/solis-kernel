#ifndef __PRELOAD_CONTROL_H__
#define __PRELOAD_CONTROL_H__

struct sspt_ip;

enum preload_call_type {
	NOT_INSTRUMENTED,
	EXTERNAL_CALL,
	INTERNAL_CALL
};

int pc_init(void);
void pc_exit(void);

enum preload_call_type pc_call_type_always_inst(void *caller);
enum preload_call_type pc_call_type(struct sspt_ip *ip, void *caller);
int pc_add_instrumented_binary(char *filename);
int pc_clean_instrumented_bins(void);
int pc_add_ignored_binary(char *filename);
int pc_clean_ignored_bins(void);

unsigned int pc_get_target_names(char ***filenames_p);
void pc_release_target_names(char ***filenames_p);

unsigned int pc_get_ignored_names(char ***filenames_p);
void pc_release_ignored_names(char ***filenames_p);

bool pc_check_dentry_is_ignored(struct dentry *dentry);

#endif /* __PRELOAD_HANDLERS_CONTROL_H__ */
