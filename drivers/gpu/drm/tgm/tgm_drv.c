/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 * Authors:
 *	Eunchul Kim <chulspro.kim@samsung.com>
 *	JinYoung Jeon <jy0.jeon@samsung.com>
 *	Taeheon Kim <th908.kim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/module.h>
#include <linux/component.h>
#include <drm/drmP.h>
#include <drm/tgm_drm.h>
#include <tgm_drv.h>

static struct drm_info_list tgm_debugfs_list[] = {
	{"gem_info", tbm_gem_info, DRIVER_GEM},
};
#define TGM_DEBUGFS_ENTRIES ARRAY_SIZE(tgm_debugfs_list)

static DEFINE_MUTEX(tgm_drv_comp_lock);
static LIST_HEAD(tgm_drv_comp_list);

struct component_dev {
	struct list_head list;
	struct device *irq_dev;
};

static LIST_HEAD(tgm_subdrv_list);

int tgm_subdrv_register(struct tgm_subdrv *subdrv)
{
	if (!subdrv)
		return -EINVAL;

	list_add_tail(&subdrv->list, &tgm_subdrv_list);

	return 0;
}
EXPORT_SYMBOL_GPL(tgm_subdrv_register);

int tgm_subdrv_unregister(struct tgm_subdrv *subdrv)
{
	if (!subdrv)
		return -EINVAL;

	list_del(&subdrv->list);

	return 0;
}
EXPORT_SYMBOL_GPL(tgm_subdrv_unregister);

