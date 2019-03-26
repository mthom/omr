/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "ut_omrshr.h"

#include "OSSharedMemoryCacheUtils.hpp"
#include "OSSharedMemoryCacheConfig.hpp"
#include "OSCacheConfigOptions.hpp"
#include "OSCacheUtils.hpp"

namespace OSSharedCacheCacheUtils
{
#if !defined(WIN32)
static IDATA StatSysVMemoryHelper(OMRPortLibrary* portLibrary, const char* cacheDirName, UDATA groupPerm, const char* cacheName, OMRPortShmemStatistic * statbuf)//cacheNameWithVGen, OMRPortShmemStatistic * statbuf)
{
  IDATA rc = -1;
  OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

  Trc_SHR_OSC_Sysv_StatSysVMemoryHelper_Enter();
  // J9 specific.
//  J9PortShcVersion versionData;
//  U_64 cacheVMVersion;
//  UDATA genVersion;
//  UDATA action;

  // J9 specific. once more, in the language agnostic case, we assume the default course.

//  genVersion = getGenerationFromName(cacheNameWithVGen);
//  if (0 == getValuesFromShcFilePrefix(OMRPORTLIB, cacheNameWithVGen, &versionData)) {
//    goto done;
//  }
//
//  cacheVMVersion = getCacheVersionToU64(versionData.esVersionMajor, versionData.esVersionMinor);

//  action = OMRSH_SYSV_REGULAR_CONTROL_FILE;
  rc = omrshmem_stat(cacheDirName, groupPerm, cacheName, statbuf); //cacheNameWithVGen, statbuf);

  //SH_OSCachesysv::SysVCacheFileTypeHelper(cacheVMVersion, genVersion);
  /*
  	switch(action){
		case OMRSH_SYSV_REGULAR_CONTROL_FILE:
			rc = omrshmem_stat(cacheDirName, groupPerm, cacheNameWithVGen, statbuf);
			break;
		case OMRSH_SYSV_OLDER_EMPTY_CONTROL_FILE:
			rc = omrshmem_statDeprecated(cacheDirName, groupPerm, cacheNameWithVGen, statbuf, OMRSH_SYSV_OLDER_EMPTY_CONTROL_FILE);
			break;
		case OMRSH_SYSV_OLDER_CONTROL_FILE:
			rc = omrshmem_statDeprecated(cacheDirName, groupPerm, cacheNameWithVGen, statbuf, OMRSH_SYSV_OLDER_CONTROL_FILE);
			break;
		default:
			Trc_SHR_Assert_ShouldNeverHappen();
			break;
	}
  */
 done:
  Trc_SHR_OSC_Sysv_StatSysVMemoryHelper_Exit(rc);
  return rc;
}
#endif
}
