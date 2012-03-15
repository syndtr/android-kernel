/**********************************************************************
 *
 * Copyright (C) Imagination Technologies Ltd. All rights reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope it will be useful but, except 
 * as otherwise stated in writing, without any warranty; without even the 
 * implied warranty of merchantability or fitness for a particular purpose. 
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * 
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 *
 * Contact Information:
 * Imagination Technologies Ltd. <gpl-support@imgtec.com>
 * Home Park Estate, Kings Langley, Herts, WD4 8LZ, UK 
 *
 ******************************************************************************/

#include <linux/version.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,38))
#ifndef AUTOCONF_INCLUDED
#include <linux/config.h>
#endif
#endif

#include <asm/atomic.h>
#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/hardirq.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/fb.h>
#include <linux/console.h>
#include <linux/mutex.h>

#include "../../staging/android/android_pm.h"

#if defined(DEBUG)
#define	PVR_DEBUG DEBUG
#undef DEBUG
#endif
#if defined(DEBUG)
#undef DEBUG
#endif
#if defined(PVR_DEBUG)
#define	DEBUG PVR_DEBUG
#undef PVR_DEBUG
#endif

#include "img_defs.h"
#include "servicesext.h"
#include "kerneldisplay.h"
#include "s3clfb.h"
#include "pvrmodule.h"

#if !defined(PVR_LINUX_USING_WORKQUEUES)
#error "PVR_LINUX_USING_WORKQUEUES must be defined"
#endif

MODULE_SUPPORTED_DEVICE(DEVNAME);

void *S3CLFBAllocKernelMem(unsigned long ulSize)
{
	return kmalloc(ulSize, GFP_KERNEL);
}

void S3CLFBFreeKernelMem(void *pvMem)
{
	kfree(pvMem);
}

void S3CLFBCreateSwapChainLockInit(S3CLFB_DEVINFO *psDevInfo)
{
	mutex_init(&psDevInfo->sCreateSwapChainMutex);
}

void S3CLFBCreateSwapChainLockDeInit(S3CLFB_DEVINFO *psDevInfo)
{
	mutex_destroy(&psDevInfo->sCreateSwapChainMutex);
}

void S3CLFBCreateSwapChainLock(S3CLFB_DEVINFO *psDevInfo)
{
	mutex_lock(&psDevInfo->sCreateSwapChainMutex);
}

void S3CLFBCreateSwapChainUnLock(S3CLFB_DEVINFO *psDevInfo)
{
	mutex_unlock(&psDevInfo->sCreateSwapChainMutex);
}

void S3CLFBAtomicBoolInit(S3CLFB_ATOMIC_BOOL *psAtomic, S3CLFB_BOOL bVal)
{
	atomic_set(psAtomic, (int)bVal);
}

void S3CLFBAtomicBoolDeInit(S3CLFB_ATOMIC_BOOL *psAtomic)
{
}

void S3CLFBAtomicBoolSet(S3CLFB_ATOMIC_BOOL *psAtomic, S3CLFB_BOOL bVal)
{
	atomic_set(psAtomic, (int)bVal);
}

S3CLFB_BOOL S3CLFBAtomicBoolRead(S3CLFB_ATOMIC_BOOL *psAtomic)
{
	return (S3CLFB_BOOL)atomic_read(psAtomic);
}

void S3CLFBAtomicIntInit(S3CLFB_ATOMIC_INT *psAtomic, int iVal)
{
	atomic_set(psAtomic, iVal);
}

void S3CLFBAtomicIntDeInit(S3CLFB_ATOMIC_INT *psAtomic)
{
}

void S3CLFBAtomicIntSet(S3CLFB_ATOMIC_INT *psAtomic, int iVal)
{
	atomic_set(psAtomic, iVal);
}

int S3CLFBAtomicIntRead(S3CLFB_ATOMIC_INT *psAtomic)
{
	return atomic_read(psAtomic);
}

void S3CLFBAtomicIntInc(S3CLFB_ATOMIC_INT *psAtomic)
{
	atomic_inc(psAtomic);
}

