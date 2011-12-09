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
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/hardirq.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/sched.h>

#include "sgxdefs.h"
#include "services_headers.h"
#include "sysinfo.h"
#include "sgxapi_km.h"
#include "sysconfig.h"
#include "sgxinfokm.h"
#include "syslocal.h"

#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/opp.h>

#define	ONE_MHZ	1000000
#define	HZ_TO_MHZ(m) ((m) / ONE_MHZ)

extern bool sgx_idle_logging;
extern uint sgx_idle_mode;
extern uint sgx_idle_timeout;
extern uint sgx_apm_latency;

#if defined(LDM_PLATFORM) && !defined(PVR_DRI_DRM_NOT_PCI)
extern struct platform_device *gpsPVRLDMDev;
#endif

static PVRSRV_ERROR PowerLockWrap(SYS_SPECIFIC_DATA *psSysSpecData, IMG_BOOL bTryLock)
{
	if (!in_interrupt())
	{
		if (bTryLock)
		{
			int locked = mutex_trylock(&psSysSpecData->sPowerLock);
			if (locked == 0)
			{
				return PVRSRV_ERROR_RETRY;
			}
		}
		else
		{
			mutex_lock(&psSysSpecData->sPowerLock);
		}
	}

	return PVRSRV_OK;
}

static IMG_VOID PowerLockUnwrap(SYS_SPECIFIC_DATA *psSysSpecData)
{
	if (!in_interrupt())
	{
		mutex_unlock(&psSysSpecData->sPowerLock);
	}
}

PVRSRV_ERROR SysPowerLockWrap(IMG_BOOL bTryLock)
{
	SYS_DATA	*psSysData;

	SysAcquireData(&psSysData);

	return PowerLockWrap(psSysData->pvSysSpecificData, bTryLock);
}

IMG_VOID SysPowerLockUnwrap(IMG_VOID)
{
	SYS_DATA	*psSysData;

	SysAcquireData(&psSysData);

	PowerLockUnwrap(psSysData->pvSysSpecificData);
}

IMG_BOOL WrapSystemPowerChange(SYS_SPECIFIC_DATA *psSysSpecData)
{
	return IMG_TRUE;
}

IMG_VOID UnwrapSystemPowerChange(SYS_SPECIFIC_DATA *psSysSpecData)
{
}

IMG_VOID SysGetSGXTimingInformation(SGX_TIMING_INFORMATION *psTimingInfo)
{
	// TODO dynamic timing
}

void sgx_idle_log_on(void);
void sgx_idle_log_off(void);

void RequestSGXFreq(SYS_DATA *psSysData, IMG_BOOL bMaxFreq)
{
	
}

PVRSRV_ERROR EnableSGXClocks(SYS_DATA *psSysData)
{
#if !defined(NO_HARDWARE)
	SYS_SPECIFIC_DATA *psSysSpecData = (SYS_SPECIFIC_DATA *) psSysData->pvSysSpecificData;

	
	if (atomic_read(&psSysSpecData->sSGXClocksEnabled) != 0)
	{
		return PVRSRV_OK;
	}

	PVR_DPF((PVR_DBG_MESSAGE, "EnableSGXClocks: Enabling SGX Clocks"));

#if defined(LDM_PLATFORM) && !defined(PVR_DRI_DRM_NOT_PCI)
	// TODO dynamic timing

	clk_enable(psSysSpecData->g3d_clk);

#ifdef CONFIG_PM_RUNTIME
	{
		int res = pm_runtime_get_sync(&gpsPVRLDMDev->dev);
		if (res < 0)
		{
			PVR_DPF((PVR_DBG_ERROR, "EnableSGXClocks: pm_runtime_get_sync failed (%d)", -res));
			return PVRSRV_ERROR_UNABLE_TO_ENABLE_CLOCK;
		}
	}
#endif
#endif
	SysEnableSGXInterrupts(psSysData);

	
	atomic_set(&psSysSpecData->sSGXClocksEnabled, 1);

#else	
	PVR_UNREFERENCED_PARAMETER(psSysData);
#endif	

	sgx_idle_log_on();

	return PVRSRV_OK;
}


