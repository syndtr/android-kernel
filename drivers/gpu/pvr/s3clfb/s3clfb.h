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

#ifndef __S3CLFB_H__
#define __S3CLFB_H__

#include <linux/version.h>

#include <asm/atomic.h>

#include <linux/kernel.h>
#include <linux/console.h>
#include <linux/fb.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/notifier.h>
#include <linux/mutex.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#ifdef S3CLFB_CONSOLE_NOLOCK
#define S3CLFB_CONSOLE_LOCK()
#define S3CLFB_CONSOLE_UNLOCK()
#else
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38))
#define	S3CLFB_CONSOLE_LOCK()		console_lock()
#define	S3CLFB_CONSOLE_UNLOCK()	console_unlock()
#else
#define	S3CLFB_CONSOLE_LOCK()		acquire_console_sem()
#define	S3CLFB_CONSOLE_UNLOCK()	release_console_sem()
#endif
#endif

#define unref__ __attribute__ ((unused))

typedef void *       S3CLFB_HANDLE;

typedef bool S3CLFB_BOOL, *S3CLFB_PBOOL;
#define	S3CLFB_FALSE false
#define S3CLFB_TRUE true

typedef	atomic_t	S3CLFB_ATOMIC_BOOL;

typedef atomic_t	S3CLFB_ATOMIC_INT;

typedef struct S3CLFB_BUFFER_TAG
{
	struct S3CLFB_BUFFER_TAG	*psNext;
	struct S3CLFB_DEVINFO_TAG	*psDevInfo;

	struct work_struct sWork;

	
	unsigned long		     	ulYOffset;

	
	
	IMG_SYS_PHYADDR              	sSysAddr;
	IMG_CPU_VIRTADDR             	sCPUVAddr;
	PVRSRV_SYNC_DATA            	*psSyncData;

	S3CLFB_HANDLE      		hCmdComplete;
	unsigned long    		ulSwapInterval;
} S3CLFB_BUFFER;

typedef struct S3CLFB_SWAPCHAIN_TAG
{
	
	unsigned int			uiSwapChainID;

	
	unsigned long       		ulBufferCount;

	
	S3CLFB_BUFFER     		*psBuffer;

	
	struct workqueue_struct   	*psWorkQueue;

	
	S3CLFB_BOOL			bNotVSynced;

	
	int				iBlankEvents;

	
	unsigned int            	uiFBDevID;
} S3CLFB_SWAPCHAIN;

typedef struct S3CLFB_FBINFO_TAG
{
	unsigned long       ulFBSize;
	unsigned long       ulBufferSize;
	unsigned long       ulRoundedBufferSize;
	unsigned long       ulWidth;
	unsigned long       ulHeight;
	unsigned long       ulByteStride;
	unsigned long       ulPhysicalWidthmm;
	unsigned long       ulPhysicalHeightmm;

	
	
	IMG_SYS_PHYADDR     sSysAddr;
	IMG_CPU_VIRTADDR    sCPUVAddr;

	
	PVRSRV_PIXEL_FORMAT ePixelFormat;

	S3CLFB_BOOL        bIs2D;
	IMG_SYS_PHYADDR     *psPageList;
	IMG_UINT32          uiBytesPerPixel;
}S3CLFB_FBINFO;

typedef struct S3CLFB_DEVINFO_TAG
{
	
	unsigned int            uiFBDevID;

	
	unsigned int            uiPVRDevID;

	
	struct mutex		sCreateSwapChainMutex;

	
	S3CLFB_BUFFER          sSystemBuffer;

	
	PVRSRV_DC_DISP2SRV_KMJTABLE	sPVRJTable;
	
	
	PVRSRV_DC_SRV2DISP_KMJTABLE	sDCJTable;

	
	S3CLFB_FBINFO          sFBInfo;

	
	S3CLFB_SWAPCHAIN      *psSwapChain;

	
	unsigned int		uiSwapChainID;

	
	S3CLFB_ATOMIC_BOOL     sFlushCommands;

	
	struct fb_info         *psLINFBInfo;

	
	struct notifier_block   sLINNotifBlock;

	
	

	
	IMG_DEV_VIRTADDR	sDisplayDevVAddr;

	DISPLAY_INFO            sDisplayInfo;

	
	DISPLAY_FORMAT          sDisplayFormat;
	
	
	DISPLAY_DIMS            sDisplayDim;

	
	S3CLFB_ATOMIC_BOOL	sBlanked;

	
	S3CLFB_ATOMIC_INT	sBlankEvents;

	S3CLFB_ATOMIC_BOOL	sEarlySuspendFlag;

}  S3CLFB_DEVINFO;

#define	S3CLFB_PAGE_SIZE 4096

#ifdef	DEBUG
#define	DEBUG_PRINTK(x) printk x
#else
#define	DEBUG_PRINTK(x)
#endif

