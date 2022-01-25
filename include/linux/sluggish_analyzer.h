#ifndef _SLUGGISH_ANALYZER_H_
#define _SLUGGISH_ANALYZER_H_

#include <linux/mm.h>
#include <linux/vmstat.h>
#include <linux/swap.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cpumask.h>
#include <linux/kernel_stat.h>
#include <linux/tick.h>
#include <asm/cputime.h>

#include <linux/load_analyzer.h>

#ifndef CPU_NUM
#define CPU_NUM	NR_CPUS
#endif

struct sluggish_load_factor_tag {
	u64 iowait[CPU_NUM];
	u64 irq[CPU_NUM];

	unsigned long freeram;
	unsigned long vmstat[NR_VM_EVENT_ITEMS];
};

void store_sluggish_load_factor(struct sluggish_load_factor_tag *factor);
int get_sluggish_warning(void);

#endif /* _SLUGGISH_ANALYZER_H_ */