S3CLFB_ERROR S3CLFBGetLibFuncAddr (char *szFunctionName, PFN_DC_GET_PVRJTABLE *ppfnFuncTable)
{
	if(strcmp("PVRGetDisplayClassJTable", szFunctionName) != 0)
	{
		return (S3CLFB_ERROR_INVALID_PARAMS);
	}

	
	*ppfnFuncTable = PVRGetDisplayClassJTable;

	return (S3CLFB_OK);
}

void S3CLFBQueueBufferForSwap(S3CLFB_SWAPCHAIN *psSwapChain, S3CLFB_BUFFER *psBuffer)
{
	int res = queue_work(psSwapChain->psWorkQueue, &psBuffer->sWork);

	if (res == 0)
	{
		printk(KERN_WARNING DRIVER_PREFIX ": %s: Device %u: Buffer already on work queue\n", __FUNCTION__, psSwapChain->uiFBDevID);
	}
}

static void WorkQueueHandler(struct work_struct *psWork)
{
	S3CLFB_BUFFER *psBuffer = container_of(psWork, S3CLFB_BUFFER, sWork);

	S3CLFBSwapHandler(psBuffer);
}

S3CLFB_ERROR S3CLFBCreateSwapQueue(S3CLFB_SWAPCHAIN *psSwapChain)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
	
	psSwapChain->psWorkQueue = alloc_ordered_workqueue(DEVNAME, WQ_FREEZABLE | WQ_MEM_RECLAIM);
#else
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36))
	psSwapChain->psWorkQueue = create_freezable_workqueue(DEVNAME);
#else
	
	psSwapChain->psWorkQueue = __create_workqueue(DEVNAME, 1, 1, 1);
#endif
#endif
	if (psSwapChain->psWorkQueue == NULL)
	{
		printk(KERN_ERR DRIVER_PREFIX ": %s: Device %u: Couldn't create workqueue\n", __FUNCTION__, psSwapChain->uiFBDevID);

		return (S3CLFB_ERROR_INIT_FAILURE);
	}

	return (S3CLFB_OK);
}

void S3CLFBInitBufferForSwap(S3CLFB_BUFFER *psBuffer)
{
	INIT_WORK(&psBuffer->sWork, WorkQueueHandler);
}

void S3CLFBDestroySwapQueue(S3CLFB_SWAPCHAIN *psSwapChain)
{
	destroy_workqueue(psSwapChain->psWorkQueue);
}

void S3CLFBFlip(S3CLFB_DEVINFO *psDevInfo, S3CLFB_BUFFER *psBuffer)
{
	struct fb_var_screeninfo sFBVar;
	int res;
	unsigned long ulYResVirtual;

	S3CLFB_CONSOLE_LOCK();

	sFBVar = psDevInfo->psLINFBInfo->var;

	sFBVar.xoffset = 0;
	sFBVar.yoffset = psBuffer->ulYOffset;

	ulYResVirtual = psBuffer->ulYOffset + sFBVar.yres;

	if (sFBVar.xres_virtual != sFBVar.xres || sFBVar.yres_virtual < ulYResVirtual)
	{
		sFBVar.xres_virtual = sFBVar.xres;
		sFBVar.yres_virtual = ulYResVirtual;

		sFBVar.activate = FB_ACTIVATE_NOW | FB_ACTIVATE_FORCE;

		res = fb_set_var(psDevInfo->psLINFBInfo, &sFBVar);
		if (res != 0)
		{
			printk(KERN_ERR DRIVER_PREFIX ": %s: Device %u: fb_set_var failed (Y Offset: %lu, Error: %d)\n", __FUNCTION__, psDevInfo->uiFBDevID, psBuffer->ulYOffset, res);
		}
	}
	else
	{
		res = fb_pan_display(psDevInfo->psLINFBInfo, &sFBVar);
		if (res != 0)
		{
			printk(KERN_ERR DRIVER_PREFIX ": %s: Device %u: fb_pan_display failed (Y Offset: %lu, Error: %d)\n", __FUNCTION__, psDevInfo->uiFBDevID, psBuffer->ulYOffset, res);
		}
	}

	S3CLFB_CONSOLE_UNLOCK();
}

int s3c_fb_wait_for_vsync(struct fb_info *info);