int tgm_device_subdrv_probe(struct drm_device *dev)
{
	struct tgm_subdrv *subdrv, *n;
	int err;

	if (!dev)
		return -EINVAL;

	list_for_each_entry_safe(subdrv, n, &tgm_subdrv_list, list) {
		if (subdrv->probe) {
			subdrv->drm_dev = dev;

			/*
			 * this probe callback would be called by sub driver
			 * after setting of all resources to this sub driver,
			 * such as clock, irq and register map are done.
			 */
			err = subdrv->probe(dev, subdrv->dev);
			if (err) {
				DRM_DEBUG("exynos drm subdrv probe failed.\n");
				list_del(&subdrv->list);
				continue;
			}
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(tgm_device_subdrv_probe);

int tgm_device_subdrv_remove(struct drm_device *dev)
{
	struct tgm_subdrv *subdrv;

	if (!dev) {
		WARN(1, "Unexpected drm device unregister!\n");
		return -EINVAL;
	}

	list_for_each_entry(subdrv, &tgm_subdrv_list, list) {
		if (subdrv->remove)
			subdrv->remove(dev, subdrv->dev);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(tgm_device_subdrv_remove);

int tgm_subdrv_open(struct drm_device *dev, struct drm_file *file)
{
	struct tgm_subdrv *subdrv;
	int ret;

	list_for_each_entry(subdrv, &tgm_subdrv_list, list) {
		if (subdrv->open) {
			ret = subdrv->open(dev, subdrv->dev, file);
			if (ret)
				goto err;
		}
	}

	return 0;

err:
	list_for_each_entry_reverse(subdrv, &subdrv->list, list) {
		if (subdrv->close)
			subdrv->close(dev, subdrv->dev, file);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(tgm_subdrv_open);

void tgm_subdrv_close(struct drm_device *dev, struct drm_file *file)
{
	struct tgm_subdrv *subdrv;

	list_for_each_entry(subdrv, &tgm_subdrv_list, list) {
		if (subdrv->close)
			subdrv->close(dev, subdrv->dev, file);
	}
}
EXPORT_SYMBOL_GPL(tgm_subdrv_close);

static int tgm_drv_load(struct drm_device *drm_dev, unsigned long flags)
{
	struct tgm_drv_private *dev_priv;
	struct drm_minor *minor = drm_dev->primary;
	int ret;

	DRM_DEBUG("%s\n", __func__);

	dev_priv= kzalloc(sizeof(struct tgm_drv_priv*),	GFP_KERNEL);
	if (!dev_priv) {
		DRM_ERROR("failed to alloc dev private data.\n");
		return -ENOMEM;
	}

	drm_dev->dev_private = (void *)dev_priv;

	ret = tdm_init(drm_dev);
	if (ret) {
		DRM_ERROR("failed to init TDM.\n");
		return -EINVAL;
	}

	ret = tbm_init(drm_dev);
	if (ret) {
		DRM_ERROR("failed to init TBM.\n");
		return -EINVAL;
	}

	ret = component_bind_all(drm_dev->dev, drm_dev);
	if (ret) {
		DRM_ERROR("failed to bind component.\n");
		return -EINVAL;
	}

	ret = tgm_device_subdrv_probe(drm_dev);
	if (ret) {
		DRM_ERROR("failed to probe subdrv.\n");
		return -EINVAL;
	}

	ret = drm_debugfs_create_files(tgm_debugfs_list,
			TGM_DEBUGFS_ENTRIES,
			minor->debugfs_root, minor);

	return ret;
}

static int tgm_drv_unload(struct drm_device *drm_dev)
{
	DRM_INFO("%s\n", __func__);

	tgm_device_subdrv_remove(drm_dev);
	component_unbind_all(drm_dev->dev, drm_dev);
	drm_debugfs_remove_files(tgm_debugfs_list,
			TGM_DEBUGFS_ENTRIES, drm_dev->primary);

	return 0;
}

static int tgm_drv_open(struct drm_device *dev, struct drm_file *file)
{
	struct tgm_drv_file_private *file_priv;

	DRM_DEBUG("%s\n", __func__);

	file_priv = kzalloc(sizeof(*file_priv), GFP_KERNEL);
	if (!file_priv)
		return -ENOMEM;

	file->driver_priv = file_priv;

	return tgm_subdrv_open(dev, file);
}

static void tgm_drv_preclose(struct drm_device *dev,
					struct drm_file *file)
{
	DRM_DEBUG("%s\n", __func__);

	tgm_subdrv_close(dev, file);
}

static void tgm_drv_postclose(struct drm_device *dev, struct drm_file *file)
{
	DRM_DEBUG("%s\n", __func__);

	if (!file->driver_priv)
		return;

	kfree(file->driver_priv);
	file->driver_priv = NULL;
}

static void tgm_drv_lastclose(struct drm_device *dev)
{
	DRM_INFO("%s\n", __func__);
}

static struct drm_ioctl_desc tgm_ioctls[] = {
	DRM_IOCTL_DEF_DRV(TBM_GEM_CREATE, tbm_gem_create_ioctl,
			DRM_UNLOCKED | DRM_AUTH),
	DRM_IOCTL_DEF_DRV(TBM_GEM_MMAP,
			tbm_gem_mmap_ioctl, DRM_UNLOCKED | DRM_AUTH),
	DRM_IOCTL_DEF_DRV(TBM_GEM_GET,
			tbm_gem_get_ioctl, DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(TBM_GEM_CPU_PREP, tbm_gem_cpu_prep_ioctl,
			DRM_UNLOCKED | DRM_AUTH),
	DRM_IOCTL_DEF_DRV(TBM_GEM_CPU_FINI, tbm_gem_cpu_fini_ioctl,
			DRM_UNLOCKED | DRM_AUTH),
#ifdef CONFIG_DRM_TDM_PP
	DRM_IOCTL_DEF_DRV(TDM_PP_GET_PROPERTY,
			tdm_pp_get_property, DRM_UNLOCKED | DRM_AUTH),
	DRM_IOCTL_DEF_DRV(TDM_PP_SET_PROPERTY,
			tdm_pp_set_property, DRM_UNLOCKED | DRM_AUTH),
	DRM_IOCTL_DEF_DRV(TDM_PP_QUEUE_BUF,
			tdm_pp_queue_buf, DRM_UNLOCKED | DRM_AUTH),
	DRM_IOCTL_DEF_DRV(TDM_PP_CMD_CTRL,
			tdm_pp_cmd_ctrl, DRM_UNLOCKED | DRM_AUTH),
	DRM_IOCTL_DEF_DRV(TDM_PP_GET_PERMISSION,
			tdm_pp_get_permission, DRM_UNLOCKED | DRM_AUTH),

#endif
#ifdef CONFIG_DRM_TDM_DPMS_CTRL
	DRM_IOCTL_DEF_DRV(TDM_DPMS_CONTROL, tdm_dpms_ioctl,
			DRM_MASTER),
#endif
};

static const struct file_operations tgm_drv_fops = {
	.owner = THIS_MODULE,
	.open = drm_open,
	.mmap = drm_gem_mmap,
	.poll = drm_poll,
	.read = drm_read,
	.unlocked_ioctl = drm_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = drm_compat_ioctl,
#endif
	.release = drm_release,
};

static struct drm_driver tgm_drm_driver = {
	.driver_features = DRIVER_GEM | DRIVER_PRIME,
	.load = tgm_drv_load,
	.unload = tgm_drv_unload,
	.open = tgm_drv_open,
	.preclose = tgm_drv_preclose,
	.postclose = tgm_drv_postclose,
	.lastclose = tgm_drv_lastclose,
	.set_busid = drm_platform_set_busid,
	.ioctls = tgm_ioctls,
	.fops = &tgm_drv_fops,
};

#ifdef CONFIG_OF
static int tgm_drv_of_match(struct device_node *node,
		struct drm_driver *tgm_drv)
{
	int ret;

	ret = of_property_read_string(node, "tgm,drv_name",
		(const char**)&tgm_drv->name);
	if (ret) {
		DRM_ERROR("failed to read driver name.\n");
		return -EINVAL;
	}

	ret = of_property_read_string(node, "tgm,drv_desc",
		(const char**)&tgm_drv->desc);
	if (ret) {
		DRM_ERROR("failed to read driver description.\n");
		return -EINVAL;
	}

	ret = of_property_read_string(node, "tgm,drv_date",
		(const char**)&tgm_drv->date);
	if (ret) {
		DRM_ERROR("failed to read driver date.\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(node, "tgm,drv_major", &tgm_drv->major);
	if (ret) {
		DRM_ERROR("failed to read major vertion.\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(node, "tgm,drv_minor", &tgm_drv->minor);
	if (ret) {
		DRM_ERROR("failed to read minor vertion.\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(node, "tgm,drv_patchlevel",
		&tgm_drv->patchlevel);
	if (ret) {
		DRM_ERROR("failed to read patch level.\n");
		return -EINVAL;
	}

	DRM_DEBUG("%s:name[%s]desc[%s]data[%s]"
		"major[%d]minor[%d]patchlevel[%d]\n",
		__func__, tgm_drv->name, tgm_drv->desc, tgm_drv->date,
		tgm_drv->major, tgm_drv->minor, tgm_drv->patchlevel);

	return 0;
}
#endif

int tgm_drv_component_add(struct device *dev,
		enum tgm_drv_dev_type dev_type)
{
	struct component_dev *cdev;

	DRM_DEBUG("%s:dev_type[%d]\n", __func__, dev_type);

	if (dev_type != TGM_DRV_DEV_TYPE_IRQ) {
		DRM_ERROR("invalid device type.\n");
		return -EINVAL;
	}

	cdev = kzalloc(sizeof(*cdev), GFP_KERNEL);
	if (!cdev)
		return -ENOMEM;

	if (dev_type == TGM_DRV_DEV_TYPE_IRQ)
		cdev->irq_dev = dev;

	mutex_lock(&tgm_drv_comp_lock);
	list_add_tail(&cdev->list, &tgm_drv_comp_list);
	mutex_unlock(&tgm_drv_comp_lock);

	return 0;
}

void tgm_drv_component_del(struct device *dev,
		enum tgm_drv_dev_type dev_type)
{
	struct component_dev *cdev, *next;

	DRM_DEBUG("%s\n", __func__);

	mutex_lock(&tgm_drv_comp_lock);

	list_for_each_entry_safe(cdev, next, &tgm_drv_comp_list, list) {
		if (dev_type == TGM_DRV_DEV_TYPE_IRQ) {
			if (cdev->irq_dev == dev)
				cdev->irq_dev = NULL;
		}

		if (!cdev->irq_dev) {
			list_del(&cdev->list);
			kfree(cdev);
		}

		break;
	}

	mutex_unlock(&tgm_drv_comp_lock);
}

static int compare_dev(struct device *dev, void *data)
{
	DRM_DEBUG("%s\n", __func__);

	return dev == (struct device *)data;
}

static struct component_match *tgm_drv_match_add(struct device *dev)
{
	struct component_match *match = NULL;
	struct component_dev *cdev;
	unsigned int attach_cnt = 0;

	DRM_DEBUG("%s\n", __func__);

	mutex_lock(&tgm_drv_comp_lock);

	if (list_empty(&tgm_drv_comp_list)) {
		mutex_unlock(&tgm_drv_comp_lock);
		return ERR_PTR(-ENODEV);
	}

	list_for_each_entry(cdev, &tgm_drv_comp_list, list) {
		if (!cdev->irq_dev)
			continue;

		attach_cnt++;

		mutex_unlock(&tgm_drv_comp_lock);
		component_match_add(dev, &match, compare_dev, cdev->irq_dev);
		DRM_DEBUG("%s:attach_cnt[%d]match[%p]irq_dev[%p]\n",
			__func__, attach_cnt, match, cdev->irq_dev);
		mutex_lock(&tgm_drv_comp_lock);
	}

	mutex_unlock(&tgm_drv_comp_lock);

	DRM_DEBUG("%s:match[%p]\n", __func__, match);

	return attach_cnt ? match : ERR_PTR(-EPROBE_DEFER);
}

static int tgm_drv_bind(struct device *dev)
{
	DRM_DEBUG("%s\n", __func__);

	return drm_platform_init(&tgm_drm_driver, to_platform_device(dev));
}

static void tgm_drv_unbind(struct device *dev)
{
	DRM_DEBUG("%s\n", __func__);
	drm_put_dev(dev_get_drvdata(dev));
}

static const struct component_master_ops tgm_drv_ops = {
	.bind = tgm_drv_bind,
	.unbind = tgm_drv_unbind,
};

/* FIXME: should be moved component to irq */
static int tgm_irq_bind(struct device *dev, struct device *master, void *data)
{
	DRM_DEBUG("%s\n", __func__);
	return 0;
}

static void tgm_irq_unbind(struct device *dev, struct device *master,
			void *data)
{
	DRM_DEBUG("%s\n", __func__);
}

static const struct component_ops tgm_irq_component_ops = {
	.bind	= tgm_irq_bind,
	.unbind = tgm_irq_unbind,
};

static int tgm_drv_probe(struct platform_device *pdev)
{
#ifdef CONFIG_OF
	struct device_node *node = pdev->dev.of_node;
	int ret;
#endif
	struct component_match *match;

	DRM_DEBUG("%s\n", __func__);

	tgm_drm_driver.num_ioctls = ARRAY_SIZE(tgm_ioctls);

#ifdef CONFIG_OF
	ret = tgm_drv_of_match(node, &tgm_drm_driver);
	if (ret) {
		DRM_ERROR("failed to match tgm driver.\n");
		return -EINVAL;
	}
#endif

	/* FIXME: should be moved component to irq */
	tgm_drv_component_add(&pdev->dev, TGM_DRV_DEV_TYPE_IRQ);
	component_add(&pdev->dev, &tgm_irq_component_ops);

	match = tgm_drv_match_add(&pdev->dev);
	if (IS_ERR(match)) {
		ret = PTR_ERR(match);
		DRM_ERROR("failed to match add.\n");
		goto err_out;
	}

	ret = component_master_add_with_match(&pdev->dev, &tgm_drv_ops,
						match);
	if (ret < 0) {
		DRM_ERROR("failed to master add with match.\n");
		goto err_component;
	}

	DRM_INFO("%s:probed\n", __func__);

	return 0;

err_component:
	component_master_del(&pdev->dev, &tgm_drv_ops);
err_out:
	return ret;
}

static int tgm_drv_remove(struct platform_device *pdev)
{
	DRM_INFO("%s\n", __func__);

	/* FIXME: should be moved component to irq */
	tgm_drv_component_del(&pdev->dev, TGM_DRV_DEV_TYPE_IRQ);
	component_del(&pdev->dev, &tgm_irq_component_ops);

	component_master_del(&pdev->dev, &tgm_drv_ops);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id tgm_drv_dt_match[] = {
	{ .compatible = "drm,tgm_drv",},
	{}
};
MODULE_DEVICE_TABLE(of, tgm_drv_dt_match);
#endif

#ifdef CONFIG_PM_SLEEP
static int tgm_resume(struct device *dev)
{
#ifdef CONFIG_DISPLAY_EARLY_DPMS
	struct display_early_dpms_nb_event event;

	DRM_INFO("%s\n", __func__);

	event.id = DISPLAY_EARLY_DPMS_ID_PRIMARY;
	event.data = (void *)true;

	display_early_dpms_nb_send_event(DISPLAY_EARLY_DPMS_VBLANK_SET,
		(void *)&event);
#endif

	return 0;
}

static int tgm_suspend(struct device *dev)
{
#ifdef CONFIG_DISPLAY_EARLY_DPMS
	struct display_early_dpms_nb_event event;

	DRM_INFO("%s\n", __func__);

	event.id = DISPLAY_EARLY_DPMS_ID_PRIMARY;
	event.data = (void *)false;

	display_early_dpms_nb_send_event(DISPLAY_EARLY_DPMS_VBLANK_SET,
		(void *)&event);
#endif

	return 0;
}
#endif

static const struct dev_pm_ops tgm_pm_ops = {
#ifdef CONFIG_PM_SLEEP
	.suspend = tgm_suspend,
	.resume	 = tgm_resume,
#endif
};

static struct platform_driver tgm_driver = {
	.probe		= tgm_drv_probe,
	.remove		= tgm_drv_remove,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "tgm-drm",
		.pm	= &tgm_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(tgm_drv_dt_match),
#endif
	},
};

static int __init tgm_drv_init(void)
{
	int ret;

	DRM_DEBUG("%s\n", __func__);

#ifdef CONFIG_DRM_TDM_PP_MSC
	ret = platform_driver_register(&pp_msc_driver);
	if (ret < 0)
		return ret;
#endif
#ifdef CONFIG_DRM_TDM_PP
	ret = platform_driver_register(&pp_driver);
	if (ret < 0)
		goto out_pp_driver;
#endif

	ret = platform_driver_register(&tgm_driver);
	if (ret)
		goto out_tgm_drv;

	return 0;

out_tgm_drv:
#ifdef CONFIG_DRM_TDM_PP
	platform_driver_unregister(&pp_driver);
out_pp_driver:
#endif
#ifdef CONFIG_DRM_TDM_PP_MSC
	platform_driver_unregister(&pp_msc_driver);
#endif
	return ret;
}

static void __exit tgm_drv_exit(void)
{
	DRM_INFO("%s\n", __func__);

	platform_driver_unregister(&tgm_driver);

#ifdef CONFIG_DRM_TDM_PP
	platform_driver_unregister(&pp_driver);
#endif

#ifdef CONFIG_DRM_TDM_PP_MSC
	platform_driver_unregister(&pp_msc_driver);
#endif
}


late_initcall(tgm_drv_init);
module_exit(tgm_drv_exit);

MODULE_AUTHOR("Eunchul Kim <chulspro.kim@samsung.com>");
MODULE_AUTHOR("JinYoung Jeon <jy0.jeon@samsung.com>");
MODULE_AUTHOR("Taeheon Kim <th908.kim@samsung.com>");
MODULE_DESCRIPTION("Tizen Graphics Manager");
MODULE_LICENSE("GPL");
