#ifndef __USE_PROBES_H__
#define __USE_PROBES_H__

#include "probes.h"

struct sspt_ip;

void probe_info_init(enum probe_t type, struct sspt_ip *ip);
void probe_info_uninit(enum probe_t type, struct sspt_ip *ip);
int probe_info_register(enum probe_t type, struct sspt_ip *ip);
void probe_info_unregister(enum probe_t type, struct sspt_ip *ip, int disarm);
struct uprobe *probe_info_get_uprobe(enum probe_t type, struct sspt_ip *ip);
int probe_info_copy(const struct probe_info *pi, struct probe_info *dest);
void probe_info_cleanup(struct probe_info *pi);

#endif /* __USE_PROBES_H__ */
