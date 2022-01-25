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

obj-$(CONFIG_SWAP_DA) := swap_preload.o
swap_preload-y := preload_module.o \
                  preload_control.o \
                  preload_debugfs.o \
                  preload_probe.o \
                  preload_threads.o \
                  preload_process.o
