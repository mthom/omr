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

/**
 * Populates some of cacheInfo for 'SH_OSCachesysv::getCacheStats' and 'SH_OSCachesysv::getJavacoreData'
 *
 * @param [in] vm  The Java VM
 * @param [in] ctrlDirName  Cache directory
 * @param [in] groupPerm  Group permissions to open the cache directory
 * @param [in] cacheNameWithVGen current cache name
 * @param [out] cacheInfo stucture to be populated with cache statistics
 * @param [in] reason Indicates the reason for getting cache stats. Refer sharedconsts.h for valid values.
 * @return 0 on success
 */
IDATA
getCacheStatsHelper(OMRPortLibrary *library, const char* cacheDirName, UDATA groupPerm, const char* cacheName, SH_OSCache_Info* cacheInfo, OSCacheConfigOptions configOptions)
{
  OMRPORT_ACCESS_FROM_OMRPORT(library);//vm->_runtime->_portLibrary);
  OMRPortShmemStatistic statbuf;
  //UDATA versionLen;
  UDATA statrc = 0;

  Trc_SHR_OSC_Sysv_getCacheStatsHelper_Entry(cacheName);//cacheNameWithVGen);

//#if defined(WIN32)
//	versionLen = J9SH_VERSION_STRING_LEN;
//#else
//	versionLen = J9SH_VERSION_STRING_LEN + strlen(OMRSH_MEMORY_ID) - 1;
//#endif

  // J9 specific.
//  if (removeCacheVersionAndGen(cacheInfo->name, CACHE_ROOT_MAXLEN, versionLen, cacheNameWithVGen) != 0) {
//    Trc_SHR_OSC_Sysv_getCacheStatsHelper_removeCacheVersionAndGenFailed();
//    return -1;
//  }
#if !defined(WIN32)
  statrc = OSSharedMemoryCacheUtils::StatSysVMemoryHelper(OMRPORTLIB, cacheDirName, groupPerm, cacheName, &statbuf);
#else
  statrc = omrshmem_stat(cacheDirName, groupPerm, cacheName, &statbuf);
#endif
  
  if (statrc == 0) {
    // J9 specific.
//#if defined(J9ZOS390)
//    if ((J9SH_ADDRMODE != cacheInfo->versionData.addrmode) && (0 == statbuf.size)) {
//      /* JAZZ103 PR 79909 + PR 88930: On z/OS, if the shared memory segment of the cache is created by a 64-bit JVM but
//       * is being accessed by a 31-bit JVM, then the size of shared memory segment may not be verifiable.
//       * In such cases, we should avoid doing any operation - destroy or list -
//       * to avoid accidental access to shared memory segment belonging to some other application.
//       */
//      Trc_SHR_Assert_True(OMRSH_ADDRMODE_32 == OMRSH_ADDRMODE);
//      omrnls_printf( J9NLS_INFO, J9NLS_SHRC_OSCACHE_SHARED_CACHE_SIZE_ZERO_AND_DIFF_ADDRESS_MODE, J9SH_ADDRMODE_64, J9SH_ADDRMODE_64, cacheInfo->name, J9SH_ADDRMODE_32);
//      Trc_SHR_OSC_Sysv_getCacheStatsHelper_shmSizeZeroDiffAddrMode();
//      return -1;
//    }
//#endif /* defined(J9ZOS390) */
    cacheInfo->os_shmid = (statbuf.shmid!=(UDATA)-1)?statbuf.shmid:(UDATA)OMRSH_OSCACHE_UNKNOWN;
		/* os_semid is populated in getJavacoreData() as we can't access _semid here */
    cacheInfo->os_semid = (UDATA)OMRSH_OSCACHE_UNKNOWN;
    cacheInfo->lastattach = (statbuf.lastAttachTime!=-1)?(statbuf.lastAttachTime*1000):OMRSH_OSCACHE_UNKNOWN;
    cacheInfo->lastdetach = (statbuf.lastDetachTime!=-1)?(statbuf.lastDetachTime*1000):OMRSH_OSCACHE_UNKNOWN;
    cacheInfo->createtime = OMRSH_OSCACHE_UNKNOWN;
    cacheInfo->nattach = (statbuf.nattach!=(UDATA)-1)?statbuf.nattach:(UDATA)OMRSH_OSCACHE_UNKNOWN;
  } else if (configOptions.statToDestroy() || configOptions.statExpired()) { //(SHR_STATS_REASON_DESTROY == reason) || (SHR_STATS_REASON_EXPIRE == reason)) {
    /* When destroying the cache, we can ignore failure to get shared memory stats.
     * This allows to delete control files for cache which does not have shared memory.
     */
    cacheInfo->os_shmid = (UDATA)OMRSH_OSCACHE_UNKNOWN;
    cacheInfo->os_semid = (UDATA)OMRSH_OSCACHE_UNKNOWN;
    cacheInfo->lastattach = OMRSH_OSCACHE_UNKNOWN;
    cacheInfo->lastdetach = OMRSH_OSCACHE_UNKNOWN;
    cacheInfo->createtime = OMRSH_OSCACHE_UNKNOWN;
    cacheInfo->nattach = (UDATA)OMRSH_OSCACHE_UNKNOWN;
  } else {
    Trc_SHR_OSC_Sysv_getCacheStatsHelper_cacheStatFailed();
    return -1;
  }
  
  Trc_SHR_OSC_Sysv_getCacheStatsHelper_Exit();
  return 0;
}

