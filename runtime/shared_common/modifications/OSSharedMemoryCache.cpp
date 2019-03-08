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

  //TODO: this leaves open the question of how to determine and write the value of the _semFileName string.
  // J9 specific:
  //getCacheVersionAndGen(OMRPORTLIB, vm, _semFileName, semLength, cacheName, versionData, _activeGeneration, false);
#endif

  while (retryCount > 0) {
    IDATA rc;

    if(_configOptions.readOnlyOpenMode()) {
      if (!statCache(_portLibrary, _cacheDirName, _shmFileName, false)) {
	OSC_ERR_TRACE(J9NLS_SHRC_OSCACHE_STARTUP_CACHE_CREATION_NOT_ALLOWED_READONLY_MODE);
	Trc_SHR_OSC_startup_cacheCreationNotAllowed_readOnlyMode();
	rc = OS_SHARED_MEMORY_CACHE_FAILURE;
	break;
      }

      /* Don't get the semaphore set when running read-only, but pretend that we did */
      shsemrc = OMRPORT_INFO_SHSEM_OPENED;
      _semhandle = NULL;
    } else {
      #if !defined(WIN32)
      shsemrc = OpenSysVSemaphoreHelper(versionData, &lastErrorInfo);
#else
      /* Currently on windows, "flags" passed to omrshsem_deprecated_open() are not used, but its better to pass correct flags */
      /*
      UDATA flags = OMRSHSEM_NO_FLAGS;
      if (OMRSH_OSCACHE_OPEXIST_STATS == _createFlags) {
	flags = OMRSHSEM_OPEN_FOR_STATS;
      } else if (OMRSH_OSCACHE_OPEXIST_DESTROY == _createFlags) {
	flags = OMRSHSEM_OPEN_FOR_DESTROY;
      } else if (OMRSH_OSCACHE_OPEXIST_DO_NOT_CREATE == _createFlags) {
	flags = OMRSHSEM_OPEN_DO_NOT_CREATE;
      }
      */

      UDATA flags = _configOptions.renderCreateOptionsToFlags();

      shsemrc = omrshsem_deprecated_open(_cacheDirName, _config->_groupPerm,
					 &_config->_semhandle, _semFileName,
					 (int)_totalNumSems, 0, flags, NULL);
      lastErrorInfo->populate(_portLibrary);
#endif
    }

    if (shsemrc == OMRPORT_INFO_SHSEM_PARTIAL) {
      /*
       * OMRPORT_INFO_SHSEM_PARTIAL indicates one of the following cases:
       * 	- omrshsem_deprecated_openDeprecated() was called and the control files for the semaphore does not exist, or
       *  - omrshsem_deprecated_openDeprecated() was called and the sysv object matching the control file has been deleted, or
       *  - omrshsem_deprecated_open() failed and flag OPEXIST_STATS is set, or
       *  - omrshsem_deprecated_open() was called with flag OPEXIST_DESTROY and control files for the semaphore does not exist
       *
       * In such cases continue without the semaphore as readonly
       *
       * If we are starting up the cache for 'destroy' i.e. OPEXIST_DESTROY is set,
       * then do not set READONLY flag as it may prevent unlinking of control files in port library if required.
       */
      if(!_configOptions.openToDestroyExistingCache()) {
	// try to open the cache in read-only mode if we're not opening to cache to destroy it.
	_configOptions.setReadOnlyOpenMode();
      }

      shsemrc = OMRPORT_INFO_SHSEM_CREATED;
      _config->_semhandle = NULL;
    }

    switch(shsemrc) {
    case OMRPORT_INFO_SHSEM_CREATED:
      if (_configOptions.groupAccessEnabled()) {
	/* Verify if the group access has been set */
	struct J9FileStat statBuf;
	char pathFileName[OMRSH_MAXPATH];

	I_32 semid = omrshsem_deprecated_getid(_semhandle);
	I_32 groupAccessRc = verifySemaphoreGroupAccess(&lastErrorInfo);

	if (0 == groupAccessRc) {
	  Trc_SHR_OSC_startup_setSemaphoreGroupAccessFailed(semid);
	  OSC_WARNING_TRACE1(OMRNLS_SHRC_OSCACHE_SEMAPHORE_SET_GROUPACCESS_FAILED, semid);
	} else if (-1 == groupAccessRc) {
	  /* Fail to get stats of the semaphore */
	  Trc_SHR_OSC_startup_badSemaphoreStat(semid);
	  errorHandler(OMRNLS_SHRC_OSCACHE_INTERNAL_ERROR_CHECKING_SEMAPHORE_ACCESS, &lastErrorInfo);
	  rc = OS_SHARED_CACHE_FAILURE;
	  break;
	}

	getCachePathName(OMRPORTLIB, _cacheDirName, pathFileName, OMRSH_MAXPATH, _semFileName);
	/* No check for return value of getCachePathName() as it always return 0 */
	memset(&statBuf, 0, sizeof(statBuf));
	if (0 == omrfile_stat(pathFileName, 0, &statBuf)) {
	  if (1 != statBuf.perm.isGroupReadable) {
	    /* Control file needs to be group readable */
	    Trc_SHR_OSC_startup_setGroupAccessFailed(pathFileName);
	    OSC_WARNING_TRACE1(J9NLS_SHRC_OSCACHE_SEM_CONTROL_FILE_SET_GROUPACCESS_FAILED, pathFileName);
	  }
	} else {
	  Trc_SHR_OSC_startup_badFileStat(pathFileName);
	  lastErrorInfo.populate(_portLibrary);
	  errorHandler(OMRNLS_SHRC_OSCACHE_ERROR_FILE_STAT, &lastErrorInfo);
	  
	  rc = OS_SHARED_CACHE_FAILURE;
	  break;
	}	
      }
#endif /* !defined(WIN32) */
    case OMRPORT_INFO_SHSEM_OPENED:
      /* Avoid any checks for semaphore access if
       * - running in readonly mode as we don't use semaphore, or
       * - user has specified a cache directory, or
       * - destroying an existing cache
       */
      if (!_configOptions.readOnlyOpenMode()  //OMR_ARE_NO_BITS_SET(_openMode, J9OSCACHE_OPEN_MODE_DO_READONLY)
	  && (OMRPORT_INFO_SHSEM_OPENED == shsemrc)
	  && (!_configOptions.isUserSpecifiedCacheDir())
	  && (!_configOptions.openToDestroyExistingCache())//OMR_ARE_NO_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_DESTROY))
	  ) {
	_config->_semAccess = checkSemaphoreAccess(&lastErrorInfo);
      }

      /* Ignore _semAccess when opening cache for printing stats, but we do need it later to display cache usability */
      if (_configOptions.openToStatExistingCache()//OMR_ARE_ALL_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_STATS)
	  || (OMRSH_SEM_ACCESS_ALLOWED == _config->_semAccess)
	  ) {
	IDATA headerMutexRc = 0;
	if (!_configOptions.restoreCheckEnabled()) { //OMR_ARE_NO_BITS_SET(_runtimeFlags, OMRSHR_RUNTIMEFLAG_RESTORE_CHECK)) {
	  /* When running "restoreFromSnapshot" utility, headerMutex has already been acquired in the first call of SH_OSCachesysv::startup()*/
	  //enterHeaderMutex(&lastErrorInfo);
	  headerMutexRc = acquireHeaderWriteLock(_portLibrary, _cacheName, &lastErrorInfo); 
	}
	
	if (0 == headerMutexRc) { //TODO: start here.
	  rc = openCache(_cacheDirName, (shsemrc == OMRPORT_INFO_SHSEM_CREATED));
	  if (!_configOptions.restoreCheckEnabled() || !_configOptions.restoreEnabled()) {
	      // OMR_ARE_NO_BITS_SET(_runtimeFlags, OMRSHR_RUNTIMEFLAG_RESTORE | OMRSHR_RUNTIMEFLAG_RESTORE_CHECK)) {
	    /* When running "restoreFromSnapshot" utility, do not release headerMutex here */
	    if (0 != releaseHeaderWriteLock(_portLibrary, &lastErrorInfo)) {
	      errorHandler(OMRNLS_SHRC_OSCACHE_ERROR_EXIT_HDR_MUTEX, &lastErrorInfo);
	      rc = OS_SHARED_MEMORY_CACHE_FAILURE;
	    }
	  }
	} else {
	  errorHandler(OMRNLS_SHRC_OSCACHE_ERROR_ENTER_HDR_MUTEX, &lastErrorInfo);
	  rc = OS_SHARED_MEMORY_CACHE_FAILURE;
	}
      } else {
	switch (_semAccess) {
	case OMRSH_SEM_ACCESS_CANNOT_BE_DETERMINED:
	  errorHandler(OMRNLS_SHRC_OSCACHE_INTERNAL_ERROR_CHECKING_SEMAPHORE_ACCESS, &lastErrorInfo);
	  break;
	case OMRSH_SEM_ACCESS_OWNER_NOT_CREATOR:
	  errorHandler(OMRNLS_SHRC_OSCACHE_SEMAPHORE_OWNER_NOT_CREATOR, NULL);
	  break;
	case OMRSH_SEM_ACCESS_GROUP_ACCESS_REQUIRED:
	  errorHandler(OMRNLS_SHRC_OSCACHE_SEMAPHORE_GROUPACCESS_REQUIRED, NULL);
	  break;
	case OMRSH_SEM_ACCESS_OTHERS_NOT_ALLOWED:
	  errorHandler(OMRNLS_SHRC_OSCACHE_SEMAPHORE_OTHERS_ACCESS_NOT_ALLOWED, NULL);
	  break;
	default:
	  Trc_SHR_Assert_ShouldNeverHappen();
	}
	rc = OS_SHARED_MEMORY_CACHE_FAILURE;
      }
      break;     
    }
  }
}
}

