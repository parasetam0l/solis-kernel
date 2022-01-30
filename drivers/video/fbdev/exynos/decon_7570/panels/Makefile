obj-$(CONFIG_EXYNOS_DECON_LCD) += dsim_panel.o
obj-$(CONFIG_EXYNOS_DECON_MDNIE_LITE) += mdnie_lite.o
obj-$(CONFIG_DISPLAY_USE_INFO) += dpui.o

obj-$(and $(CONFIG_EXYNOS_DECON_MDNIE_LITE),$(CONFIG_SEC_GPIO_DVS),$(subst undefined,y,$(origin CONFIG_SAMSUNG_PRODUCT_SHIP))) += dd_mdnie.o
obj-$(and $(CONFIG_SEC_GPIO_DVS),$(subst undefined,y,$(origin CONFIG_SAMSUNG_PRODUCT_SHIP))) += dd_lcd.o dd_backlight.o dd_dpu.o dd_param.o

obj-$(CONFIG_EXYNOS_DECON_LCD_S6E8AA0)	+= s6e8aa0_mipi_lcd.o s6e8aa0_lcd_ctrl.o
obj-$(CONFIG_EXYNOS_DECON_LCD_S6E8AA5X01)	+= s6e8aa5x01_mipi_lcd.o s6e8aa5x01_lcd_ctrl.o
ifdef CONFIG_POP
obj-$(CONFIG_EXYNOS_DECON_LCD_S6E36W1X01)	+= s6e36w1x01_mipi_lcd_pop.o s6e36w1x01_lcd_ctrl_pop.o s6e36w1x01_dimming_pop.o
else
obj-$(CONFIG_EXYNOS_DECON_LCD_S6E36W1X01)       += s6e36w1x01_mipi_lcd.o s6e36w1x01_lcd_ctrl.o s6e36w1x01_dimming.o
endif
obj-$(CONFIG_EXYNOS_DECON_LCD_S6E36W2X01)       += s6e36w2x01_mipi_lcd.o s6e36w2x01_lcd_ctrl.o s6e36w2x01_dimming.o
