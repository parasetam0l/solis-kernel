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

obj-$(CONFIG_SWAP_DA) := swap_ksyms.o
swap_ksyms-y := ksyms_module.o

ifeq ($(CONFIG_KALLSYMS),y)
	swap_ksyms-y += ksyms.o
else
	swap_ksyms-y += no_ksyms.o
endif