/* OK, I am uhh, respectfully wondering why the lastErrorInfo object created by startup is not
   passed to this function. Seriously, why is that?? I will try to discover the answer! */
/* The caller should hold the mutex */
IDATA
OSSharedMemoryCache::openCache(const char* cacheDirName, bool semCreated) //, J9PortShcVersion* versionData, bool semCreated)
{
  /* we are attaching to existing cache! */
  Trc_SHR_OSC_openCache_Entry(_cacheName);
  IDATA rc;
  IDATA result = OS_SHARED_MEMORY_CACHE_FAILURE; //OSCACHESYSV_FAILURE;
  LastErrorInfo lastErrorInfo; // *ahem* WHY??
	
  OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
  
  rc = shmemOpenWrapper(_shmFileName, &lastErrorInfo);
  Trc_SHR_OSC_openCache_shmem_open(_shmFileName, _cacheSize);
  
  switch (rc) {
  case OMRPORT_ERROR_SHMEM_OPEN_ATTACHED_FAILED:
    _openSharedMemory = true;
    /* FALLTHROUGH */
  case OMRPORT_ERROR_SHMEM_CREATE_ATTACHED_FAILED:
    /*We failed to attach to the memory.*/
    errorHandler(OMRNLS_SHRC_OSCACHE_SHMEM_OPEN_ATTACHED_FAILED, &lastErrorInfo);
    Trc_SHR_OSC_openCache_ExitAttachFailed();
    result = OS_SHARED_MEMORY_CACHE_FAILURE;
    break;
  case OMRPORT_INFO_SHMEM_OPENED:
    /* Avoid any checks if
     * - user has specified a cache directory, or
     * - destroying an existing cache
     */
    if (!_configOptions.isUserSpecifiedCacheDir() && !_configOptions.openToDestroyExistingCache()) {
      //if (!_isUserSpecifiedCacheDir && (OMR_ARE_NO_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_DESTROY))) {
      _config->_shmAccess = checkSharedMemoryAccess(&lastErrorInfo);
    }
    /* Ignore _shmAccess when opening cache for printing stats, but we do need it later to display cache usability */
    if (_configOptions.openToStatExistingCache() // OMR_ARE_ALL_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_STATS)
	|| (OMRSH_SHM_ACCESS_ALLOWED == _config->_shmAccess)
	) {
      /* ALL SET */
      Trc_SHR_OSC_openCache_Exit_Opened(_cacheName);
      result = OS_SHARED_CACHE_OPENED;
    } else {
      switch (_config->_shmAccess) {
      case OMRSH_SHM_ACCESS_CANNOT_BE_DETERMINED:
	errorHandler(OMRNLS_SHRC_OSCACHE_INTERNAL_ERROR_CHECKING_SHARED_MEMORY_ACCESS, &lastErrorInfo);
	break;
      case OMRSH_SHM_ACCESS_OWNER_NOT_CREATOR:
	errorHandler(OMRNLS_SHRC_OSCACHE_SHARED_MEMORY_OWNER_NOT_CREATOR, NULL);
	break;
      case OMRSH_SHM_ACCESS_GROUP_ACCESS_REQUIRED:
	errorHandler(OMRNLS_SHRC_OSCACHE_SHARED_MEMORY_GROUPACCESS_REQUIRED, NULL);
	break;
      case OMRSH_SHM_ACCESS_GROUP_ACCESS_READONLY_REQUIRED:
	errorHandler(OMRNLS_SHRC_OSCACHE_SHARED_MEMORY_GROUPACCESS_READONLY_REQUIRED, NULL);
	break;
      case OMRSH_SHM_ACCESS_OTHERS_NOT_ALLOWED:
	errorHandler(OMRNLS_SHRC_OSCACHE_SHARED_MEMORY_OTHERS_ACCESS_NOT_ALLOWED, NULL);
	break;
      default:
	Trc_SHR_Assert_ShouldNeverHappen();
      }
      Trc_SHR_OSC_openCache_ExitAccessNotAllowed(_config->_shmAccess);
      result = OS_SHARED_CACHE_FAILURE;
    }

    break;
  case OMRPORT_INFO_SHMEM_CREATED:
    /* We opened semaphore yet created the cache area -
     * we should set it up, but don't need to init semaphore
     */
    // TODO: this is now a pure virtual function in the Config class.
    rc = _config->initializeHeader(cacheDirName, lastErrorInfo); //versionData, lastErrorInfo);
    if(rc == OS_SHARED_MEMORY_CACHE_FAILURE) {
      Trc_SHR_OSC_openCache_Exit_CreatedHeaderInitFailed(_cacheName);
      result = OS_SHARED_MEMORY_CACHE_FAILURE;
      break;
    }
    Trc_SHR_OSC_openCache_Exit_Created(_cacheName);
    result = OS_SHARED_MEMORY_CACHE_CREATED;
    break;

  case OMRPORT_ERROR_SHMEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT:
    errorHandler(OMRNLS_SHRC_OSCACHE_SHMEM_OPEN_WAIT_FOR_CREATION_MUTEX_TIMEDOUT, &lastErrorInfo);
    Trc_SHR_OSC_openCache_Exit4();
    result = OS_SHARED_MEMORY_CACHE_FAILURE;
    break;
		
  case OMRPORT_INFO_SHMEM_PARTIAL:
    /* If OMRPORT_INFO_SHMEM_PARTIAL then ::startup() was called by j9shr_destroy_cache().
     * Returning OSCACHESYSV_OPENED will cause j9shr_destroy_cache() to call ::destroy(),
     * which will cleanup any control files that have there SysV IPC objects del
     */
    result = OS_SHARED_MEMORY_CACHE_OPENED;
    break;
    
  case OMRPORT_ERROR_SHMEM_OPFAILED:
  case OMRPORT_ERROR_SHMEM_OPFAILED_CONTROL_FILE_LOCK_FAILED:
  case OMRPORT_ERROR_SHMEM_OPFAILED_CONTROL_FILE_CORRUPT:
  case OMRPORT_ERROR_SHMEM_OPFAILED_SHMID_MISMATCH:
  case OMRPORT_ERROR_SHMEM_OPFAILED_SHM_KEY_MISMATCH:
  case OMRPORT_ERROR_SHMEM_OPFAILED_SHM_GROUPID_CHECK_FAILED:
  case OMRPORT_ERROR_SHMEM_OPFAILED_SHM_USERID_CHECK_FAILED:
  case OMRPORT_ERROR_SHMEM_OPFAILED_SHM_SIZE_CHECK_FAILED:
  case OMRPORT_ERROR_SHMEM_OPFAILED_SHARED_MEMORY_NOT_FOUND:
  default:
    if ((_configOptions.openToStatExistingCache() || _configOptions.OpenToDestroyExistingCache()
	 || _configOptions.openButDoNotCreate())
	//OMR_ARE_ANY_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_STATS | OMRSH_OSCACHE_OPEXIST_DESTROY | OMRSH_OSCACHE_OPEXIST_DO_NOT_CREATE)
	&& (OMRPORT_ERROR_SHMEM_OPFAILED_SHARED_MEMORY_NOT_FOUND == rc)
	) {
      if (_configOptions.OpenToDestroyExistingCache()) { //OMR_ARE_ALL_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_DESTROY)) {
	/* Absence of shared memory is equivalent to non-existing cache. Do not display any error message,
	 * but do call cleanupSysVResources() to remove any semaphore set in case we opened it successfully.
	 */
	cleanupSysvResources();
      } else if (_configOptions.openToStatExistingCache() || _configOptions.openButDoNotCreate()) {//OMR_ARE_ANY_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_STATS | OMRSH_OSCACHE_OPEXIST_DO_NOT_CREATE)) {
	omrnls_printf( OMRNLS_ERROR, OMRNLS_SHRC_OSCACHE_NOT_EXIST);
      }
      Trc_SHR_OSC_openCache_Exit3();
      result = OS_SHARED_MEMORY_CACHE_NOT_EXIST;
    } else {
      I_32 shmid = 0;

      /* For some error codes, portlibrary stores shared memory id obtained from the control file
       * enabling us to display it in the error message. Retrieve the id and free memory allocated to handle.
       */
      if (NULL != _config->_shmhandle) {
	shmid = omrshmem_getid(_config->_shmhandle);
	omrmem_free_memory(_config->_shmhandle);
      }

      if ((OMRPORT_ERROR_SHMEM_OPFAILED == rc) || (OMRPORT_ERROR_SHMEM_OPFAILED_SHARED_MEMORY_NOT_FOUND == rc)) {
	errorHandler(OMRNLS_SHRC_OSCACHE_SHMEM_OPFAILED_V1, &lastErrorInfo);
	if ((OMRPORT_ERROR_SHMEM_OPFAILED == rc) && (0 != shmid)) {
	  omrnls_printf( OMRNLS_ERROR, OMRNLS_SHRC_OSCACHE_SHARED_MEMORY_OPFAILED_DISPLAY_SHMID, shmid);
	}
      } else if (OMRPORT_ERROR_SHMEM_OPFAILED_CONTROL_FILE_LOCK_FAILED == rc) {
	errorHandler(OMRNLS_SHRC_OSCACHE_SHMEM_OPFAILED_CONTROL_FILE_LOCK_FAILED, &lastErrorInfo);
      } else if (OMRPORT_ERROR_SHMEM_OPFAILED_CONTROL_FILE_CORRUPT == rc) {
	errorHandler(OMRNLS_SHRC_OSCACHE_SHMEM_OPFAILED_CONTROL_FILE_CORRUPT, &lastErrorInfo);
      } else if (OMRPORT_ERROR_SHMEM_OPFAILED_SHMID_MISMATCH == rc) {
	errorHandler(OMRNLS_SHRC_OSCACHE_SHMEM_OPFAILED_SHMID_MISMATCH, &lastErrorInfo);
	omrnls_printf( OMRNLS_ERROR, OMRNLS_SHRC_OSCACHE_SHARED_MEMORY_OPFAILED_DISPLAY_SHMID, shmid);
      } else if (OMRPORT_ERROR_SHMEM_OPFAILED_SHM_KEY_MISMATCH == rc) {
	errorHandler(OMRNLS_SHRC_OSCACHE_SHMEM_OPFAILED_SHM_KEY_MISMATCH, &lastErrorInfo);
	omrnls_printf( OMRNLS_ERROR, OMRNLS_SHRC_OSCACHE_SHARED_MEMORY_OPFAILED_DISPLAY_SHMID, shmid);
      } else if (OMRPORT_ERROR_SHMEM_OPFAILED_SHM_GROUPID_CHECK_FAILED == rc) {
	errorHandler(OMRNLS_SHRC_OSCACHE_SHMEM_OPFAILED_SHM_GROUPID_CHECK_FAILED, &lastErrorInfo);
	omrnls_printf( OMRNLS_ERROR, OMRNLS_SHRC_OSCACHE_SHARED_MEMORY_OPFAILED_DISPLAY_SHMID, shmid);
      } else if (OMRPORT_ERROR_SHMEM_OPFAILED_SHM_USERID_CHECK_FAILED == rc) {
	errorHandler(OMRNLS_SHRC_OSCACHE_SHMEM_OPFAILED_SHM_USERID_CHECK_FAILED, &lastErrorInfo);
	omrnls_printf( OMRNLS_ERROR, OMRNLS_SHRC_OSCACHE_SHARED_MEMORY_OPFAILED_DISPLAY_SHMID, shmid);
      } else if (OMRPORT_ERROR_SHMEM_OPFAILED_SHM_SIZE_CHECK_FAILED == rc) {
	errorHandler(OMRNLS_SHRC_OSCACHE_SHMEM_OPFAILED_SHM_SIZE_CHECK_FAILED, &lastErrorInfo);
	omrnls_printf( OMRNLS_ERROR, OMRNLS_SHRC_OSCACHE_SHARED_MEMORY_OPFAILED_DISPLAY_SHMID, shmid);
      }
      /* Report any error that occurred during unlinking of control file */
      if (OMRPORT_INFO_CONTROL_FILE_UNLINK_FAILED == _controlFileStatus.status) {
	omrnls_printf( OMRNLS_ERROR, OMRNLS_SHRC_OSCACHE_SHARED_MEMORY_CONTROL_FILE_UNLINK_FAILED, _shmFileName);
	OSC_ERR_TRACE1(OMRNLS_SHRC_OSCACHE_PORT_ERROR_NUMBER, _controlFileStatus.errorCode);
	OSC_ERR_TRACE1(OMRNLS_SHRC_OSCACHE_PORT_ERROR_MESSAGE, _controlFileStatus.errorMsg);
      }
#if !defined(WIN32)
      if (_configOptions.openToStatExistingCache()) { //(_createFlags & OMRSH_OSCACHE_OPEXIST_STATS)) {
	OSC_TRACE(OMRNLS_SHRC_OSCACHE_CONTROL_FILE_RECREATE_PROHIBITED_FETCHING_CACHE_STATS);
      } else if (_configOptions.openToDestroyExistingCache()) { //OMR_ARE_ANY_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_DO_NOT_CREATE)) {
	OSC_TRACE(OMRNLS_SHRC_OSCACHE_CONTROL_FILE_RECREATE_PROHIBITED);
      } else if (_configOptions.readOnlyOpenMode()) { // 0 != (_openMode & OMROSCACHE_OPEN_MODE_DO_READONLY)) {
	OSC_TRACE(OMRNLS_SHRC_OSCACHE_CONTROL_FILE_RECREATE_PROHIBITED_RUNNING_READ_ONLY);
      }
#endif
      Trc_SHR_OSC_openCache_Exit3();
      result = OS_SHARED_MEMORY_CACHE_FAILURE; // OSCACHESYSV_FAILURE;
    }
    break;
  }

  return result;
}