IMG_VOID DisableSGXClocks(SYS_DATA *psSysData)
{
#if !defined(NO_HARDWARE)
	SYS_SPECIFIC_DATA *psSysSpecData = (SYS_SPECIFIC_DATA *) psSysData->pvSysSpecificData;

	
	if (atomic_read(&psSysSpecData->sSGXClocksEnabled) == 0)
	{
		return;
	}

	sgx_idle_log_off();

	PVR_DPF((PVR_DBG_MESSAGE, "DisableSGXClocks: Disabling SGX Clocks"));

	SysDisableSGXInterrupts(psSysData);

#if defined(LDM_PLATFORM) && !defined(PVR_DRI_DRM_NOT_PCI)
	clk_disable(psSysSpecData->g3d_clk);

#ifdef CONFIG_PM_RUNTIME
	{
		int res = pm_runtime_put_sync(&gpsPVRLDMDev->dev);
		if (res < 0)
		{
			PVR_DPF((PVR_DBG_ERROR, "DisableSGXClocks: pm_runtime_put_sync failed (%d)", -res));
		}
	}
#endif
		// TODO dynamic timing
#endif

	
	atomic_set(&psSysSpecData->sSGXClocksEnabled, 0);

#else	
	PVR_UNREFERENCED_PARAMETER(psSysData);
#endif	
}

static PVRSRV_ERROR AcquireGPTimer(SYS_SPECIFIC_DATA *psSysSpecData)
{
	PVR_UNREFERENCED_PARAMETER(psSysSpecData);

	return PVRSRV_OK;
}

static void ReleaseGPTimer(SYS_SPECIFIC_DATA *psSysSpecData)
{
	PVR_UNREFERENCED_PARAMETER(psSysSpecData);
}

PVRSRV_ERROR EnableSystemClocks(SYS_DATA *psSysData)
{
	SYS_SPECIFIC_DATA *psSysSpecData = (SYS_SPECIFIC_DATA *) psSysData->pvSysSpecificData;

	PVR_TRACE(("EnableSystemClocks: Enabling System Clocks"));

	if (!psSysSpecData->bSysClocksOneTimeInit)
	{
		mutex_init(&psSysSpecData->sPowerLock);

		atomic_set(&psSysSpecData->sSGXClocksEnabled, 0);

		psSysSpecData->bSysClocksOneTimeInit = IMG_TRUE;
	}

	return AcquireGPTimer(psSysSpecData);
}

IMG_VOID DisableSystemClocks(SYS_DATA *psSysData)
{
	SYS_SPECIFIC_DATA *psSysSpecData = (SYS_SPECIFIC_DATA *) psSysData->pvSysSpecificData;

	PVR_TRACE(("DisableSystemClocks: Disabling System Clocks"));

	
	DisableSGXClocks(psSysData);

	ReleaseGPTimer(psSysSpecData);
}

PVRSRV_ERROR SysPMRuntimeRegister(void)
{
#if defined(LDM_PLATFORM) && !defined(PVR_DRI_DRM_NOT_PCI) && defined(CONFIG_PM_RUNTIME)
	pm_runtime_enable(&gpsPVRLDMDev->dev);
#endif
	return PVRSRV_OK;
}

PVRSRV_ERROR SysPMRuntimeUnregister(void)
{
#if defined(LDM_PLATFORM) && !defined(PVR_DRI_DRM_NOT_PCI) && defined(CONFIG_PM_RUNTIME)
	pm_runtime_disable(&gpsPVRLDMDev->dev);
#endif
	return PVRSRV_OK;
}

PVRSRV_ERROR SysDvfsInitialize(SYS_SPECIFIC_DATA *psSysSpecificData)
{
	// TODO DVFS
	return PVRSRV_OK;
}

PVRSRV_ERROR SysDvfsDeinitialize(SYS_SPECIFIC_DATA *psSysSpecificData)
{
	// TODO DVFS
	return PVRSRV_OK;
}