#define DISPLAY_DEVICE_NAME "PowerVR S3C Linux Display Driver"
#define	DRVNAME	"s3clfb"
#define	DEVNAME	DRVNAME
#define	DRIVER_PREFIX "pvr: " DRVNAME

typedef enum _S3CLFB_ERROR_
{
	S3CLFB_OK                             =  0,
	S3CLFB_ERROR_GENERIC                  =  1,
	S3CLFB_ERROR_OUT_OF_MEMORY            =  2,
	S3CLFB_ERROR_TOO_FEW_BUFFERS          =  3,
	S3CLFB_ERROR_INVALID_PARAMS           =  4,
	S3CLFB_ERROR_INIT_FAILURE             =  5,
	S3CLFB_ERROR_CANT_REGISTER_CALLBACK   =  6,
	S3CLFB_ERROR_INVALID_DEVICE           =  7,
	S3CLFB_ERROR_DEVICE_REGISTER_FAILED   =  8,
	S3CLFB_ERROR_SET_UPDATE_MODE_FAILED   =  9
} S3CLFB_ERROR;

#ifndef UNREFERENCED_PARAMETER
#define	UNREFERENCED_PARAMETER(param) (param) = (param)
#endif

S3CLFB_ERROR S3CLFBInit(void);
S3CLFB_ERROR S3CLFBDeInit(void);

S3CLFB_DEVINFO *S3CLFBGetDevInfoPtr(unsigned uiFBDevID);
unsigned S3CLFBMaxFBDevIDPlusOne(void);
void *S3CLFBAllocKernelMem(unsigned long ulSize);
void S3CLFBFreeKernelMem(void *pvMem);
S3CLFB_ERROR S3CLFBGetLibFuncAddr(char *szFunctionName, PFN_DC_GET_PVRJTABLE *ppfnFuncTable);
S3CLFB_ERROR S3CLFBCreateSwapQueue (S3CLFB_SWAPCHAIN *psSwapChain);
void S3CLFBDestroySwapQueue(S3CLFB_SWAPCHAIN *psSwapChain);
void S3CLFBInitBufferForSwap(S3CLFB_BUFFER *psBuffer);
void S3CLFBSwapHandler(S3CLFB_BUFFER *psBuffer);
void S3CLFBQueueBufferForSwap(S3CLFB_SWAPCHAIN *psSwapChain, S3CLFB_BUFFER *psBuffer);
void S3CLFBFlip(S3CLFB_DEVINFO *psDevInfo, S3CLFB_BUFFER *psBuffer);
S3CLFB_BOOL S3CLFBWaitForVSync(S3CLFB_DEVINFO *psDevInfo);
S3CLFB_ERROR S3CLFBUnblankDisplay(S3CLFB_DEVINFO *psDevInfo);
S3CLFB_ERROR S3CLFBEnableLFBEventNotification(S3CLFB_DEVINFO *psDevInfo);
S3CLFB_ERROR S3CLFBDisableLFBEventNotification(S3CLFB_DEVINFO *psDevInfo);
void S3CLFBCreateSwapChainLockInit(S3CLFB_DEVINFO *psDevInfo);
void S3CLFBCreateSwapChainLockDeInit(S3CLFB_DEVINFO *psDevInfo);
void S3CLFBCreateSwapChainLock(S3CLFB_DEVINFO *psDevInfo);
void S3CLFBCreateSwapChainUnLock(S3CLFB_DEVINFO *psDevInfo);
void S3CLFBAtomicBoolInit(S3CLFB_ATOMIC_BOOL *psAtomic, S3CLFB_BOOL bVal);
void S3CLFBAtomicBoolDeInit(S3CLFB_ATOMIC_BOOL *psAtomic);
void S3CLFBAtomicBoolSet(S3CLFB_ATOMIC_BOOL *psAtomic, S3CLFB_BOOL bVal);
S3CLFB_BOOL S3CLFBAtomicBoolRead(S3CLFB_ATOMIC_BOOL *psAtomic);
void S3CLFBAtomicIntInit(S3CLFB_ATOMIC_INT *psAtomic, int iVal);
void S3CLFBAtomicIntDeInit(S3CLFB_ATOMIC_INT *psAtomic);
void S3CLFBAtomicIntSet(S3CLFB_ATOMIC_INT *psAtomic, int iVal);
int S3CLFBAtomicIntRead(S3CLFB_ATOMIC_INT *psAtomic);
void S3CLFBAtomicIntInc(S3CLFB_ATOMIC_INT *psAtomic);

#if defined(DEBUG)
void S3CLFBPrintInfo(S3CLFB_DEVINFO *psDevInfo);
#else
#define	S3CLFBPrintInfo(psDevInfo)
#endif

#endif 