IDATA
OSSharedMemoryCache::shmemOpenWrapper(const char *cacheName, LastErrorInfo *lastErrorInfo)
{
  OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
  IDATA rc = 0;
  U_32 perm = _configOptions.readOnlyOpenMode() ? OMRSH_SHMEM_PERM_READ : OMRSH_SHMEM_PERM_READ_WRITE;
  // U_32 perm = (_openMode & J9OSCACHE_OPEN_MODE_DO_READONLY) ? OMRSH_SHMEM_PERM_READ : OMRSH_SHMEM_PERM_READ_WRITE;

  LastErrorInfo localLastErrorInfo;
  Trc_SHR_OSC_shmemOpenWrapper_Entry(cacheName);
  UDATA flags = _configOptions.renderCreateOptionsToFlags();

//  UDATA flags = OMRSHMEM_NO_FLAGS;
//
//  if (OMR_ARE_ANY_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_STATS)) {
//    flags |= OMRSHMEM_OPEN_FOR_STATS;
//  } else if (OMR_ARE_ANY_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_DESTROY)) {
//    flags |= OMRSHMEM_OPEN_FOR_DESTROY;
//  } else if (OMR_ARE_ANY_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_DO_NOT_CREATE)) {
//    flags |= OMRSHMEM_OPEN_DO_NOT_CREATE;
//  }

#if !defined(WIN32)
  rc = OpenSysVMemoryHelper(cacheName, perm, &localLastErrorInfo);
#else
  rc = omrshmem_open(_cacheDirName, _config->_groupPerm, &_config->_shmhandle, cacheName, _cacheSize, perm, OMRMEM_CATEGORY_CLASSES_SHC_CACHE, flags, NULL);
  localLastErrorInfo.populate(OMRPORTLIB);
//	localLastErrorInfo.lastErrorCode = omrerror_last_error_number();
//	localLastErrorInfo.lastErrorMsg = omrerror_last_error_message();
#endif

#if defined(J9ZOS390)
  if (OMRPORT_ERROR_SHMEM_ZOS_STORAGE_KEY_READONLY == rc) {
    // we no longer have a storage key.
    //    Trc_SHR_OSC_Event_OpenReadOnly_StorageKey();
    _configOptions.setReadOnlyOpenMode();
      // _openMode |= J9OSCACHE_OPEN_MODE_DO_READONLY;
    perm = OMRSH_SHMEM_PERM_READ;
    rc = OpenSysVMemoryHelper(cacheName, perm, &localLastErrorInfo);
  }
#endif

  if (OMRPORT_ERROR_SHMEM_OPFAILED == rc) {
    // J9 specific.
//#if !defined(WIN32)
//    if (_activeGeneration >= 7) {
//#endif
    if (_configOptions.tryReadOnlyOnOpenFailure()) {//_openMode & J9OSCACHE_OPEN_MODE_TRY_READONLY_ON_FAIL) {
      _configOptions.setReadOnlyOpenMode();
      //_openMode |= J9OSCACHE_OPEN_MODE_DO_READONLY;
      perm = OMRSH_SHMEM_PERM_READ;
      rc = omrshmem_open(_cacheDirName, _config->_groupPerm, &_config->_shmhandle,
			 cacheName, _cacheSize, perm, OMRMEM_CATEGORY_CLASSES_SHC_CACHE,
			 OMRSHMEM_NO_FLAGS, &_controlFileStatus);
	/* if omrshmem_open is successful, portable error number is set to 0 */
      localLastErrorInfo.populate(OMRPORTLIB);
//	localLastErrorInfo.lastErrorCode = omrerror_last_error_number();
//	localLastErrorInfo.lastErrorMsg = omrerror_last_error_message();
      }
//#if !defined(WIN32)
//    }
//#endif
  }
  
  if (((rc == OMRPORT_INFO_SHMEM_OPENED) || (rc == OMRPORT_INFO_SHMEM_OPENED_STALE)) && (perm == OMRSH_SHMEM_PERM_READ)) {
    Trc_SHR_OSC_Event_OpenReadOnly();
    _runningReadOnly = true;
  }
  
  if (NULL != lastErrorInfo) {
    memcpy(lastErrorInfo, &localLastErrorInfo, sizeof(LastErrorInfo));
  }
  
  Trc_SHR_OSC_shmemOpenWrapper_Exit(rc, _cacheSize);
  return rc;
}

