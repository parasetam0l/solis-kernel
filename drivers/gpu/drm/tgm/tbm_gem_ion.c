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
#include <tbm_gem_ion.h>

extern struct ion_device *ion_exynos;

 int tbm_gem_ion_buffer_alloc(struct drm_device *dev,
		unsigned int flags, struct tbm_gem_buf *buf)
{
	struct scatterlist *sg = NULL;
	struct tgm_drv_private *dev_priv = dev->dev_private;
	struct tbm_private *tbm_priv = dev_priv->tbm_priv;
	struct gem_private *gem_priv = tbm_priv->gem_priv;
	unsigned int nr_pages = 0, i = 0, heap_id_mask;
	unsigned long sgt_size = 0;
	int ret = 0, mem_flags = 0;

	DRM_DEBUG("%s\n", __func__);

	if (buf->dma_addr) {
		DRM_DEBUG_KMS("already allocated.\n");
		return 0;
	}

	buf->page_size = buf->size;

	if (IS_TBM_NONCONTIG_BUF(flags))
		heap_id_mask = ION_HEAP_SYSTEM_MASK;
	else
		heap_id_mask = ION_HEAP_SYSTEM_CONTIG_MASK;

	if (IS_TBM_CACHABLE_BUF(flags))
		mem_flags = ION_FLAG_CACHED;

	buf->ion_handle = ion_alloc(gem_priv->tbm_ion_client, buf->size,
			SZ_4K, heap_id_mask, mem_flags);
	if (IS_ERR((void *)buf->ion_handle)) {
		DRM_ERROR("%s Could not allocate\n", __func__);
		return -ENOMEM;
	}

	ret = ion_set_client_ops(gem_priv->tbm_ion_client, buf->ion_handle,
			tbm_gem_object_unreference, buf->obj);
	if (ret) {
		DRM_ERROR("failed to ion_set_client_ops\n");
		goto err;
	}

	buf->sgt = ion_sg_table(gem_priv->tbm_ion_client, buf->ion_handle);
	if (!buf->sgt) {
		DRM_ERROR("failed to get sg table.\n");
		ret = -ENOMEM;
		goto err;
	}

	buf->dma_addr = sg_dma_address(buf->sgt->sgl);
	if (!buf->dma_addr) {
		DRM_ERROR("failed to get dma addr.\n");
		ret = -EINVAL;
		goto err;
	}

	for_each_sg(buf->sgt->sgl, sg, buf->sgt->nents, i)
		nr_pages++;

	sgt_size = sizeof(struct page) * nr_pages;
	buf->pages = kzalloc(sgt_size, GFP_KERNEL | __GFP_NOWARN | __GFP_NORETRY);
	if (!buf->pages) {
		unsigned int order;
		order = get_order(sgt_size);
		DRM_INFO("%s:sglist kzalloc failed: order:%d, trying vzalloc\n",
					__func__, order);
		buf->pages = vzalloc(sgt_size);
		if (!buf->pages) {
			DRM_ERROR("failed to allocate pages.\n");
			ret = -ENOMEM;
			goto err;
		}
	}

	for_each_sg(buf->sgt->sgl, sg, buf->sgt->nents, i) {
		buf->pages[i] = phys_to_page(sg_dma_address(sg));
		buf->page_size = sg->length;
        }

	DRM_DEBUG("%s:dma_addr[0x%x]size[%d]\n",
		__func__, (int)buf->dma_addr, (int)buf->size);

	return ret;
err:
	ion_free(gem_priv->tbm_ion_client, buf->ion_handle);
	buf->dma_addr = (dma_addr_t)NULL;
	buf->sgt = NULL;

	return ret;
}

 void tbm_gem_ion_buffer_dealloc(struct drm_device *dev,
		unsigned int flags, struct tbm_gem_buf *buf)
{
	struct tgm_drv_private *dev_priv = dev->dev_private;
	struct tbm_private *tbm_priv = dev_priv->tbm_priv;
	struct gem_private *gem_priv = tbm_priv->gem_priv;

	DRM_DEBUG("%s\n", __func__);

	if (is_vmalloc_addr(buf->pages))
		vfree(buf->pages);
	else
		kfree(buf->pages);
	buf->pages = NULL;

	ion_free(gem_priv->tbm_ion_client, buf->ion_handle);

	buf->dma_addr = (dma_addr_t)NULL;
	buf->sgt = NULL;
}

void *tbm_gem_ion_get_dma_addr(struct drm_device *drm_dev,
		struct device *dev, unsigned int gem_handle,
		struct drm_file *filp)
{
	struct tbm_gem_object *tbm_gem_obj;
	struct drm_gem_object *obj;
	struct tbm_gem_buf	*buffer;

	obj = drm_gem_object_lookup(drm_dev, filp, gem_handle);
	if (!obj) {
		DRM_ERROR("failed to lookup gem object.\n");
		return ERR_PTR(-EINVAL);
	}

