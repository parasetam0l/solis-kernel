#
# Makefile for the sensor drivers.
#

# Each configuration option enables a list of files.
obj-$(CONFIG_SENSORS_SSP)		+= ssp_dev.o ssp_spi.o ssp_data.o ssp_sysfs.o\
						ssp_input.o ssp_firmware.o ssp_debug.o
obj-$(CONFIG_SENSORS_SYSFS)		+= sensors_core.o
obj-$(CONFIG_SENSORS_SSP_SENSORHUB)	+= ssp_sensorhub.o

obj-$(CONFIG_SENSORS_SSP_FACTORY)	+= factory/ssp_factory.o factory/
obj-$(CONFIG_SENSORS_SSP_BBD)		+= ssp_bbd.o factory/mcu_bcm477x.o
obj-$(CONFIG_SENSORS_SSP_BBD)		+= bbdpl/
