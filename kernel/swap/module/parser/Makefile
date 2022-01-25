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

obj-$(CONFIG_SWAP_DA) := swap_message_parser.o
swap_message_parser-y := swap_msg_parser.o \
                         msg_parser.o \
                         msg_buf.o \
                         msg_cmd.o \
                         features.o \
                         us_inst.o \
                         cpu_ctrl.o \
                         usm_msg.o