/**
 * This method performs additional checks to catch scenarios that are not handled by permission and/or mode settings provided by operating system,
 * to avoid any unintended access to shared memory.
 *
 * @param[in] lastErrorInfo	Pointer to store last portable error code and/or message
 *
 * @return enum SH_SysvShmAccess indicating if the process can access the shared memory or not
 */
SH_SysvShmAccess
OSSharedMemoryCache::checkSharedMemoryAccess(LastErrorInfo *lastErrorInfo)
{
  SH_SysvShmAccess shmAccess = OMRSH_SHM_ACCESS_ALLOWED;

  if (NULL != lastErrorInfo) {
    lastErrorInfo->lastErrorCode = 0;
  }

#if !defined(WIN32)
  IDATA rc = -1;
  OMRPortShmemStatistic statBuf;
  OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
  I_32 shmid = omrshmem_getid(_config->_shmhandle);
  
  memset(&statBuf, 0, sizeof(statBuf));
  rc = omrshmem_handle_stat(_config->_shmhandle, &statBuf);
  if (OMRPORT_INFO_SHMEM_STAT_PASSED == rc) {
    UDATA uid = omrsysinfo_get_euid();

    if (statBuf.cuid != uid) {
      if (statBuf.ouid == uid) {
	/* Current process is owner but not the creator of the shared memory.
	 * This implies external entity assigned ownership of the shared memory to this process.
	 */
	shmAccess = OMRSH_SHM_ACCESS_OWNER_NOT_CREATOR;
	Trc_SHR_OSC_Sysv_checkSharedMemoryAccess_OwnerNotCreator(uid, shmid, statBuf.cuid, statBuf.ouid);
      } else {
	UDATA gid = omrsysinfo_get_egid();
	bool sameGroup = false;
	
	if ((statBuf.ogid == gid) || (statBuf.cgid == gid)) {
	  sameGroup = true;
	  Trc_SHR_OSC_Sysv_checkSharedMemoryAccess_GroupIDMatch(gid, shmid, statBuf.cgid, statBuf.ogid);
	} else {
	  /* check supplementary groups */
	  U_32 *list = NULL;
	  IDATA size = 0;
	  IDATA i;

	  size = omrsysinfo_get_groups(&list, OMRMEM_CATEGORY_CLASSES_SHC_CACHE);
	  if (size > 0) {
	    for (i = 0; i < size; i++) {
	      if ((statBuf.ogid == list[i]) || (statBuf.cgid == list[i])) {
		sameGroup = true;
		Trc_SHR_OSC_Sysv_checkSharedMemoryAccess_SupplementaryGroupMatch(list[i], statBuf.cgid, statBuf.ogid, shmid);
		break;
	      }
	    }
	  } else {
	    shmAccess = OMRSH_SHM_ACCESS_CANNOT_BE_DETERMINED;
	    if (NULL != lastErrorInfo) {
	      lastErrorInfo->populate(OMRPORTLIB);
//	      lastErrorInfo->lastErrorCode = omrerror_last_error_number();
//	      lastErrorInfo->lastErrorMsg = omrerror_last_error_message();
	    }
	    Trc_SHR_OSC_Sysv_checkSharedMemoryAccess_GetGroupsFailed();
	    goto _end;
	  }
	  if (NULL != list) {
	    omrmem_free_memory(list);
	  }
	}
	if (sameGroup) {
	  /* This process belongs to same group as owner or creator of the shared memory. */
	  if (0 == _groupPerm) {
	    /* If 'groupAccess' option is not set, it implies this process wants to attach to a shared cache that it owns or created.
	     * But this process is neither creator nor owner of the semaphore set.
	     * This implies we should not allow this process to use the cache.
	     */
	    Trc_SHR_OSC_Sysv_checkSharedMemoryAccess_GroupAccessRequired(shmid);
	    
	    if (statBuf.perm.isGroupWriteable) {
	      /* The shared memory has group read-write permission set, so this process can use it if 'groupAcccess' option is specified. */
	      shmAccess = OMRSH_SHM_ACCESS_GROUP_ACCESS_REQUIRED;
	    } else {
	      /* The shared memory does not have group write permission set, so this process can use it in readonly mode. */
	      shmAccess = OMRSH_SHM_ACCESS_GROUP_ACCESS_READONLY_REQUIRED;
	    }
	  }
	} else {
	  /* This process does not belong to same group as owner or creator of the shared memory.
	   * Do not allow access to the cache.
	   */
	  shmAccess = OMRSH_SHM_ACCESS_OTHERS_NOT_ALLOWED;
	  Trc_SHR_OSC_Sysv_checkSharedMemoryAccess_OthersNotAllowed(shmid);
	}
      }
    }
  } else {
    shmAccess = OMRSH_SHM_ACCESS_CANNOT_BE_DETERMINED;
    if (NULL != lastErrorInfo) {
      lastErrorInfo->populate(OMRPORTLIB);
//      lastErrorInfo->lastErrorCode = omrerror_last_error_number();
//      lastErrorInfo->lastErrorMsg = omrerror_last_error_message();
    }
    Trc_SHR_OSC_Sysv_checkSharedMemoryAccess_ShmemStatFailed(shmid);
  }

_end:
#endif /* !defined(WIN32) */

  return shmAccess;
}

