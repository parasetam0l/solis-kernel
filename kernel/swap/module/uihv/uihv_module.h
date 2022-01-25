#ifndef __UIHV_MODULE_H__
#define __UIHV_MODULE_H__

int uihv_data_set(const char *app_path, unsigned long main_addr);
int uihv_set_handler(char *path);
int uihv_enable(void);
int uihv_disable(void);

#endif /* __UIHV_MODULE_H__ */
