/*
 * Samsung Exynos4 SoC series FIMC-IS slave interface driver
 *
 * v4l2 subdev driver interface
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 * Contact: Younghwan Joo, <yhwan.joo@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/cma.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <media/videobuf2-core.h>
#include <asm/cacheflush.h>
#include <media/videobuf2-cma-phys.h>
#if defined(CONFIG_VIDEOBUF2_ION)
#include <media/videobuf2-ion.h>
#endif
#include "fimc-is-core.h"
#include "fimc-is-param.h"

#if defined(CONFIG_VIDEOBUF2_ION)
#define	FIMC_IS_ION_NAME	"exynos4-fimc-is"
#define FIMC_IS_FW_BASE_MASK		((1 << 26) - 1)

void *is_vb_cookie;
void *buf_start;
#endif

#if defined(CONFIG_VIDEOBUF2_CMA_PHYS)
void fimc_is_mem_cache_clean(const void *start_addr, unsigned long size)
{
	unsigned long paddr;

	dmac_map_area(start_addr, size, DMA_TO_DEVICE);
	/*
	 * virtual & phsical addrees mapped directly, so we can convert
	 * the address just using offset
	 */
	paddr = __pa((unsigned long)start_addr);
	outer_clean_range(paddr, paddr + size);
}

void fimc_is_mem_cache_inv(const void *start_addr, unsigned long size)
{
	unsigned long paddr;
	paddr = __pa((unsigned long)start_addr);
	outer_inv_range(paddr, paddr + size);
	dmac_unmap_area(start_addr, size, DMA_FROM_DEVICE);
}

int fimc_is_init_mem_mgr(struct fimc_is_dev *dev)
{
	struct cma_info mem_info;
	int err;
	unsigned char *membase;

	/* Alloc FW memory */
	err = cma_info(&mem_info, &dev->pdev->dev, FIMC_IS_MEM_FW);
	if (err) {
		dev_err(&dev->pdev->dev, "%s: get cma info failed\n", __func__);
		return -EINVAL;
	}
	printk(KERN_INFO "%s : [cma_info] start_addr : 0x%x, end_addr : 0x%x, "
			"total_size : 0x%x, free_size : 0x%x\n",
			__func__, mem_info.lower_bound, mem_info.upper_bound,
			mem_info.total_size, mem_info.free_size);
	dev->mem.size = mem_info.total_size;
	dev->mem.base = (dma_addr_t)cma_alloc
		(&dev->pdev->dev, FIMC_IS_MEM_FW, (size_t)dev->mem.size, 0);
	dev->is_p_region =
		(struct is_region *)(phys_to_virt(dev->mem.base +
				FIMC_IS_A5_MEM_SIZE - FIMC_IS_REGION_SIZE));
	dev->is_shared_region =
		(struct is_share_region *)(phys_to_virt(dev->mem.base +
				FIMC_IS_SHARED_REGION_ADDR));

	membase = (unsigned char *)(phys_to_virt(dev->mem.base));
	memset(membase, 0x0, FIMC_IS_A5_MEM_SIZE);

	memset((void *)dev->is_p_region, 0,
		(unsigned long)sizeof(struct is_region));
	fimc_is_mem_cache_clean((void *)dev->is_p_region, IS_PARAM_SIZE);
	printk(KERN_INFO "ctrl->mem.base = 0x%x\n", dev->mem.base);
	printk(KERN_INFO "ctrl->mem.size = 0x%x\n", dev->mem.size);

	if (dev->mem.size >= (FIMC_IS_A5_MEM_SIZE + FIMC_IS_EXTRA_MEM_SIZE)) {
		dev->mem.fw_ref_base =
				dev->mem.base + FIMC_IS_A5_MEM_SIZE + 0x1000;
		dev->mem.setfile_ref_base =
				dev->mem.base + FIMC_IS_A5_MEM_SIZE + 0x1000
						+ FIMC_IS_EXTRA_FW_SIZE;
		printk(KERN_INFO "ctrl->mem.fw_ref_base = 0x%x\n",
							dev->mem.fw_ref_base);
		printk(KERN_INFO "ctrl->mem.setfile_ref_base = 0x%x\n",
						dev->mem.setfile_ref_base);
	} else {
		dev->mem.fw_ref_base = 0;
		dev->mem.setfile_ref_base = 0;
	}
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS_BAYER
	err = cma_info(&mem_info, &dev->pdev->dev, FIMC_IS_MEM_ISP_BUF);
	printk(KERN_INFO "%s : [cma_info] start_addr : 0x%x, end_addr : 0x%x, "
			"total_size : 0x%x, free_size : 0x%x\n",
			__func__, mem_info.lower_bound, mem_info.upper_bound,
			mem_info.total_size, mem_info.free_size);
	if (err) {
		dev_err(&dev->pdev->dev, "%s: get cma info failed\n", __func__);
		return -EINVAL;
	}
	dev->alloc_ctx = dev->vb2->init(dev);
	if (IS_ERR(dev->alloc_ctx))
		return PTR_ERR(dev->alloc_ctx);
#endif
	return 0;
}

#elif defined(CONFIG_VIDEOBUF2_ION)
struct vb2_mem_ops *fimc_is_mem_ops(void)
{
	return (struct vb2_mem_ops *)&vb2_ion_memops;
}