	tbm_gem_obj = to_tbm_gem_obj(obj);
	buffer = tbm_gem_obj->buffer;
	buffer->iova =  iovmm_map(dev, buffer->sgt->sgl, 0, buffer->size,
		DMA_TO_DEVICE, 0);

	DRM_DEBUG("%s:h[%d]obj[%p]a[0x%x]\n", __func__, gem_handle,
		obj, (int)buffer->iova);

	return &buffer->iova;
}

void tbm_gem_ion_put_dma_addr(struct drm_device *drm_dev,
		struct device *dev, unsigned int gem_handle,
		struct drm_file *filp)
{
	struct tbm_gem_object *tbm_gem_obj;
	struct drm_gem_object *obj;
	struct tbm_gem_buf	*buffer;

	obj = drm_gem_object_lookup(drm_dev, filp, gem_handle);
	if (!obj) {
		DRM_ERROR("failed to lookup gem object.\n");
		return;
	}

	tbm_gem_obj = to_tbm_gem_obj(obj);
	buffer = tbm_gem_obj->buffer;

	DRM_DEBUG("%s:h[%d]obj[%p]a[0x%x]\n", __func__, gem_handle,
		obj, (int)buffer->iova);

	iovmm_unmap(dev, buffer->iova);
	drm_gem_object_unreference_unlocked(obj);

	/*
	 * decrease obj->refcount one more time because we has already
	 * increased it at exynos_drm_gem_get_dma_addr().
	 */
	drm_gem_object_unreference_unlocked(obj);
}

void *tbm_gem_ion_get_dma_buf(struct drm_device *drm_dev,
		struct device *dev, unsigned int gem_handle,
		struct drm_file *filp)
{
	struct drm_gem_object *obj;

	obj = drm_gem_object_lookup(drm_dev, filp, gem_handle);
	if (!obj) {
		DRM_ERROR("failed to lookup gem object.\n");
		return ERR_PTR(-EINVAL);
	}

	DRM_DEBUG("%s:h[%d]obj[%p]buf[%p]\n", __func__, gem_handle,
		obj, obj->dma_buf);
	drm_gem_object_unreference_unlocked(obj);

	return obj->dma_buf;
}

struct dma_buf *tbm_gem_ion_prime_export(struct drm_device *dev,
				  struct drm_gem_object *obj, int flags)
{
	struct tgm_drv_private *dev_priv = dev->dev_private;
	struct tbm_private *tbm_priv = dev_priv->tbm_priv;
	struct gem_private *gem_priv = tbm_priv->gem_priv;
	struct tbm_gem_object *tbm_gem_obj = to_tbm_gem_obj(obj);
	struct tbm_gem_buf *buf;
	struct dma_buf *dmabuf = NULL;

	mutex_lock(&dev->struct_mutex);
	drm_gem_object_reference(obj);

	if (!tbm_gem_obj) {
		DRM_ERROR("failed to get tbm_gem_object:obj[%p]\n", obj);
		goto out;
	}

	buf = tbm_gem_obj->buffer;
	if (!buf) {
		DRM_ERROR("failed to get tbm_gem_buf:obj[%p]\n", obj);
		goto out;
	}

	if (!buf->ion_handle) {
		DRM_ERROR("failed to get ion_handle:obj[%p]\n", obj);
		goto out;
	}

	dmabuf = ion_share_dma_buf(gem_priv->tbm_ion_client,
				   buf->ion_handle);
	if (IS_ERR(dmabuf))
		DRM_ERROR("dmabuf is error and dmabuf is %p!\n", dmabuf);

out:
	drm_gem_object_unreference(obj);
	mutex_unlock(&dev->struct_mutex);

	return dmabuf;
}

struct drm_gem_object *tbm_gem_ion_prime_import(struct drm_device *dev,
					 struct dma_buf *dma_buf)
{
	struct tgm_drv_private *dev_priv = dev->dev_private;
	struct tbm_private *tbm_priv = dev_priv->tbm_priv;
	struct gem_private *gem_priv = tbm_priv->gem_priv;
	struct tbm_gem_object *tbm_gem_obj;
	struct tbm_gem_buf *buf;
	struct drm_gem_object *obj;
	struct ion_handle *ion_handle;
	struct scatterlist *sg = NULL;
	unsigned long size, sgt_size;
	unsigned int i = 0, nr_pages = 0, heap_id;
	int ret = 0;

	ion_handle = get_ion_handle_from_dmabuf(gem_priv->tbm_ion_client,
			dma_buf);
	if (IS_ERR_OR_NULL(ion_handle)) {
		DRM_ERROR("Unable to import dmabuf\n");
		return ERR_PTR(-EINVAL);
	}

	ion_handle_get_size(gem_priv->tbm_ion_client,
					ion_handle, &size, &heap_id);
	if (size == 0) {
		DRM_ERROR(
			"cannot create GEM object from zero size ION buffer\n");
		ret = -EINVAL;
		goto err;
	}

