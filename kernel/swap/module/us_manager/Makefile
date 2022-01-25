ccflags-y := -Ikernel/swap/module

ifeq ($(CONFIG_ARM), y)
    link = arm
endif

ifeq ($(CONFIG_ARM64), y)
    link = arm64
endif

ifeq ($(CONFIG_X86), y)
    link = x86
endif

ccflags-y += -Ikernel/swap/module/kprobe/arch/$(link) \
             -Ikernel/swap/module/uprobe/arch/$(link)

obj-$(CONFIG_SWAP_DA) := swap_us_manager.o
swap_us_manager-y := \
    helper.o \
    us_manager.o \
    us_slot_manager.o \
    debugfs_us_manager.o \
    sspt/sspt_ip.o \
    sspt/sspt_page.o \
    sspt/sspt_file.o \
    sspt/sspt_proc.o \
    sspt/sspt_feature.o \
    sspt/sspt_filter.o \
    pf/pf_group.o \
    pf/proc_filters.o \
    img/img_ip.o \
    img/img_file.o \
    img/img_proc.o \
    probes/probes.o \
    probes/probe_info_new.o \
    callbacks.o \
    usm_hook.o