IDATA
OSSharedMemoryCache::OpenSysVMemoryHelper(const char* cacheName, U_32 perm, LastErrorInfo *lastErrorInfo)
{
  IDATA rc = -1;
  OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
  
  Trc_SHR_OSC_Sysv_OpenSysVMemoryHelper_Enter();

  //J9PortShcVersion versionData;
  //U_64 cacheVMVersion;
  //UDATA genVersion;
  UDATA action;
  UDATA flags = OMRSHMEM_NO_FLAGS;
  
  if (NULL != lastErrorInfo) {
    lastErrorInfo->lastErrorCode = 0;
  }

  // all of this -- J9 specific!
  //genVersion = getGenerationFromName(cacheName);
//	if (0 == getValuesFromShcFilePrefix(OMRPORTLIB, cacheName, &versionData)) {
//		goto done;
//	}
//
//	cacheVMVersion = getCacheVersionToU64(versionData.esVersionMajor, versionData.esVersionMinor);

  /* TODO: This is a utility static that decides the SysV control file name based on the generation and
   * version of the J9 SCC. Here, we just assume the user wants a regular control file, sharing
   * the same name as the cache, with no fancy versioning. A versioning solution should be designed
   * to replace this.
   */
  //SH_OSCachesysv::SysVCacheFileTypeHelper(cacheVMVersion, genVersion);
  action = OMRSH_SYSV_REGULAR_CONTROL_FILE;

  switch(action){
  case OMRSH_SYSV_REGULAR_CONTROL_FILE:
    /*
    if (OMR_ARE_ANY_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_STATS)) {
      flags |= OMRSHMEM_OPEN_FOR_STATS;
    } else if (OMR_ARE_ANY_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_DESTROY)) {
      flags |= OMRSHMEM_OPEN_FOR_DESTROY;
    } else if (OMR_ARE_ANY_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_DO_NOT_CREATE)) {
      flags |= OMRSHMEM_OPEN_DO_NOT_CREATE;
    } */

    flags = _configOptions.renderCreateOptionsToFlags();
    // J9 specific!
// #if defined(J9ZOS390)
//     if (0 != (_runtimeFlags & OMRSHR_RUNTIMEFLAG_ENABLE_STORAGEKEY_TESTING)) {
//       flags |=  OMRSHMEM_STORAGE_KEY_TESTING;
//       flags |=  _storageKeyTesting << OMRSHMEM_STORAGE_KEY_TESTING_SHIFT;
//     }
//     flags |= OMRSHMEM_PRINT_STORAGE_KEY_WARNING;
// #endif
      rc = omrshmem_open(_cacheDirName, _config->_groupPerm, &_config->_shmhandle, cacheName, _cacheSize, perm, OMRMEM_CATEGORY_CLASSES, flags, &_controlFileStatus);
      break;
  case OMRSH_SYSV_OLDER_EMPTY_CONTROL_FILE:
    rc = omrshmem_openDeprecated(_cacheDirName, _config->_groupPerm, &_config->_shmhandle, cacheName, perm, OMRSH_SYSV_OLDER_EMPTY_CONTROL_FILE, OMRMEM_CATEGORY_CLASSES);
    break;
  case OMRSH_SYSV_OLDER_CONTROL_FILE:
    rc = omrshmem_openDeprecated(_cacheDirName, _config->_groupPerm, &_config->_shmhandle, cacheName, perm, OMRSH_SYSV_OLDER_CONTROL_FILE, OMRMEM_CATEGORY_CLASSES);
    break;
  default:
    Trc_SHR_Assert_ShouldNeverHappen();
    break;
  }
 done:
  /* if above portLibrary call is successful, portable error number is set to 0 */
  if (NULL != lastErrorInfo) {
    lastErrorInfo->populate(OMRPORTLIB);
//    lastErrorInfo->lastErrorCode = omrerror_last_error_number();
//    lastErrorInfo->lastErrorMsg = omrerror_last_error_message();
  }
  Trc_SHR_OSC_Sysv_OpenSysVMemoryHelper_Exit(rc);
  return rc;
}

