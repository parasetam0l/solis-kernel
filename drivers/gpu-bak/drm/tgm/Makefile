#
# Makefile for the TGM driver.
#

ccflags-y := -Iinclude/drm -Idrivers/gpu/drm/tgm
tgm-y := tgm_drv.o

tgm-$(CONFIG_DRM_TDM) += tdm.o
tgm-$(CONFIG_DRM_TDM_IRQ) += tdm_irq.o
tgm-$(CONFIG_DRM_TDM_IRQ_EXYNOS) += tdm_irq_exynos.o
tgm-$(CONFIG_DRM_TDM_PP) += tdm_pp.o
tgm-$(CONFIG_DRM_TDM_PP_MSC) += tdm_pp_msc.o
tgm-$(CONFIG_DRM_TBM) += tbm.o
tgm-$(CONFIG_DRM_TBM_GEM) += tbm_gem.o
tgm-$(CONFIG_DRM_TBM_GEM_DMA_MAPPING) += tbm_gem_dma_mapping.o
tgm-$(CONFIG_DRM_TBM_GEM_ION) += tbm_gem_ion.o

obj-$(CONFIG_DRM_TGM) += tgm.o
