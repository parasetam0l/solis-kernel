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

obj-$(CONFIG_SWAP_DA) := swap_kprobe.o
swap_kprobe-y := \
	swap_kprobes_deps.o \
	swap_slots.o \
	swap_td_raw.o \
	swap_ktd.o


### ARM64
swap_kprobe-$(CONFIG_ARM64) += \
	arch/arm64/swap-asm/simulate-insn.o \
	arch/arm64/swap-asm/condn-helpers.o


ifeq ($(CONFIG_SWAP_KERNEL_IMMUTABLE), y)

swap_kprobe-y += swap_no_kprobes.o

else # CONFIG_SWAP_KERNEL_IMMUTABLE

swap_kprobe-y += swap_kprobes.o

### ARM
swap_kprobe-$(CONFIG_ARM) += \
	arch/arm/swap-asm/swap_kprobes.o \
	../arch/arm/probes/probes_arm.o \
	../arch/arm/probes/tramps_arm_img.o

ifeq ($(CONFIG_STRICT_MEMORY_RWX), y)
swap_kprobe-$(CONFIG_ARM) += arch/arm/swap-asm/memory_rwx.o
endif #ifeq ($(CONFIG_STRICT_MEMORY_RWX), y)


### ARM64
swap_kprobe-$(CONFIG_ARM64) += arch/arm64/swap-asm/swap_kprobes.o \
                               arch/arm64/swap-asm/dbg_interface.o \
                               arch/arm64/swap-asm/kprobes-arm64.o


### X86
swap_kprobe-$(CONFIG_X86) += arch/x86/swap-asm/swap_kprobes.o

endif # CONFIG_SWAP_KERNEL_IMMUTABLE