/**
 * Method to clean up semaphore set and shared memory resources as part of error handling.
 */
void
OSSharedMemoryCache::cleanupSysvResources(void)
{
  OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

  /* Setting handles to null prevents further use of this class, only if we haven't finished startup */
  if (NULL != _config->_shmhandle) {
    /* When ::startup() calls omrshmem_open() the cache is opened attached.
     * So, we must detach if we want clean up to work (see isCacheActive call below)
     */
    omrshmem_detach(&_config->_shmhandle);
  }

#if !defined(WIN32)
  /*If someone is still attached, don't destroy it*/
  /* isCacheActive isn't really accurate for Win32, so can't check */
  if(isCacheActive()) {
    if (NULL != _config->_semhandle) {
      omrshsem_deprecated_close(&_config->_semhandle);
      OSC_ERR_TRACE(OMRNLS_SHRC_OSCACHE_HANDLE_ERROR_ACTION_CLOSESEM);
    }
    if (NULL != _config->_shmhandle) {
      omrshmem_close(&_config->_shmhandle);
      OSC_ERR_TRACE(OMRNLS_SHRC_OSCACHE_HANDLE_ERROR_ACTION_CLOSESM);
    }
    return;
  }
#endif

  if ((NULL != _config->_semhandle) && (OMRSH_SEM_ACCESS_ALLOWED == _config->_semAccess)) {
#if defined(WIN32)
    if (omrshsem_deprecated_destroy(&_config->_semhandle) == 0) {
      OSC_TRACE(OMRNLS_SHRC_OSCACHE_HANDLE_ERROR_ACTION_DESTROYED_SEM);
    } else {
      I_32 errorno = omrerror_last_error_number();
      const char * errormsg = omrerror_last_error_message();
      OSC_ERR_TRACE(OMRNLS_SHRC_OSCACHE_HANDLE_ERROR_ACTION_DESTROYSEM_ERROR);
      OSC_ERR_TRACE1(OMRNLS_SHRC_OSCACHE_PORT_ERROR_NUMBER_SYSV_ERR_RECOVER, errorno);
      Trc_SHR_Assert_True(errormsg != NULL);
      OSC_ERR_TRACE1(OMRNLS_SHRC_OSCACHE_PORT_ERROR_MESSAGE_SYSV_ERR_RECOVER, errormsg);
    }
#else
    I_32 semid = omrshsem_deprecated_getid(_config->_semhandle);

    if (omrshsem_deprecated_destroy(&_semhandle) == 0) {
      OSC_TRACE1(OMRNLS_SHRC_OSCACHE_HANDLE_ERROR_ACTION_DESTROYED_SEM_WITH_SEMID, semid);
    } else {
      I_32 errorno = omrerror_last_error_number();
      const char * errormsg = omrerror_last_error_message();
      I_32 lastError = errorno | OMRPORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
      I_32 lastSysCall = errorno - lastError;

      if ((OMRPORT_ERROR_SYSV_IPC_SEMCTL_ERROR == lastSysCall) && (OMRPORT_ERROR_SYSV_IPC_ERRNO_EPERM == lastError)) {
	OSC_ERR_TRACE1(OMRNLS_SHRC_OSCACHE_HANDLE_ERROR_ACTION_DESTROYSEM_NOT_PERMITTED, semid);
      } else {
	OSC_ERR_TRACE1(OMRNLS_SHRC_OSCACHE_HANDLE_ERROR_ACTION_DESTROYSEM_ERROR_V1, semid);
	OSC_ERR_TRACE1(OMRNLS_SHRC_OSCACHE_PORT_ERROR_NUMBER_SYSV_ERR_RECOVER, errorno);
	Trc_SHR_Assert_True(errormsg != NULL);
	OSC_ERR_TRACE1(OMRNLS_SHRC_OSCACHE_PORT_ERROR_MESSAGE_SYSV_ERR_RECOVER, errormsg);
      }
    }
#endif
  }

  if ((NULL != _config->_shmhandle) && (OMRSH_SHM_ACCESS_ALLOWED == _config->_shmAccess)) {
#if defined(WIN32)
    if (omrshmem_destroy(_cacheDirName, _config->_groupPerm, &_config->_shmhandle) == 0) {
      OSC_TRACE(OMRNLS_SHRC_OSCACHE_HANDLE_ERROR_ACTION_DESTROYED_SHM);
    } else {
      // TODO: isn't this the same lastErrorInfo->populate()?? Why don't we use it?
      I_32 errorno = omrerror_last_error_number();
      const char * errormsg = omrerror_last_error_message();
      
      OSC_ERR_TRACE(OMRNLS_SHRC_OSCACHE_HANDLE_ERROR_ACTION_DESTROYSM_ERROR);
      OSC_ERR_TRACE1(OMRNLS_SHRC_OSCACHE_PORT_ERROR_NUMBER_SYSV_ERR_RECOVER, errorno);
      Trc_SHR_Assert_True(errormsg != NULL);
      OSC_ERR_TRACE1(OMRNLS_SHRC_OSCACHE_PORT_ERROR_MESSAGE_SYSV_ERR_RECOVER, errormsg);
    }
#else
    I_32 shmid = omrshmem_getid(_config->_shmhandle);

    if (omrshmem_destroy(_cacheDirName, _config->_groupPerm, &_config->_shmhandle) == 0) {
      OSC_TRACE1(OMRNLS_SHRC_OSCACHE_HANDLE_ERROR_ACTION_DESTROYED_SHM_WITH_SHMID, shmid);
    } else {
      I_32 errorno = omrerror_last_error_number();
      const char * errormsg = omrerror_last_error_message();
      
      I_32 lastError = errorno | OMRPORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
      I_32 lastSysCall = errorno - lastError;

      if ((OMRPORT_ERROR_SYSV_IPC_SHMCTL_ERROR == lastSysCall) && (OMRPORT_ERROR_SYSV_IPC_ERRNO_EPERM == lastError))
	{
	  OSC_ERR_TRACE1(OMRNLS_SHRC_OSCACHE_HANDLE_ERROR_ACTION_DESTROYSHM_NOT_PERMITTED, shmid);
	} else {
	OSC_ERR_TRACE1(OMRNLS_SHRC_OSCACHE_HANDLE_ERROR_ACTION_DESTROYSM_ERROR_V1, shmid);
	OSC_ERR_TRACE1(OMRNLS_SHRC_OSCACHE_PORT_ERROR_NUMBER_SYSV_ERR_RECOVER, errorno);
	Trc_SHR_Assert_True(errormsg != NULL);
	OSC_ERR_TRACE1(OMRNLS_SHRC_OSCACHE_PORT_ERROR_MESSAGE_SYSV_ERR_RECOVER, errormsg);
      }
    }
#endif
  }
}