	obj = ion_get_client_object(gem_priv->tbm_ion_client, ion_handle);
	if (obj) {
		tbm_gem_obj = to_tbm_gem_obj(obj);
		if (tbm_gem_obj->buffer->ion_handle != ion_handle) {
			DRM_ERROR("Unable get GEM object from ion\n");
			ret = -EINVAL;
			goto err;
		}

		drm_gem_object_reference(obj);
		ion_free(gem_priv->tbm_ion_client, ion_handle);
		goto out;
	}

	buf = tbm_init_buf(dev, size);
	if (!buf) {
		DRM_ERROR("Unable to allocate the GEM buffer\n");
		ret = -ENOMEM;
		goto err;
	}

	tbm_gem_obj = tbm_gem_obj_init(dev, size);
	if (!tbm_gem_obj) {
		DRM_ERROR("Unable to initialize GEM object\n");
		ret = -ENOMEM;
		goto err_fini_buf;
	}

	tbm_gem_obj->buffer = buf;
	tbm_gem_obj->flags = TBM_BO_NONCONTIG;
	if (ion_is_cached(gem_priv->tbm_ion_client, ion_handle))
		tbm_gem_obj->flags |= TBM_BO_CACHABLE;
	obj = &tbm_gem_obj->base;

	buf->ion_handle = ion_handle;
	buf->sgt = ion_sg_table(gem_priv->tbm_ion_client, buf->ion_handle);
	if (!buf->sgt) {
		DRM_ERROR("failed to get sg table.\n");
		ret = -ENOMEM;
		goto err_gem_obj;
	}

	buf->dma_addr = sg_dma_address(buf->sgt->sgl);
	if (!buf->dma_addr) {
		DRM_ERROR("failed to get dma addr.\n");
		ret = -EINVAL;
		goto err_gem_obj;
	}

	for_each_sg(buf->sgt->sgl, sg, buf->sgt->nents, i)
		nr_pages++;

	sgt_size = sizeof(struct page) * nr_pages;
	buf->pages = kzalloc(sgt_size, GFP_KERNEL | __GFP_NOWARN |
				__GFP_NORETRY);
	if (!buf->pages) {
		unsigned int order;

		order = get_order(sgt_size);
		DRM_INFO("%s:order:%d, trying vzalloc\n",
					__func__, order);
		buf->pages = vzalloc(sgt_size);
		if (!buf->pages) {
			DRM_ERROR("failed to allocate pages.\n");
			ret = -ENOMEM;
			goto err_buf;
		}
	}

	for_each_sg(buf->sgt->sgl, sg, buf->sgt->nents, i) {
		buf->pages[i] = phys_to_page(sg_dma_address(sg));
		buf->page_size = sg->length;
	}

out:
	return obj;
err_buf:
	buf->dma_addr = (dma_addr_t)NULL;
	buf->sgt = NULL;
err_gem_obj:
	tbm_gem_obj->buffer = NULL;
	/* release file pointer to gem object. */
	drm_gem_object_release(obj);
	kfree(tbm_gem_obj);
	tbm_gem_obj = NULL;
err_fini_buf:
	tbm_fini_buf(dev, buf);
err:
	ion_free(gem_priv->tbm_ion_client, ion_handle);

	return ERR_PTR(ret);
}

 int tbm_gem_ion_init(struct drm_device *drm_dev)
{
	struct tgm_drv_private *dev_priv = drm_dev->dev_private;
	struct drm_driver *drm_drv = drm_dev->driver;
	struct tbm_private *tbm_priv = dev_priv->tbm_priv;
	struct gem_private *gem_priv;

	drm_drv->gem_prime_export = tbm_gem_ion_prime_export,
	drm_drv->gem_prime_import = tbm_gem_ion_prime_import,

	gem_priv = kzalloc(sizeof(*gem_priv), GFP_KERNEL);
	if (!gem_priv) {
		DRM_ERROR("failed to alloc gem dev private data.\n");
		return -ENOMEM;
	}

	gem_priv->tbm_ion_client = ion_client_create(ion_exynos, "tbm");
	if (!gem_priv->tbm_ion_client) {
		DRM_ERROR("failed to create tbm_ion_client\n");
		return -ENOMEM;
	}

	tbm_priv->gem_priv = gem_priv;
	tbm_priv->gem_bufer_alloc = tbm_gem_ion_buffer_alloc;
	tbm_priv->gem_bufer_dealloc = tbm_gem_ion_buffer_dealloc;
	tbm_priv->gem_get_dma_addr = tbm_gem_ion_get_dma_addr;
	tbm_priv->gem_put_dma_addr = tbm_gem_ion_put_dma_addr;
#ifdef CONFIG_DRM_DMA_SYNC
	tbm_priv->gem_get_dma_buf = tbm_gem_ion_get_dma_buf;
#endif

	return 0;
}
