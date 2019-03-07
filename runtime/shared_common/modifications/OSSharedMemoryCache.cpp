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

#include "OSSharedMemoryCache.hpp"

OSSharedMemoryCache::OSSharedMemoryCache(OMRPortLibrary* library,
					 const char* cacheName,
					 const char* cacheDirName,
					 IDATA numLocks,
					 OSCacheConfigOptions& configOptions)
  : _portLibrary(library)
  , _config(OSSharedMemoryCacheConfig(numLocks))
  , _configOptions(configOptions)
{
  // I need to revise the arguments to this trace message.
  // Trc_SHR_OSC_Constructor_Entry(cacheName, piconfig->sharedClassCacheSize, createFlag);
  initialize();
  // expect the open mode has been set in the configOptions object already.
  // configOptions.setOpenMode(openMode);
  startup(cacheName, ctrlDirName);
  Trc_SHR_OSC_Constructor_Exit(cacheName);
}

void
OSSharedMemoryCache::initialize()
{
  commonInit();

  _attach_count = 0;
  _config->_shmhandle = NULL;
  _config->_semhandle = NULL;
  // _actualCacheSize = 0;
  _shmFileName = NULL;
  _semFileName = NULL;
  _openSharedMemory = false;
  _config->_semid = 0;
  _config->_groupPerm = 0;
  _corruptionCode = NO_CORRUPTION;
  _corruptValue = NO_CORRUPTION;
  _config->_semAccess = OMRSH_SEM_ACCESS_ALLOWED;
  _config->_shmAccess = OMRSH_SHM_ACCESS_ALLOWED;
}

bool
OSSharedMemoryCache::startup(const char* cacheName, const char* ctrlDirName)
{
  IDATA retryCount;
  IDATA shsemrc = 0;
  IDATA semLength = 0;
  LastErrorInfo lastErrorInfo;

  UDATA defaultCacheSize = J9_SHARED_CLASS_CACHE_DEFAULT_SIZE;
  
  // J9 specific stuff:
  
//#if defined(J9VM_ENV_DATA64)
//#if defined(OPENJ9_BUILD)
//  defaultCacheSize = J9_SHARED_CLASS_CACHE_DEFAULT_SIZE_64BIT_PLATFORM;
//#else /* OPENJ9_BUILD */
//  if (J2SE_VERSION(vm) >= J2SE_19) {
//    defaultCacheSize = J9_SHARED_CLASS_CACHE_DEFAULT_SIZE_64BIT_PLATFORM;
//  }
//#endif /* OPENJ9_BUILD */
//#endif /* J9VM_ENV_DATA64 */

  OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

  // TODO: arguments need revision
  // Trc_SHR_OSC_startup_Entry(cacheName, (piconfig!= NULL)? piconfig->sharedClassCacheSize : defaultCacheSize, create);

  if (_configOptions.groupAccessEnabled()) {
    _groupPerm = 1;
  }

  // J9 specific.
  // versionData->cacheType = OMRPORT_SHR_CACHE_TYPE_NONPERSISTENT;
  _cacheSize = (piconfig!= NULL) ? (U_32)piconfig->sharedClassCacheSize : (U_32)defaultCacheSize;
  //  _initializer = i;
  _totalNumSems = _numLocks + 1;		/* +1 because of header mutex */
  _userSemCntr = 0;

  retryCount = OMRSH_OSCACHE_RETRYMAX;

  // J9 specific.
  // _storageKeyTesting = storageKeyTesting;

  IDATA openMode = _configOptions.openMode();
  IDATA cacheDirPermissions = _configOptions.cacheDirPermissions();

  // this is how commonStartup was broken up, into these next two blocks.
  if(initCacheDirName(ctrlDirName, cacheDirPermissions, openMode) != 0) {
    Trc_SHR_OSC_Mmap_startup_commonStartupFailure();
    return false;
  }
  
  if(initCacheName(cacheName) != 0) {
    return false;
  }

  Trc_SHR_OSC_startup_commonStartupSuccess();

#if defined(WIN32)
  _semFileName = _cacheName; // J9 specific: was _cacheNameWithVGen;
#else
  semLength = strlen(_cacheName) // strlen(_cacheNameWithVGen)
    + (strlen(OMRSH_SEMAPHORE_ID) - strlen(OMRSH_MEMORY_ID)) + 1;
  /* Unfortunate case is that Java5 and early Java6 caches did not have _G append on the semaphore file,
   * so to connect with a generation 1 or 2 cache (Java5 was only ever G01), remove the _G01 from the semaphore file name */
  // J9 specific:
//  if (_activeGeneration < 3) {
//    semLength -= strlen("_GXX");
//  }
  if (!(_semFileName = (char*)omrmem_allocate_memory(semLength, OMRMEM_CATEGORY_CLASSES))) {
    Trc_SHR_OSC_startup_nameAllocateFailure();
    OSC_ERR_TRACE(J9NLS_SHRC_OSCACHE_ALLOC_FAILED);
    return false;
  }
  // J9 specific:
  //getCacheVersionAndGen(OMRPORTLIB, vm, _semFileName, semLength, cacheName, versionData, _activeGeneration, false);
#endif

  
}