void *fimc_is_mem_init(struct device *dev)
{
	return vb2_ion_create_context(dev, SZ_4K,
			VB2ION_CTX_IOMMU | VB2ION_CTX_VMCONTIG);
}

void fimc_is_mem_init_mem_cleanup(void *alloc_ctxes)
{
	vb2_ion_destroy_context(alloc_ctxes);
}

void fimc_is_mem_resume(void *alloc_ctxes)
{
	vb2_ion_attach_iommu(alloc_ctxes);
}

void fimc_is_mem_suspend(void *alloc_ctxes)
{
	vb2_ion_detach_iommu(alloc_ctxes);
}

int fimc_is_cache_flush(struct fimc_is_dev *dev,
				const void *start_addr, unsigned long size)
{
	vb2_ion_sync_for_device(dev->mem.fw_cookie,
			(unsigned long)start_addr - (unsigned long)dev->mem.kvaddr,
			size, DMA_BIDIRECTIONAL);
	return 0;
}

/* Allocate firmware */
int fimc_is_alloc_firmware(struct fimc_is_dev *dev)
{
	void *fimc_is_bitproc_buf;
	int ret;

	dbg("Allocating memory for FIMC-IS firmware.\n");
	fimc_is_bitproc_buf =
		vb2_ion_private_alloc(dev->alloc_ctx, FIMC_IS_A5_MEM_SIZE);
	if (IS_ERR(fimc_is_bitproc_buf)) {
		fimc_is_bitproc_buf = 0;
		printk(KERN_ERR "Allocating bitprocessor buffer failed\n");
		return -ENOMEM;
	}

	ret = vb2_ion_dma_address(fimc_is_bitproc_buf, &dev->mem.dvaddr);
	if ((ret < 0) || (dev->mem.dvaddr & FIMC_IS_FW_BASE_MASK)) {
		err("The base memory is not aligned to 64MB.\n");
		vb2_ion_private_free(fimc_is_bitproc_buf);
		dev->mem.dvaddr = 0;
		fimc_is_bitproc_buf = 0;
		return -EIO;
	}
	dbg("FIMC Device vaddr = %08x , size = %08x\n",
				dev->mem.dvaddr, FIMC_IS_A5_MEM_SIZE);

	dev->mem.kvaddr = vb2_ion_private_vaddr(fimc_is_bitproc_buf);
	if (IS_ERR(dev->mem.kvaddr)) {
		err("Bitprocessor memory remap failed\n");
		vb2_ion_private_free(fimc_is_bitproc_buf);
		dev->mem.dvaddr = 0;
		fimc_is_bitproc_buf = 0;
		return -EIO;
	}
	dbg("Virtual address for FW: %08lx\n",
			(long unsigned int)dev->mem.kvaddr);
	dbg("Physical address for FW: %08lx\n",
			(long unsigned int)virt_to_phys(dev->mem.kvaddr));
	dev->mem.bitproc_buf = fimc_is_bitproc_buf;
	dev->mem.fw_cookie = fimc_is_bitproc_buf;

	is_vb_cookie = dev->mem.fw_cookie;
	buf_start = dev->mem.kvaddr;
	return 0;
}

void fimc_is_mem_cache_clean(const void *start_addr, unsigned long size)
{
	off_t offset;

	if (start_addr < buf_start) {
		err("Start address error\n");
		return;
	}
	size--;

	offset = start_addr - buf_start;

	vb2_ion_sync_for_device(is_vb_cookie, offset, size, DMA_TO_DEVICE);
}

void fimc_is_mem_cache_inv(const void *start_addr, unsigned long size)
{
	off_t offset;

	if (start_addr < buf_start) {
		err("Start address error\n");
		return;
	}

	offset = start_addr - buf_start;

	vb2_ion_sync_for_device(is_vb_cookie, offset, size, DMA_FROM_DEVICE);
}

int fimc_is_init_mem_mgr(struct fimc_is_dev *dev)
{
	int ret;
	dev->alloc_ctx = (struct vb2_alloc_ctx *)
			fimc_is_mem_init(&dev->pdev->dev);
	if (IS_ERR(dev->alloc_ctx)) {
		err("Couldn't prepare allocator FW ctx.\n");
		return PTR_ERR(dev->alloc_ctx);
	}
	ret = fimc_is_alloc_firmware(dev);
	if (ret) {
		err("Couldn't alloc for FIMC-IS firmware\n");
		return -EINVAL;
	}
	memset(dev->mem.kvaddr, 0, FIMC_IS_A5_MEM_SIZE);
	dev->is_p_region =
		(struct is_region *)(dev->mem.kvaddr +
			FIMC_IS_A5_MEM_SIZE - FIMC_IS_REGION_SIZE);
	dev->is_shared_region =
		(struct is_share_region *)(dev->mem.kvaddr + FIMC_IS_SHARED_REGION_ADDR);
	if (fimc_is_cache_flush(dev, (void *)dev->is_p_region, IS_PARAM_SIZE)) {
		err("fimc_is_cache_flush-Err\n");
		return -EINVAL;
	}
	return 0;
}
#endif
