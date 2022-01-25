#
# Makefile for the RF switch subsystem.
#

ifeq ($(CONFIG_MRVL_WL_SD8977),y)
EXTRA_CFLAGS += -DCSA_MAC_FILE_PATH='"/csa/.mac.info"'
EXTRA_CFLAGS += -DOPT_MAC_FILE_PATH='"/opt/etc/.mac.info"'
EXTRA_CFLAGS += -DWLAN_MAC_FILE_PATH='"/sys/module/sd8xxx/parameters/mac_addr"'
EXTRA_CFLAGS += -DPSM_FILE_PATH='"/opt/etc/.psm.info"'
EXTRA_CFLAGS += -DWLAN_AUTO_DS_FILE_PATH='"/sys/module/sd8xxx/parameters/auto_ds"'
EXTRA_CFLAGS += -DWLAN_PS_MODE_FILE_PATH='"/sys/module/sd8xxx/parameters/ps_mode"'
EXTRA_CFLAGS += -DWLAN_HW_TEST_FILE_PATH='"/sys/module/sd8xxx/parameters/hw_test"'
EXTRA_CFLAGS += -DCSA_BDADDR_FILE_PATH='"/csa/bluetooth/.bd_addr"'
EXTRA_CFLAGS += -DBT_MAC_FILE_PATH='"/sys/module/bt8xxx/parameters/bt_mac"'
endif

rfkill-y			+= core.o
rfkill-$(CONFIG_RFKILL_INPUT)	+= input.o
obj-$(CONFIG_RFKILL)		+= rfkill.o
obj-$(CONFIG_RFKILL_REGULATOR)	+= rfkill-regulator.o
obj-$(CONFIG_RFKILL_GPIO)	+= rfkill-gpio.o

