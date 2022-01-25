dtb-$(CONFIG_ARCH_THUNDER) += thunder-88xx.dtb
dtb-$(CONFIG_ARCH_VEXPRESS) += rtsm_ve-aemv8a.dtb foundation-v8.dtb
dtb-$(CONFIG_ARCH_XGENE) += apm-mustang.dtb
dtb-$(CONFIG_SOC_EXYNOS8890) += exynos8890-espresso8890.dtb exynos8890-espresso8890_rev01.dtb exynos8890-universal8890.dtb exynos8890-smdk8890.dtb
dtb-$(CONFIG_SOC_EXYNOS8890) += exynos8890-evt1-smdk8890.dtb
dtb-$(CONFIG_SOC_EXYNOS7870) += exynos7870-smdk7870.dtb exynos7870-universal7870.dtb exynos7870-universal7870_rev01.dtb
ifeq ($(CONFIG_SOLIS),y)
ifeq ($(CONFIG_SOLIS_LTE),y)
dtb-$(CONFIG_SOLIS_LTE) += exynos7270-tizen_solis_lte-r03.dtb \
                           exynos7270-tizen_solis_lte-r04.dtb \
                           exynos7270-tizen_solis_lte-r05.dtb \
                           exynos7270-tizen_solis_lte-r06.dtb \
                           exynos7270-tizen_solis_lte-r07.dtb \
                           exynos7270-tizen_solis_lte-r08.dtb \
                           exynos7270-tizen_solis_lte-r09.dtb \
                           exynos7270-tizen_solis_lte-r10.dtb \
                           exynos7270-tizen_solis_lte-r11.dtb \
                           exynos7270-tizen_solis_lte-r12.dtb \
                           exynos7270-tizen_solis_lte-r13.dtb
else
dtb-$(CONFIG_SOLIS) += exynos7270-tizen_solis-r03.dtb \
                       exynos7270-tizen_solis-r04.dtb \
                       exynos7270-tizen_solis-r05.dtb \
                       exynos7270-tizen_solis-r06.dtb \
                       exynos7270-tizen_solis-r07.dtb \
                       exynos7270-tizen_solis-r08.dtb \
                       exynos7270-tizen_solis-r09.dtb \
                       exynos7270-tizen_solis-r10.dtb \
                       exynos7270-tizen_solis-r11.dtb \
                       exynos7270-tizen_solis-r12.dtb \
                       exynos7270-tizen_solis-r13.dtb
endif
else ifeq ($(CONFIG_POP),y)
dtb-$(CONFIG_POP) += exynos7270-tizen_pop-r00.dtb \
		     exynos7270-tizen_pop-r01.dtb \
		     exynos7270-tizen_pop-r02.dtb \
		     exynos7270-tizen_pop-r03.dtb \
                     exynos7270-tizen_pop-r04.dtb \
                     exynos7270-tizen_pop-r05.dtb \
		     exynos7270-tizen_pop-r06.dtb
else
dtb-$(CONFIG_SOC_EXYNOS7570) += exynos7570-smdk7570.dtb exynos7270-smdk7270.dtb exynos7270-universal7270.dtb exynos7570-universal7570-IOT-rev02.dtb exynos7270-universal7270-4g4g.dtb
endif

targets += dtbs

DTB_NAMES := $(subst $\",,$(CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE_NAMES))
ifneq ($(DTB_NAMES),)
DTB_LIST := $(addsuffix .dtb,$(DTB_NAMES))
else
DTB_LIST := $(dtb-y)
endif
targets += $(DTB_LIST)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := dts/*.dtb *.dtb