#if !defined(WIN32)
/**
 * This method checks whether the group access of the semaphore is successfully set when a new cache is created with "groupAccess" suboption
 *
 * @param[in] lastErrorInfo	Pointer to store last portable error code and/or message.
 *
 * @return -1 Failed to get the stats of the semaphore.
 * 			0 Group access is not set.
 * 			1 Group access is set.
 */
I_32
OSSharedMemoryCache::verifySemaphoreGroupAccess(LastErrorInfo *lastErrorInfo)
{
  I_32 rc = 1;
  OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
  OMRPortShsemStatistic statBuf;

  memset(&statBuf, 0, sizeof(statBuf));
  if (OMRPORT_INFO_SHSEM_STAT_PASSED != omrshsem_deprecated_handle_stat(_config->_semhandle, &statBuf)) {
    if (NULL != lastErrorInfo) {
      lastErrorInfo->populate(_portLibrary);
    }
    rc = -1;
  } else {
    if ((1 != statBuf.perm.isGroupWriteable) || (1 != statBuf.perm.isGroupReadable)) {
      rc = 0;
    }
  }

  return rc;
}

/**
 * This method checks whether the group access of the shared memory is successfully set when a new cache is created with "groupAccess" suboption
 *
 * @param[in] lastErrorInfo	Pointer to store last portable error code and/or message.
 *
 * @return -1 Failed to get the stats of the shared memory.
 * 			0 Group access is not set.
 * 			1 Group access is set.
 */
I_32
OSSharedMemoryCache::verifySharedMemoryGroupAccess(LastErrorInfo *lastErrorInfo)
{
  I_32 rc = 1;
  OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
  OMRPortShmemStatistic statBuf;

  memset(&statBuf, 0, sizeof(statBuf));
  if (OMRPORT_INFO_SHMEM_STAT_PASSED != omrshmem_handle_stat(_config->_shmhandle, &statBuf)) {
    if (NULL != lastErrorInfo) {
      lastErrorInfo->populate(_portLibrary);
    }

    rc = -1;

  } else {
    if ((1 != statBuf.perm.isGroupWriteable) || (1 != statBuf.perm.isGroupReadable)) {
      rc = 0;
    }
  }

  return rc;
}
#endif /* !defined(WIN32) */

