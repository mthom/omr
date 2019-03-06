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

#include "OSMemoryMappedCacheUtils.hpp"
#include "OSMemoryMappedCacheConfig.hpp"
#include "OSMemoryMappedCacheConfigOptions.hpp"

namespace OSMemoryMappedCacheUtils
{
/**
 * This method performs additional checks to catch scenarios that are not handled by permission and/or mode settings provided by operating system,
 * to avoid any unintended access to shared cache file
 * 
 * @param [in] portLibrary  The port library
 * @param [in] findHandle  The handle of the shared cache file
 * @param[in] openMode The file access mode
 * @param[in] lastErrorInfo	Pointer to store last portable error code and/or message
 *
 * @return enum SH_CacheFileAccess indicating if the process can access the shared cache file set or not
 */
SH_CacheFileAccess
checkCacheFileAccess(OMRPortLibrary *portLibrary, UDATA fileHandle, I_32 openMode, LastErrorInfo *lastErrorInfo)
{
  SH_CacheFileAccess cacheFileAccess = OMRSH_CACHE_FILE_ACCESS_ALLOWED;

  if (NULL != lastErrorInfo) {
    lastErrorInfo->lastErrorCode = 0;
  }

#if !defined(WIN32)
  OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
  OMRFileStat statBuf;
  IDATA rc = omrfile_fstat(fileHandle, &statBuf);

  if (-1 != rc) {
    UDATA uid = omrsysinfo_get_euid();

    if (statBuf.ownerUid != uid) {
      UDATA gid = omrsysinfo_get_egid();
      bool sameGroup = false;

      if (statBuf.ownerGid == gid) {
	sameGroup = true;
	Trc_SHR_OSC_File_checkCacheFileAccess_GroupIDMatch(gid, statBuf.ownerGid);
      } else {
	/* check supplementary groups */
	U_32 *list = NULL;
	IDATA size = 0;
	IDATA i = 0;

	size = omrsysinfo_get_groups(&list, OMRMEM_CATEGORY_CLASSES_SHC_CACHE);
	if (size > 0) {
	  for (i = 0; i < size; i++) {
	    if (statBuf.ownerGid == list[i]) {
	      sameGroup = true;
	      Trc_SHR_OSC_File_checkCacheFileAccess_SupplementaryGroupMatch(list[i], statBuf.ownerGid);
	      break;
	    }
	  }
	} else {
	  cacheFileAccess = OMRSH_CACHE_FILE_ACCESS_CANNOT_BE_DETERMINED;
	  if (NULL != lastErrorInfo) {
	    lastErrorInfo->populate();
	    /*
	    lastErrorInfo->lastErrorCode = omrerror_last_error_number();
	    lastErrorInfo->lastErrorMsg = omrerror_last_error_message();
	    */
	  }
	  Trc_SHR_OSC_File_checkCacheFileAccess_GetGroupsFailed();
	  goto _end;
	}
	if (NULL != list) {
	  omrmem_free_memory(list);
	}
      }
      if (sameGroup) {
	/* This process belongs to same group as owner of the shared cache file. */
	if (configOptions.groupAccessEnabled()) { // !OMR_ARE_ANY_BITS_SET(openMode, OMROSCACHE_OPEN_MODE_GROUPACCESS)) {
	  /* If 'groupAccess' option is not set, it implies this process wants to access a shared cache file that it created.
	   * But this process is not the owner of the cache file.
	   * This implies we should not allow this process to use the cache.
	   */
	  cacheFileAccess = OMRSH_CACHE_FILE_ACCESS_GROUP_ACCESS_REQUIRED;
	  Trc_SHR_OSC_File_checkCacheFileAccess_GroupAccessRequired();
	}
      } else {
	/* This process does not belong to same group as owner of the shared cache file.
	 * Do not allow access to the cache.
	 */
	cacheFileAccess = OMRSH_CACHE_FILE_ACCESS_OTHERS_NOT_ALLOWED;
	Trc_SHR_OSC_File_checkCacheFileAccess_OthersNotAllowed();
      }
    }
  } else {
    cacheFileAccess = OMRSH_CACHE_FILE_ACCESS_CANNOT_BE_DETERMINED;
    if (NULL != lastErrorInfo) {
      lastErrorInfo->populate(); /*
      lastErrorInfo->lastErrorCode = omrerror_last_error_number();
      lastErrorInfo->lastErrorMsg = omrerror_last_error_message();
				 */
    }
    Trc_SHR_OSC_File_checkCacheFileAccess_FileStatFailed();
  }

 _end:
#endif /* !defined(oWIN32) */

  return cacheFileAccess;
}
  
/**
 * This method checks whether the group access of the shared cache file is successfully set when a new cache/snapshot is created with "groupAccess" suboption
 *
 * @param [in] portLibrary The port library
 * @param[in] fileHandle The handle of the shared cache file
 * @param[in] lastErrorInfo	Pointer to store last portable error code and/or message
 * 
 * @return -1 Failed to get the stats of the file.
 * 			0 Group access is not set.
 * 			1 Group access is set.
 */
I_32
verifyCacheFileGroupAccess(OMRPortLibrary *portLibrary, IDATA fileHandle, LastErrorInfo *lastErrorInfo)
{
  I_32 rc = 1;
#if !defined(WIN32)
  struct OMRFileStat statBuf;
  OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	
  memset(&statBuf, 0, sizeof(statBuf));
  if (0 == omrfile_fstat(fileHandle, &statBuf)) {
    if ((1 != statBuf.perm.isGroupWriteable)
	|| (1 != statBuf.perm.isGroupReadable)
	) {
      rc = 0;
    }
  } else {
    if (NULL != lastErrorInfo) {
      /*
      lastErrorInfo->lastErrorCode = omrerror_last_error_number();
      lastErrorInfo->lastErrorMsg = omrerror_last_error_message();
      */
      lastErrorInfo->populate();
    }
    
    rc = -1;
  }

#endif /* !defined(WIN32) */
  return rc;
}

IDATA
getCacheStats(OMRPortLibrary* library, const char* cacheDirName, const char* cacheName, SH_OSCache_Info *cacheInfo,
	      OSCacheConfigOption configOptions)
{
  OMRPORT_ACCESS_FROM_OMRPORT(library);
  OSMemoryMappedCache* cache = NULL;
  
  I_64 *timeValue;
  I_32 inUse;
  IDATA lockRc;

  cache = new OSMemoryMappedCache(OMRPORTLIB, OMRSH_OSCACHE_MMAP_LOCK_COUNT, configOptions);

  if (!cache->startup(cacheName, cacheDirName)) {
    /* If that fails - try to open the cache read-only */
    configOptions.setReadOnlyOpenMode();

    if (!cache->startup(cacheName, cacheDirName)) {
      cache->cleanup();
      return -1;
    }

    inUse = OMRSH_OSCACHE_UNKNOWN;
  } else {
    /* Try to acquire the attach write lock. This will only succeed if noone else
     * has the attach read lock i.e. there is noone connected to the cache */
    lockRc = cache->tryAcquireAttachWriteLock(); // cacheInfo->generation);
    if (0 == lockRc) {
      Trc_SHR_OSC_Mmap_getCacheStats_cacheNotInUse();
      inUse = 0;
      cache->releaseAttachWriteLock(cacheInfo->generation);
    } else {
      Trc_SHR_OSC_Mmap_getCacheStats_cacheInUse();
      inUse = 1;
    }
  }

  cacheInfo->lastattach = OMRSH_OSCACHE_UNKNOWN;
  cacheInfo->lastdetach = OMRSH_OSCACHE_UNKNOWN;
  cacheInfo->createtime = OMRSH_OSCACHE_UNKNOWN;
  cacheInfo->nattach = inUse;

  /* this next big commented-outpart deals with reading in the values
   * of the header, based on the mode the cache is in (32-bit or
   * 64-bit addressing mode). Here we get around the same problem by
   * calling the header's virtual functions.
   */

  // TODO: make getCacheStats a friend function of OSMemoryMappedCache
  // so it can call these protected functions.
  IDATA rc = cache->internalAttach();
  
  if (0 != rc) {
    cache->setError(rc);
    cache->cleanup();
    return -1;
  }
  
  if ((timeValue = (I_64*) cache->_config->getLastAttachTimeLocation())) {
    cacheInfo->lastattach = *timeValue;
  }
  if ((timeValue = (I_64*) cache->_config->getLastDetachTimeLocation())) {
    cacheInfo->lastdetach = *timeValue;
  }
  if ((timeValue = (I_64*) cache->_config->getLastCreateTimeLocation())) {
    cacheInfo->createtime = *timeValue;
  }

  cache->internalDetach();

// TODO: I've put this off until CacheMap is ported.
  
//  if (SHR_STATS_REASON_ITERATE == reason) {
//    getCacheStatsCommon(vm, cache, cacheInfo);
//  }

//	Trc_SHR_OSC_Mmap_getCacheStats_Exit(cacheInfo->os_shmid,
//					    cacheInfo->os_semid,
//					    cacheInfo->lastattach,
//					    cacheInfo->lastdetach,
//					    cacheInfo->createtime,
//					    cacheInfo->nattach,
//					    cacheInfo->versionData.cacheType);
	/* Note that generation is not set here. This could be determined by parsing the filename,
	 * but is currently set by the caller */
	cache->cleanup();
	return 0;
  
  /* CMVC 177634: Skip calling internalAttach() when destroying the cache */
  //  if (!configOption.openToDestroyExistingCache()) {
    /* The offset of fields createTime, lastAttachedTime, lastDetachedTime in struct OSCache_mmap_header2 are different on 32-bit and 64-bit caches. 
     * This depends on OSCachemmap_header_version_current and OSCache_header_version_current not changing in an incompatible way.
     */

//    		if (OMRSH_ADDRMODE == cacheInfo->versionData.addrmode) {
//			IDATA rc;
//			/* Attach to the cache, so we can read the fields in the header */
//			rc = cache->internalAttach(false, cacheInfo->generation);
//			if (0 != rc) {
//				cache->setError(rc);
//				cache->cleanup();
//				return -1;
//			}
//			cacheHeader = cache->_headerStart;
//
//			/* Read the fields from the header and populate cacheInfo */
//			if ((timeValue = (I_64*)getMmapHeaderFieldAddressForGen(cacheHeader, cacheInfo->generation, OSCACHEMMAP_HEADER_FIELD_LAST_ATTACHED_TIME))) {
//				cacheInfo->lastattach = *timeValue;
//			}
//			if ((timeValue = (I_64*)getMmapHeaderFieldAddressForGen(cacheHeader, cacheInfo->generation, OSCACHEMMAP_HEADER_FIELD_LAST_DETACHED_TIME))) {
//				cacheInfo->lastdetach = *timeValue;
//			}
//			if ((timeValue = (I_64*)getMmapHeaderFieldAddressForGen(cacheHeader, cacheInfo->generation, OSCACHEMMAP_HEADER_FIELD_CREATE_TIME))) {
//				cacheInfo->createtime = *timeValue;
//			}
//			if (SHR_STATS_REASON_ITERATE == reason) {
//				getCacheStatsCommon(vm, cache, cacheInfo);
//			}
//			cache->internalDetach(cacheInfo->generation);
//		}
//  */
//  }
}
  
}