/**
 * Populates cacheInfo with cache statistics.
 *
 * @param [in] vm  The Java VM
 * @param [in] ctrlDirName  Cache directory
 * @param [in] groupPerm  Group permissions to open the cache directory
 * @param [in] cacheNameWithVGen current cache name
 * @param [out] cacheInfo stucture to be populated with cache statistics
 * @param [in] reason Indicates the reason for getting cache stats. Refer sharedconsts.h for valid values.
 *
 * @return 0 on success
 */
// IDATA
// getCacheStats(OMRPortLibrary *library, const char* ctrlDirName, UDATA groupPerm, const char* cacheName, SH_OSCache_Info* cacheInfo, OSCacheConfigOptions configOptions)
// {
//   IDATA retval = 0;
//   OMRPORT_ACCESS_FROM_OMRPORT(library);
//   char cacheDirName[OMRSH_MAXPATH];
// 
//   OSCacheUtils::getCacheDirName(OMRPORTLIB, ctrlDirName, cacheDirName, OMRSH_MAXPATH, configOptions);//, OMRPORT_SHR_CACHE_TYPE_NONPERSISTENT);
//   if (OSSharedMemoryCacheUtils::getCacheStatsHelper(OMRPORTLIB, cacheDirName, groupPerm, cacheName, cacheInfo, configOptions) == 0) {
//     /* Using 'SH_OSCachesysv cacheStruct' breaks the pattern of calling getRequiredConstrBytes(), and then allocating memory.
//      * However it is consistent with 'SH_OSCachemmap::getCacheStats'.
// 		 */
//     OSSharedMemoryCache cacheStruct;
//     OSSharedMemoryCache* cache = NULL;
// //    J9PortShcVersion versionData;
// //    OMRSharedCachePreinitConfig piconfig;
//     bool attachedMem = false;
// 
// //    getValuesFromShcFilePrefix(OMRPORTLIB, cacheNameWithVGen, &versionData);
// //    versionData.cacheType = OMRPORT_SHR_CACHE_TYPE_NONPERSISTENT;
// 
//     if (configOptions.statIterate() || configOptions.statList()) { // (SHR_STATS_REASON_ITERATE == reason) || (SHR_STATS_REASON_LIST == reason)) {
//       // create a cache with a single lock.
//       cache = new OSSharedMemoryCache(library, cacheName, cacheDirName, 1, configOptions); //(SH_OSCachesysv *) SH_OSCache::newInstance(OMRPORTLIB, &cacheStruct, cacheInfo->name, cacheInfo->generation, &versionData);
//       
//       if (!cache->startup(cacheName, ctrlDirName)) {
// 	goto done;
//       }
// 
//       /* Avoid calling attach() for incompatible cache since its anyway going to fail.
//        * But we can still get semid from _semhandle as it got populated during startup itself.
//        */
//       if (0 == cacheInfo->isCompatible) {
// 	/* if the cache was opened read-only, there is no _semhandle */
// 	if (NULL != cache->_config->_semhandle) {
// 	  /* SHR_STATS_REASON_LIST needs to populate the os_semid */
// 	  cacheInfo->os_semid = cache->_config->_semid = omrshsem_deprecated_getid(cache->_config->_semhandle);
// 	}
//       } else {
// 	/* Attach to the cache, so we can read the fields in the header */
// 	if (NULL == cache->attach()) {//omr_vmthread_getCurrent(vm), NULL)) {
// 	  cache->cleanup();
// 	  goto done;
// 	}
// 	
// 	attachedMem = true;
// 
// 	/* SHR_STATS_REASON_LIST needs to populate the os_semid */
// 	if (0 != cache->_config->_semid) {
// 	  cacheInfo->os_semid = cache->_config->_semid;
// 	}
// 	
// 	if (configOptions.statIterate()) {//SHR_STATS_REASON_ITERATE == reason) {
// 	  //TODO: implement this.
// 	  getCacheStatsCommon(vm, cache, cacheInfo);
// 	}
// 	
// 	if (attachedMem == true) {
// 	  cache->detach();
// 	}
//       }
//       
//       cache->cleanup();
//     }
//   } else {
//     retval = -1;
//   }
//  done:
//   return retval;
// }
  
}
