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

obj-$(CONFIG_SWAP_DA) := swap_webprobe.o
swap_webprobe-y := webprobe.o \
                   webprobe_debugfs.o \
                   web_msg.o
