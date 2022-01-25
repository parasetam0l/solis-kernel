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





###############################################################################
###                      swap energy module description                     ###
###############################################################################
obj-$(CONFIG_SWAP_DA) := swap_energy.o
swap_energy-y := energy_module.o \
                 energy.o \
                 rational_debugfs.o \
                 debugfs_energy.o \
                 lcd/lcd_base.o \
                 lcd/lcd_debugfs.o





###############################################################################
###                               math support                              ###
###############################################################################
# S6E8AA0:
ifeq ($(CONFIG_LCD_S6E8AA0), y)
    swap_energy-y += lcd/s6e8aa0.o
    LCD_FUNC_LIST += s6e8aa0
endif


# PANEL_S6E8AA0:
ifeq ($(CONFIG_DISPLAY_PANEL_S6E8AA0), y)
    swap_energy-y += lcd/s6e8aa0_panel.o
    LCD_FUNC_LIST += s6e8aa0_panel
endif


# MARU:
ifeq ($(CONFIG_MARU_BACKLIGHT), y)
    swap_energy-y += lcd/maru.o
    LCD_FUNC_LIST += maru
endif





###############################################################################
###                          description functions                          ###
###############################################################################
LCD_FUNC_ARGS := void
LCD_FUNC_RET := struct lcd_ops *





###############################################################################
###                            generate defines                             ###
###############################################################################
LCD_PREFIX := lcd_energy_

# add prefix
TMP := $(foreach it, $(LCD_FUNC_LIST), $(LCD_PREFIX)$(it))
LCD_FUNC_LIST := $(TMP)

# generate DEFINITION_LCD_FUNC
TMP := ($(LCD_FUNC_ARGS));
DEFINITION_LCD_FUNC := DEFINITION_LCD_FUNC=\
$(foreach it, $(LCD_FUNC_LIST), "extern" $(LCD_FUNC_RET) $(it)$(TMP))


# generate DEFINITION_LCD_ARRAY
COMMA := ,
AND := &
DEFINITION_LCD_ARRAY := DEFINITION_LCD_ARRAY=\
"{" $(foreach it, $(LCD_FUNC_LIST), &$(it),) "}"


# generate LCD_MAKE_FNAME
LCD_MAKE_FNAME := LCD_MAKE_FNAME(name)=$(LCD_PREFIX)\#\#name





###############################################################################
###                  add generate defines to EXTRA_CFLAGS                   ###
###############################################################################
ccflags-y    += -D"$(DEFINITION_LCD_FUNC)" \
                -D"$(DEFINITION_LCD_ARRAY)" \
                -D"$(LCD_MAKE_FNAME)"