S3CLFB_BOOL S3CLFBWaitForVSync(S3CLFB_DEVINFO *psDevInfo)
{
	(void)s3c_fb_wait_for_vsync(psDevInfo->psLINFBInfo);

	return S3CLFB_TRUE;
}

static int S3CLFBFrameBufferEvents(struct notifier_block *psNotif,
                             unsigned long event, void *data)
{
	S3CLFB_DEVINFO *psDevInfo;
	struct fb_event *psFBEvent = (struct fb_event *)data;
	struct fb_info *psFBInfo = psFBEvent->info;
	S3CLFB_BOOL bBlanked;

	
	if (event != FB_EVENT_BLANK)
	{
		return 0;
	}

	bBlanked = (*(IMG_INT *)psFBEvent->data != 0) ? S3CLFB_TRUE: S3CLFB_FALSE;

	psDevInfo = S3CLFBGetDevInfoPtr(psFBInfo->node);

#if 0
	if (psDevInfo != NULL)
	{
		if (bBlanked)
		{
			DEBUG_PRINTK((KERN_INFO DRIVER_PREFIX ": %s: Device %u: Blank event received\n", __FUNCTION__, psDevInfo->uiFBDevID));
		}
		else
		{
			DEBUG_PRINTK((KERN_INFO DRIVER_PREFIX ": %s: Device %u: Unblank event received\n", __FUNCTION__, psDevInfo->uiFBDevID));
		}
	}
	else
	{
		DEBUG_PRINTK((KERN_INFO DRIVER_PREFIX ": %s: Device %u: Blank/Unblank event for unknown framebuffer\n", __FUNCTION__, psFBInfo->node));
	}
#endif

	if (psDevInfo != NULL)
	{
		S3CLFBAtomicBoolSet(&psDevInfo->sBlanked, bBlanked);
		S3CLFBAtomicIntInc(&psDevInfo->sBlankEvents);
	}

	return 0;
}

S3CLFB_ERROR S3CLFBUnblankDisplay(S3CLFB_DEVINFO *psDevInfo)
{
	int res;

	S3CLFB_CONSOLE_LOCK();
	res = fb_blank(psDevInfo->psLINFBInfo, 0);
	S3CLFB_CONSOLE_UNLOCK();
	if (res != 0 && res != -EINVAL)
	{
		printk(KERN_ERR DRIVER_PREFIX
			": %s: Device %u: fb_blank failed (%d)\n", __FUNCTION__, psDevInfo->uiFBDevID, res);
		return (S3CLFB_ERROR_GENERIC);
	}

	return (S3CLFB_OK);
}

#ifdef CONFIG_PM_SLEEP
static void S3CLFBBlankDisplay(S3CLFB_DEVINFO *psDevInfo)
{
	S3CLFB_CONSOLE_LOCK();
	fb_blank(psDevInfo->psLINFBInfo, 1);
	S3CLFB_CONSOLE_UNLOCK();
}

static int s3clfb_suspend(struct device *dev)
{
	unsigned uiMaxFBDevIDPlusOne = S3CLFBMaxFBDevIDPlusOne();
	unsigned i;

	for (i=0; i < uiMaxFBDevIDPlusOne; i++)
	{
		S3CLFB_DEVINFO *psDevInfo = S3CLFBGetDevInfoPtr(i);

		if (psDevInfo != NULL)
		{
			S3CLFBAtomicBoolSet(&psDevInfo->sEarlySuspendFlag, S3CLFB_TRUE);
			S3CLFBBlankDisplay(psDevInfo);
		}
	}

	return 0;
}

static int s3clfb_resume(struct device *dev)
{
	unsigned uiMaxFBDevIDPlusOne = S3CLFBMaxFBDevIDPlusOne();
	unsigned i;

	for (i=0; i < uiMaxFBDevIDPlusOne; i++)
	{
		S3CLFB_DEVINFO *psDevInfo = S3CLFBGetDevInfoPtr(i);

		if (psDevInfo != NULL)
		{
			S3CLFBUnblankDisplay(psDevInfo);
			S3CLFBAtomicBoolSet(&psDevInfo->sEarlySuspendFlag, S3CLFB_FALSE);
		}
	}

	return 0;
}
#endif

