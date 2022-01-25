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

obj-$(CONFIG_SWAP_DA) := swap_uprobe.o
swap_uprobe-y := swap_uprobes.o

### ARM
swap_uprobe-$(CONFIG_ARM) += \
	arch/arm/swap-asm/swap_uprobes.o \
	../arch/arm/probes/probes_thumb.o \
	../arch/arm/probes/decode_thumb.o \
	../arch/arm/probes/tramps_thumb_img.o \
	../arch/arm/probes/probes.o \
	../arch/arm/uprobe/swap_uprobe.o


### ARM64
swap_uprobe-$(CONFIG_ARM64) += \
	arch/arm64/swap-asm/swap_uprobes.o \
	arch/arm64/swap-asm/uprobes-arm64.o \
	../arch/arm/probes/probes_arm.o \
	../arch/arm/probes/tramps_arm_img.o \
	../arch/arm/probes/decode_thumb.o \
	../arch/arm/probes/probes_thumb.o \
	../arch/arm/probes/tramps_thumb_img.o \
	../arch/arm/probes/probes.o \
	../arch/arm/uprobe/swap_uprobe.o


### X86
swap_uprobe-$(CONFIG_X86) += arch/x86/swap-asm/swap_uprobes.o \
			     arch/x86/swap-asm/swap_sc_patch.o