S3CLFB_ERROR S3CLFBEnableLFBEventNotification(S3CLFB_DEVINFO *psDevInfo)
{
	int                res;
	S3CLFB_ERROR         eError;

	
	memset(&psDevInfo->sLINNotifBlock, 0, sizeof(psDevInfo->sLINNotifBlock));

	psDevInfo->sLINNotifBlock.notifier_call = S3CLFBFrameBufferEvents;

	S3CLFBAtomicBoolSet(&psDevInfo->sBlanked, S3CLFB_FALSE);
	S3CLFBAtomicIntSet(&psDevInfo->sBlankEvents, 0);

	res = fb_register_client(&psDevInfo->sLINNotifBlock);
	if (res != 0)
	{
		printk(KERN_ERR DRIVER_PREFIX
			": %s: Device %u: fb_register_client failed (%d)\n", __FUNCTION__, psDevInfo->uiFBDevID, res);

		return (S3CLFB_ERROR_GENERIC);
	}

	eError = S3CLFBUnblankDisplay(psDevInfo);
	if (eError != S3CLFB_OK)
	{
		printk(KERN_ERR DRIVER_PREFIX
			": %s: Device %u: UnblankDisplay failed (%d)\n", __FUNCTION__, psDevInfo->uiFBDevID, eError);
		return eError;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	psDevInfo->sEarlySuspend.suspend = S3CLFBEarlySuspendHandler;
	psDevInfo->sEarlySuspend.resume = S3CLFBEarlyResumeHandler;
	psDevInfo->sEarlySuspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 1;
	register_early_suspend(&psDevInfo->sEarlySuspend);
#endif

	return (S3CLFB_OK);
}

S3CLFB_ERROR S3CLFBDisableLFBEventNotification(S3CLFB_DEVINFO *psDevInfo)
{
	int res;

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&psDevInfo->sEarlySuspend);
#endif

	res = fb_unregister_client(&psDevInfo->sLINNotifBlock);
	if (res != 0)
	{
		printk(KERN_ERR DRIVER_PREFIX
			": %s: Device %u: fb_unregister_client failed (%d)\n", __FUNCTION__, psDevInfo->uiFBDevID, res);
		return (S3CLFB_ERROR_GENERIC);
	}

	S3CLFBAtomicBoolSet(&psDevInfo->sBlanked, S3CLFB_FALSE);

	return (S3CLFB_OK);
}

STATIC_ANDROID_PM_OPS(s3clfb_apm_ops, s3clfb_suspend, s3clfb_resume);

static int __devinit s3clfb_probe(struct platform_device *pdev)
{
	if(S3CLFBInit() != S3CLFB_OK)
	{
		printk(KERN_ERR DRIVER_PREFIX ": %s: S3CLFBInit failed\n", __FUNCTION__);
		return -ENODEV;
	}

	android_pm_enable(&pdev->dev, &s3clfb_apm_ops);

	return 0;
}

static int __devexit s3clfb_remove(struct platform_device *pdev)
{
	android_pm_disable(&pdev->dev);

	if(S3CLFBDeInit() != S3CLFB_OK)
	{
		printk(KERN_ERR DRIVER_PREFIX ": %s: S3CLFBDeInit failed\n", __FUNCTION__);
	}
	
	return 0;
}

static const struct dev_pm_ops s3clfb_pm_ops = {
#ifndef CONFIG_ANDROID_PM
	SET_SYSTEM_SLEEP_PM_OPS(s3clfb_suspend, s3clfb_resume)
#endif
};

static struct platform_driver s3clfb_driver = {
	.probe = s3clfb_probe,
	.remove = __devexit_p(s3clfb_remove),
	.driver = {
		   .name = "s3c-g3dfb",
		   .owner = THIS_MODULE,
	},
};

static int __init s3clfb_init(void)
{
	platform_driver_register(&s3clfb_driver);

	return 0;
}

static void __exit s3clfb_cleanup(void)
{
	platform_driver_unregister(&s3clfb_driver);
}

module_init(s3clfb_init);
module_exit(s3clfb_cleanup);

MODULE_AUTHOR("Imagination Technologies Ltd. <gpl-support@imgtec.com>");
MODULE_DESCRIPTION("Samsung S3C PVR Framebuffer driver");
MODULE_LICENSE("GPL");
