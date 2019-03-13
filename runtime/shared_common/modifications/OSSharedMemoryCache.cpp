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

#include "sharedconsts.h"
#include "shrnls.h"
#include "ut_omrshr.h"

#include "OSCacheConfigOptions.hpp"
#include "OSCacheUtils.hpp"
#include "OSSharedMemoryCache.hpp"

OSSharedMemoryCache::OSSharedMemoryCache(OMRPortLibrary* library,
					 const char* cacheName,
					 const char* cacheDirName,
					 IDATA numLocks,
					 OSCacheConfigOptions* configOptions)
  : OSCacheImpl(library, configOptions, numLocks)  
{
  // I need to revise the arguments to this trace message.
  // Trc_SHR_OSC_Constructor_Entry(cacheName, piconfig->sharedClassCacheSize, createFlag);
  initialize();
  // expect the open mode has been set in the configOptions object already.
  // configOptions.setOpenMode(openMode);
  startup(cacheName, cacheDirName);
  Trc_SHR_OSC_Constructor_Exit(cacheName);
}

void
OSSharedMemoryCache::initialize()
{
  commonInit();

  _attachCount = 0;
  _config->_shmhandle = NULL;
  _config->_semhandle = NULL;
  // _actualCacheSize = 0;
  _shmFileName = NULL;
  _semFileName = NULL;
  _startupCompleted = false;
  _openSharedMemory = false;
  _policies = NULL;
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

  if (_configOptions->groupAccessEnabled()) {
    _config->_groupPerm = 1;
  }

  // J9 specific.
  // versionData->cacheType = OMRPORT_SHR_CACHE_TYPE_NONPERSISTENT;
  //  _cacheSize = (piconfig!= NULL) ? (U_32)piconfig->sharedClassCacheSize : (U_32)defaultCacheSize;
  //  _initializer = i;
    
  _config->_totalNumSems = _numLocks + 1;		/* +1 because of header mutex */
  _userSemCntr = 0;

  retryCount = OMRSH_OSCACHE_RETRYMAX;

  // J9 specific.
  // _storageKeyTesting = storageKeyTesting;

  // These are taken from the ConfigOptions object inside OSCacheImpl now.
//  IDATA openMode = _configOptions->openMode();
//  IDATA cacheDirPermissions = _configOptions->cacheDirPermissions();

  // this is how commonStartup was broken up, into these next two blocks.
  if(initCacheDirName(ctrlDirName) != 0) {
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
    OSC_ERR_TRACE(_configOptions, J9NLS_SHRC_OSCACHE_ALLOC_FAILED);
    return false;
  }

  //TODO: this leaves open the question of how to determine and write the value of the _semFileName string.
  // J9 specific:
  //getCacheVersionAndGen(OMRPORTLIB, vm, _semFileName, semLength, cacheName, versionData, _activeGeneration, false);
#endif

  _policies = constructSharedMemoryPolicy();
  
  while (retryCount > 0) {
    IDATA rc;

    if(_configOptions->readOnlyOpenMode()) {
      if (!OSCacheUtils::statCache(_portLibrary, _cacheLocation, _shmFileName, false)) {
	OSC_ERR_TRACE(_configOptions, J9NLS_SHRC_OSCACHE_STARTUP_CACHE_CREATION_NOT_ALLOWED_READONLY_MODE);
	Trc_SHR_OSC_startup_cacheCreationNotAllowed_readOnlyMode();
	rc = OS_SHARED_MEMORY_CACHE_FAILURE;
	break;
      }

      /* Don't get the semaphore set when running read-only, but pretend that we did */
      shsemrc = OMRPORT_INFO_SHSEM_OPENED;
      _config->_semhandle = NULL;
    } else {
#if !defined(WIN32)
      shsemrc = _policies->openSharedSemaphore(&lastErrorInfo); // OpenSysVSemaphoreHelper(&lastErrorInfo);
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

      UDATA flags = _configOptions->renderCreateOptionsToFlags();
      shsemrc = omrshsem_deprecated_open(_cacheLocation, _config->_groupPerm,
					 &_config->_semhandle, _semFileName,
					 (int)_config->_totalNumSems, 0, flags, NULL);
      lastErrorInfo.populate(_portLibrary);
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
      if (!_configOptions->openToDestroyExistingCache()) {
	// try to open the cache in read-only mode if we're not opening to cache to destroy it.
	_configOptions->setReadOnlyOpenMode();
      }

      shsemrc = OMRPORT_INFO_SHSEM_CREATED;
      _config->_semhandle = NULL;
    }

    switch(shsemrc) {
    case OMRPORT_INFO_SHSEM_CREATED:
#if !defined(WIN32)      
      if (_configOptions->groupAccessEnabled()) {
	/* Verify if the group access has been set */
	struct J9FileStat statBuf;
	char pathFileName[OMRSH_MAXPATH];

	I_32 semid = omrshsem_deprecated_getid(_config->_semhandle);
	I_32 groupAccessRc = _policies->verifySharedSemaphoreGroupAccess(&lastErrorInfo);

	if (0 == groupAccessRc) {
	  Trc_SHR_OSC_startup_setSemaphoreGroupAccessFailed(semid);
	  OSC_WARNING_TRACE1(_configOptions, J9NLS_SHRC_OSCACHE_SEMAPHORE_SET_GROUPACCESS_FAILED, semid);
	} else if (-1 == groupAccessRc) {
	  /* Fail to get stats of the semaphore */
	  Trc_SHR_OSC_startup_badSemaphoreStat(semid);
	  errorHandler(J9NLS_SHRC_OSCACHE_INTERNAL_ERROR_CHECKING_SEMAPHORE_ACCESS, &lastErrorInfo);
	  rc = OMRSH_OSCACHE_FAILURE;
	  break;
	}

	OSCacheUtils::getCachePathName(OMRPORTLIB, _cacheLocation, pathFileName, OMRSH_MAXPATH);//, _semFileName);
	/* No check for return value of getCachePathName() as it always return 0 */
	memset(&statBuf, 0, sizeof(statBuf));
	if (0 == omrfile_stat(pathFileName, 0, &statBuf)) {
	  if (1 != statBuf.perm.isGroupReadable) {
	    /* Control file needs to be group readable */
	    Trc_SHR_OSC_startup_setGroupAccessFailed(pathFileName);
	    OSC_WARNING_TRACE1(_configOptions, J9NLS_SHRC_OSCACHE_SEM_CONTROL_FILE_SET_GROUPACCESS_FAILED, pathFileName);
	  }
	} else {
	  Trc_SHR_OSC_startup_badFileStat(pathFileName);
	  lastErrorInfo.populate(_portLibrary);
	  errorHandler(J9NLS_SHRC_OSCACHE_ERROR_FILE_STAT, &lastErrorInfo);

	  rc = OMRSH_OSCACHE_FAILURE;
	  break;
	}
      }
#endif /* !defined(WIN32) */
      /* FALLTHROUGH */
    case OMRPORT_INFO_SHSEM_OPENED:
      /* Avoid any checks for semaphore access if
       * - running in readonly mode as we don't use semaphore, or
       * - user has specified a cache directory, or
       * - destroying an existing cache
       */
      if (!_configOptions->readOnlyOpenMode()  //OMR_ARE_NO_BITS_SET(_configOptions->_openMode, J9OSCACHE_OPEN_MODE_DO_READONLY)
	  && (OMRPORT_INFO_SHSEM_OPENED == shsemrc)
	  && (!_configOptions->isUserSpecifiedCacheDir())
	  && (!_configOptions->openToDestroyExistingCache())//OMR_ARE_NO_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_DESTROY))
	  ) {
	_config->_semAccess = _policies->checkSharedSemaphoreAccess(&lastErrorInfo);
      }

      /* Ignore _semAccess when opening cache for printing stats, but we do need it later to display cache usability */
      if (_configOptions->openToStatExistingCache()//OMR_ARE_ALL_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_STATS)
	  || (OMRSH_SEM_ACCESS_ALLOWED == _config->_semAccess)
      ) {
	IDATA headerMutexRc = 0;
	if (!_configOptions->restoreCheckEnabled()) { //OMR_ARE_NO_BITS_SET(_runtimeFlags, OMRSHR_RUNTIMEFLAG_RESTORE_CHECK)) {
	  /* When running "restoreFromSnapshot" utility, headerMutex has already been acquired in the first call of SH_OSCachesysv::startup()*/
	  //enterHeaderMutex(&lastErrorInfo);
	  headerMutexRc = _config->acquireHeaderWriteLock(_portLibrary, _cacheName, &lastErrorInfo);
	}

	if (0 == headerMutexRc) {
	  rc = openCache(_cacheLocation);//, (shsemrc == OMRPORT_INFO_SHSEM_CREATED));
	  if (!_configOptions->restoreCheckEnabled() || !_configOptions->restoreEnabled()) {
	      // OMR_ARE_NO_BITS_SET(_runtimeFlags, OMRSHR_RUNTIMEFLAG_RESTORE | OMRSHR_RUNTIMEFLAG_RESTORE_CHECK)) {
	    /* When running "restoreFromSnapshot" utility, do not release headerMutex here */
	    if (0 != _config->releaseHeaderWriteLock(_portLibrary, &lastErrorInfo)) {
	      errorHandler(J9NLS_SHRC_OSCACHE_ERROR_EXIT_HDR_MUTEX, &lastErrorInfo);
	      rc = OS_SHARED_MEMORY_CACHE_FAILURE;
	    }
	  }
	} else {
	  errorHandler(J9NLS_SHRC_OSCACHE_ERROR_ENTER_HDR_MUTEX, &lastErrorInfo);
	  rc = OS_SHARED_MEMORY_CACHE_FAILURE;
	}
      } else {
	switch (_config->_semAccess) {
	case OMRSH_SEM_ACCESS_CANNOT_BE_DETERMINED:
	  errorHandler(J9NLS_SHRC_OSCACHE_INTERNAL_ERROR_CHECKING_SEMAPHORE_ACCESS, &lastErrorInfo);
	  break;
	case OMRSH_SEM_ACCESS_OWNER_NOT_CREATOR:
	  errorHandler(J9NLS_SHRC_OSCACHE_SEMAPHORE_OWNER_NOT_CREATOR, NULL);
	  break;
	case OMRSH_SEM_ACCESS_GROUP_ACCESS_REQUIRED:
	  errorHandler(J9NLS_SHRC_OSCACHE_SEMAPHORE_GROUPACCESS_REQUIRED, NULL);
	  break;
	case OMRSH_SEM_ACCESS_OTHERS_NOT_ALLOWED:
	  errorHandler(J9NLS_SHRC_OSCACHE_SEMAPHORE_OTHERS_ACCESS_NOT_ALLOWED, NULL);
	  break;
	default:
	  Trc_SHR_Assert_ShouldNeverHappen();
	}
	rc = OS_SHARED_MEMORY_CACHE_FAILURE;
      }
      break;

    case OMRPORT_ERROR_SHSEM_OPFAILED:
    case OMRPORT_ERROR_SHSEM_OPFAILED_CONTROL_FILE_LOCK_FAILED:
    case OMRPORT_ERROR_SHSEM_OPFAILED_CONTROL_FILE_CORRUPT:
    case OMRPORT_ERROR_SHSEM_OPFAILED_SEMID_MISMATCH:
    case OMRPORT_ERROR_SHSEM_OPFAILED_SEM_KEY_MISMATCH:
    case OMRPORT_ERROR_SHSEM_OPFAILED_SEM_SIZE_CHECK_FAILED:
    case OMRPORT_ERROR_SHSEM_OPFAILED_SEM_MARKER_CHECK_FAILED:
    case OMRPORT_ERROR_SHSEM_OPFAILED_SEMAPHORE_NOT_FOUND:
      if(_configOptions->openToDestroyExistingCache()
	 && (OMRPORT_ERROR_SHSEM_OPFAILED_SEMAPHORE_NOT_FOUND == shsemrc)
      ) {
	/* No semaphore set was found when opening for
	   "destroy". Avoid printing any error message. */
	rc = OS_SHARED_MEMORY_CACHE_SUCCESS;
      } else {
	I_32 semid = 0;

	/* For some error codes, portlibrary stores semaphore set id obtained from the control file
	 * enabling us to display it in the error message. Retrieve the id and free memory allocated to handle.
	 */

	if (NULL != _config->_semhandle) {
	  semid = omrshsem_deprecated_getid(_config->_semhandle);
	  omrmem_free_memory(_config->_semhandle);
	}
	
	if ((OMRPORT_ERROR_SHSEM_OPFAILED == shsemrc) || (OMRPORT_ERROR_SHSEM_OPFAILED_SEMAPHORE_NOT_FOUND == shsemrc)) {
	  errorHandler(J9NLS_SHRC_OSCACHE_SEMAPHORE_OPFAILED, &lastErrorInfo);
	  if ((OMRPORT_ERROR_SHSEM_OPFAILED == shsemrc) && (0 != semid)) {
	    omrnls_printf( J9NLS_ERROR, J9NLS_SHRC_OSCACHE_SEMAPHORE_OPFAILED_DISPLAY_SEMID, semid);
	  }
	} else if (OMRPORT_ERROR_SHSEM_OPFAILED_CONTROL_FILE_LOCK_FAILED == shsemrc) {
	  errorHandler(J9NLS_SHRC_OSCACHE_SEMAPHORE_OPFAILED_CONTROL_FILE_LOCK_FAILED, &lastErrorInfo);
	} else if (OMRPORT_ERROR_SHSEM_OPFAILED_CONTROL_FILE_CORRUPT == shsemrc) {
	  errorHandler(J9NLS_SHRC_OSCACHE_SEMAPHORE_OPFAILED_CONTROL_FILE_CORRUPT, &lastErrorInfo);
	} else if (OMRPORT_ERROR_SHSEM_OPFAILED_SEMID_MISMATCH == shsemrc) {
	  errorHandler(J9NLS_SHRC_OSCACHE_SEMAPHORE_OPFAILED_SEMID_MISMATCH, &lastErrorInfo);
	  omrnls_printf( J9NLS_ERROR, J9NLS_SHRC_OSCACHE_SEMAPHORE_OPFAILED_DISPLAY_SEMID, semid);
	} else if (OMRPORT_ERROR_SHSEM_OPFAILED_SEM_KEY_MISMATCH == shsemrc) {
	  errorHandler(J9NLS_SHRC_OSCACHE_SEMAPHORE_OPFAILED_SEM_KEY_MISMATCH, &lastErrorInfo);
	  omrnls_printf( J9NLS_ERROR, J9NLS_SHRC_OSCACHE_SEMAPHORE_OPFAILED_DISPLAY_SEMID, semid);
	} else if (OMRPORT_ERROR_SHSEM_OPFAILED_SEM_SIZE_CHECK_FAILED == shsemrc) {
	  errorHandler(J9NLS_SHRC_OSCACHE_SEMAPHORE_OPFAILED_SEM_SIZE_CHECK_FAILED, &lastErrorInfo);
	  omrnls_printf( J9NLS_ERROR, J9NLS_SHRC_OSCACHE_SEMAPHORE_OPFAILED_DISPLAY_SEMID, semid);
	} else if (OMRPORT_ERROR_SHSEM_OPFAILED_SEM_MARKER_CHECK_FAILED == shsemrc) {
	  errorHandler(J9NLS_SHRC_OSCACHE_SEMAPHORE_OPFAILED_SEM_MARKER_CHECK_FAILED, &lastErrorInfo);
	  omrnls_printf( J9NLS_ERROR, J9NLS_SHRC_OSCACHE_SEMAPHORE_OPFAILED_DISPLAY_SEMID, semid);
	}
	/* Report any error that occurred during unlinking of control file */
	if (OMRPORT_INFO_CONTROL_FILE_UNLINK_FAILED == _controlFileStatus.status) {
	  omrnls_printf( J9NLS_ERROR, J9NLS_SHRC_OSCACHE_SEMAPHORE_CONTROL_FILE_UNLINK_FAILED, _semFileName);
	  OSC_ERR_TRACE1(_configOptions, J9NLS_SHRC_OSCACHE_PORT_ERROR_NUMBER, _controlFileStatus.errorCode);
	  OSC_ERR_TRACE1(_configOptions, J9NLS_SHRC_OSCACHE_PORT_ERROR_MESSAGE, _controlFileStatus.errorMsg);
	}
      }
      
      /* While opening shared cache for "destroy" if some error occurs
       * when opening the semaphore set, don't bail out just yet but
       * try to open the shared memory.
       */
      if (_configOptions->openToDestroyExistingCache()) {
	_config->_semhandle = NULL;
	/* Skip acquiring header mutex as the semaphore handle is NULL */
	// versionData is J9 specific.
	rc = openCache(_cacheLocation);//, false); //, versionData, false);
      } else if (_configOptions->tryReadOnlyOnOpenFailure()) {
	/* Try read-only mode for 'nonfatal' option only if shared memory control file exists
	 * because we can't create a new control file when running in read-only mode.
	 */
	if (OSCacheUtils::statCache(_portLibrary, _cacheLocation, _shmFileName, false)) {
	  OSC_TRACE(_configOptions, J9NLS_SHRC_OSCACHE_STARTUP_NONFATAL_TRY_READONLY);
	  Trc_SHR_OSC_startup_attemptNonfatalReadOnly();
	  _configOptions->setOpenMode(_configOptions->openMode() | J9OSCACHE_OPEN_MODE_DO_READONLY);
	  rc = OS_SHARED_MEMORY_CACHE_RESTART;
	} else {
	  rc = OS_SHARED_MEMORY_CACHE_FAILURE;
	}
      } else {
	rc = OS_SHARED_MEMORY_CACHE_FAILURE;
      }
      break;

    case OMRPORT_ERROR_SHSEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT:
      errorHandler(J9NLS_SHRC_OSCACHE_SEMAPHORE_WAIT_FOR_CREATION_MUTEX_TIMEDOUT, &lastErrorInfo);
      rc = OS_SHARED_MEMORY_CACHE_FAILURE;
      break;

    default:
      errorHandler(J9NLS_SHRC_OSCACHE_UNKNOWN_ERROR, &lastErrorInfo);
      rc = OS_SHARED_MEMORY_CACHE_FAILURE;
      break;
    }

    switch (rc) {
    case OS_SHARED_MEMORY_CACHE_CREATED:
      if (_configOptions->verboseEnabled()) {
	OSC_TRACE1(_configOptions, J9NLS_SHRC_OSCACHE_SHARED_CACHE_CREATEDA, _cacheName);
      }
#if !defined(WIN32)
      if (_configOptions->groupAccessEnabled()) {//OMR_ARE_ALL_BITS_SET(_openMode, J9OSCACHE_OPEN_MODE_GROUPACCESS)) {
	/* Verify if the group access has been set */
	struct J9FileStat statBuf;
	I_32 shmid = omrshmem_getid(_config->_shmhandle);
	I_32 groupAccessRc = _policies->verifySharedMemoryGroupAccess(&lastErrorInfo);

	if (0 == groupAccessRc) {
	  Trc_SHR_OSC_startup_setSharedMemoryGroupAccessFailed(shmid);
	  OSC_WARNING_TRACE1(_configOptions, J9NLS_SHRC_OSCACHE_SHARED_MEMORY_SET_GROUPACCESS_FAILED, shmid);
	} else if (-1 == groupAccessRc) {
	  Trc_SHR_OSC_startup_badSharedMemoryStat(shmid);
	  errorHandler(J9NLS_SHRC_OSCACHE_INTERNAL_ERROR_CHECKING_SHARED_MEMORY_ACCESS, &lastErrorInfo);
	  retryCount = 0;
	  continue;
	}

	memset(&statBuf, 0, sizeof(statBuf));
	if (0 == omrfile_stat(_cachePathName, 0, &statBuf)) {
	  if (1 != statBuf.perm.isGroupReadable) {
	    /* Control file needs to be group readable */
	    Trc_SHR_OSC_startup_setGroupAccessFailed(_cachePathName);
	    OSC_WARNING_TRACE1(_configOptions, J9NLS_SHRC_OSCACHE_SHM_CONTROL_FILE_SET_GROUPACCESS_FAILED, _cachePathName);
	  }
	} else {
	  Trc_SHR_OSC_startup_badFileStat(_cachePathName);
	  lastErrorInfo.populate(_portLibrary);
//	  lastErrorInfo.lastErrorCode = omrerror_last_error_number();
//	  lastErrorInfo.lastErrorMsg = omrerror_last_error_message();
	  errorHandler(J9NLS_SHRC_OSCACHE_ERROR_FILE_STAT, &lastErrorInfo);
	  retryCount = 0;
	  continue;
	}
      }
#endif /* !defined(WIN32) */
      setError(OMRSH_OSCACHE_CREATED);
      getTotalSize();
      Trc_SHR_OSC_startup_Exit_Created(cacheName);
      _startupCompleted = true;
      
      return true;

    case OS_SHARED_MEMORY_CACHE_OPENED:
      if (_configOptions->verboseEnabled()) {
	if (_runningReadOnly) {
	  OSC_TRACE1(_configOptions, J9NLS_SHRC_OSCACHE_SYSV_STARTUP_OPENED_READONLY, _cacheName);
	} else {
	  OSC_TRACE1(_configOptions, J9NLS_SHRC_OSCACHE_SHARED_CACHE_OPENEDA, _cacheName);
	}
      }
      setError(OMRSH_OSCACHE_OPENED);
      getTotalSize();
      Trc_SHR_OSC_startup_Exit_Opened(cacheName);
      _startupCompleted=true;
      
      return true;

    case OS_SHARED_MEMORY_CACHE_RESTART:
      Trc_SHR_OSC_startup_attempt_Restart(cacheName);
      break;

    case OS_SHARED_MEMORY_CACHE_FAILURE:
      retryCount = 0;
      continue;

    case OS_SHARED_MEMORY_CACHE_NOT_EXIST:
      /* Currently, this case occurs only when OMRSH_OSCACHE_OPEXIST_STATS, OMRSH_OSCACHE_OPEXIST_DESTROY or OMRSH_OSCACHE_OPEXIST_DO_NOT_CREATE is set and shared memory does not exist. */
      setError(OMRSH_OSCACHE_NO_CACHE);
      Trc_SHR_OSC_startup_Exit_NoCache(cacheName);
      return false;

    default:
      break;
    }

    retryCount--;
  }

  setError(OMRSH_OSCACHE_FAILURE);
  Trc_SHR_OSC_startup_Exit_Failed(cacheName);
  return false;  
}

/**
 * Returns if the cache is accessible by current user or not
 *
 * @return enum SH_CacheAccess
 */
SH_CacheAccess
OSSharedMemoryCache::isCacheAccessible() const
{
  if (OMRSH_SHM_ACCESS_ALLOWED == _config->_shmAccess) {
    return J9SH_CACHE_ACCESS_ALLOWED;
  } else if (OMRSH_SHM_ACCESS_GROUP_ACCESS_REQUIRED == _config->_shmAccess) {
    return J9SH_CACHE_ACCESS_ALLOWED_WITH_GROUPACCESS;
  } else if (OMRSH_SHM_ACCESS_GROUP_ACCESS_READONLY_REQUIRED == _config->_shmAccess) {
    return J9SH_CACHE_ACCESS_ALLOWED_WITH_GROUPACCESS_READONLY;
  } else {
    return J9SH_CACHE_ACCESS_NOT_ALLOWED;
  }
}

/**
 * This function restore a non-persistent cache from its snapshot file, startup and walk the restored cache to check for corruption.
 *
 * @param[in] vm The current OMR_VM
 * @param[in] cacheName The name of the cache
 * @param[in] numLocks The number of locks to be initialized
 * @param[in] i Pointer to an initializer to be used to initialize the data area of the new cache
 * @param[in, out] cacheExist True if the cache to be restored already exits, false otherwise
 *
 * @return 0 on success and -1 on failure
 */
// IDATA
// OSSharedMemoryCache::restoreFromSnapshot(const char* cacheName, UDATA numLocks, bool& cacheExists)
// {
//   OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
//   IDATA rc = 0;
//   char cacheDirName[OMRSH_MAXPATH];		/* OMRSH_MAXPATH defined to be EsMaxPath which is 1024 */
//   //  char nameWithVGen[CACHE_ROOT_MAXLEN];	/* CACHE_ROOT_MAXLEN defined to be 88 */
//   char pathFileName[OMRSH_MAXPATH];
//   //  J9PortShcVersion versionData;
//   char fullCacheName[CACHE_ROOT_MAXLEN]; // replaces nameWithVGen.
//   IDATA fd = 0;
// 
//   const char* ctrlDirName = _cacheLocation; // for: vm->sharedClassConfig->ctrlDirName;
// 
//   Trc_SHR_OSC_Sysv_restoreFromSnapshot_Entry();
// 
//   // _verboseFlags = vm->sharedClassConfig->verboseFlags;
//   //	setCurrentCacheVersion(vm, J2SE_VERSION(vm), &versionData);
//   // versionData.cacheType = OMRPORT_SHR_CACHE_TYPE_SNAPSHOT;
// 
//   if (-1 == OSCacheUtils::getCacheDirName(OMRPORTLIB, ctrlDirName, cacheDirName, OMRSH_MAXPATH))//, OMRPORT_SHR_CACHE_TYPE_SNAPSHOT))
//   {
//     Trc_SHR_OSC_Sysv_restoreFromSnapshot_getCacheDirFailed();
//     OSC_ERR_TRACE(_configOptions, J9NLS_SHRC_GETSNAPSHOTDIR_FAILED);
//     rc = -1;
//     goto done;
//   }
// 
//   // SH_OSCache::getCacheVersionAndGen(OMRPORTLIB, vm, nameWithVGen, CACHE_ROOT_MAXLEN, cacheName, &versionData, OSCACHE_CURRENT_CACHE_GEN, false);
//   /* No check for the return value of getCachePathName() as it always returns 0 */
//   OSCacheUtils::getCachePathName(OMRPORTLIB, cacheDirName, pathFileName, OMRSH_MAXPATH, fullCacheName);
//   fd = omrfile_open(pathFileName, EsOpenRead | EsOpenWrite, 0);
// 
//   if (-1 == fd) {
//     I_32 errorno = omrerror_last_error_number();
// 
//     if (OMRPORT_ERROR_FILE_NOENT == errorno) {
//       Trc_SHR_OSC_Sysv_restoreFromSnapshot_fileNotFound(pathFileName);
//       OSC_ERR_TRACE1(_configOptions, J9NLS_SHRC_OSCACHE_ERROR_SNAPSHOT_FILE_NOT_FOUND, pathFileName);
//     } else {
//       const char * errormsg = omrerror_last_error_message();
// 
//       Trc_SHR_OSC_Sysv_restoreFromSnapshot_fileOpenFailed(pathFileName);
//       OSC_ERR_TRACE1(_configOptions, J9NLS_SHRC_PORT_ERROR_NUMBER, errorno);
//       Trc_SHR_Assert_True(errormsg != NULL);
//       OSC_ERR_TRACE1(_configOptions, J9NLS_SHRC_PORT_ERROR_MESSAGE, errormsg);
//       OSC_ERR_TRACE1(_configOptions, J9NLS_SHRC_ERROR_SNAPSHOT_FILE_OPEN, pathFileName);
//     }
//     rc = -1;
//   } else {
//     I_64 fileSize = omrfile_flength(fd);
//     LastErrorInfo lastErrorInfo;
//     I_32 openMode = 0;
//     SH_CacheFileAccess cacheFileAccess = OMRSH_CACHE_FILE_ACCESS_ALLOWED;
// 
//     // we expect that _configOptions has been configured with the
//     // restore settingsprior to this function being called. In J9,
//     // this function configures the restored cache as it goes about
//     // restoring it.
//     if (_configOptions->groupAccessEnabled()) { //OMR_ARE_ALL_BITS_SET(vm->sharedClassConfig->runtimeFlags, OMRSHR_RUNTIMEFLAG_ENABLE_GROUP_ACCESS)) {
//       openMode |= J9OSCACHE_OPEN_MODE_GROUPACCESS;
//       _config->_groupPerm = 1;
//     } else {
//       _config->_groupPerm = 0;
//     }
// 
//     cacheFileAccess = OSMemoryMappedCacheUtils::checkCacheFileAccess(OMRPORTLIB, fd, openMode, &lastErrorInfo);
// 
//     if (OMRSH_CACHE_FILE_ACCESS_ALLOWED != cacheFileAccess) {
//       switch (cacheFileAccess) {
//       case OMRSH_CACHE_FILE_ACCESS_GROUP_ACCESS_REQUIRED:
// 	OSC_ERR_TRACE1(_configOptions, J9NLS_SHRC_OSCACHE_SNAPSHOT_GROUPACCESS_REQUIRED, pathFileName);
// 	break;
//       case OMRSH_CACHE_FILE_ACCESS_OTHERS_NOT_ALLOWED:
// 	OSC_ERR_TRACE1(_configOptions, J9NLS_SHRC_OSCACHE_SNAPSHOT_OTHERS_ACCESS_NOT_ALLOWED, pathFileName);
// 	break;
//       case OMRSH_CACHE_FILE_ACCESS_CANNOT_BE_DETERMINED:
// 	printErrorMessage(&lastErrorInfo);
// 	OSC_ERR_TRACE1(_configOptions, J9NLS_SHRC_OSCACHE_SNAPSHOT_INTERNAL_ERROR_CHECKING_CACHEFILE_ACCESS, pathFileName);
// 	break;
//       default:
// 	Trc_SHR_Assert_ShouldNeverHappen();
//       }
//       omrfile_close(fd);
//       rc = -1;
//       Trc_SHR_OSC_Sysv_restoreFromSnapshot_fileAccessNotAllowed(pathFileName);
//       goto done;
//     }
// 
//     if ((fileSize < MIN_CC_SIZE) || (fileSize > MAX_CC_SIZE)) {
//       Trc_SHR_OSC_Sysv_restoreFromSnapshot_fileSizeInvalid(pathFileName, fileSize);
//       OSC_ERR_TRACE4(J9NLS_SHRC_OSCACHE_ERROR_SNAPSHOT_FILE_LENGTH, pathFileName, fileSize,
// 		     MIN_CC_SIZE, MAX_CC_SIZE);
//       rc = -1;
//       /* lock the file to prevent reading and writing */
//     } else if (omrfile_lock_bytes(fd, OMRPORT_FILE_WRITE_LOCK | OMRPORT_FILE_WAIT_FOR_LOCK, 0, fileSize) < 0) {
//       I_32 errorno = omrerror_last_error_number();
//       const char * errormsg = omrerror_last_error_message();
//       
//       Trc_SHR_OSC_Sysv_restoreFromSnapshot_fileLockFailed(pathFileName);
//       OSC_ERR_TRACE1(_configOptions, J9NLS_SHRC_PORT_ERROR_NUMBER, errorno);
//       Trc_SHR_Assert_True(errormsg != NULL);
//       OSC_ERR_TRACE1(_configOptions, J9NLS_SHRC_PORT_ERROR_MESSAGE, errormsg);
//       OSC_ERR_TRACE1(_configOptions, J9NLS_SHRC_ERROR_SNAPSHOT_FILE_LOCK, pathFileName);
//       rc = -1;
//     } else {
//       // the commented lines in this section are all J9 specific.
//       
//       //OMRSharedCachePreinitConfig* piconfig = vm->sharedCachePreinitConfig;
//       //OMR_VMThread* currentThread = omr_vmthread_getCurrent(vm); //vm->internalVMFunctions->currentVMThread(vm);
//       bool rcStartup = false;
// 
//       //piconfig->sharedClassCacheSize = (UDATA)fileSize;
//       //versionData.cacheType = OMRPORT_SHR_CACHE_TYPE_NONPERSISTENT;
//       //SH_OSCache::getCacheVersionAndGen(OMRPORTLIB, vm, nameWithVGen, CACHE_ROOT_MAXLEN, cacheName, &versionData, OSCACHE_CURRENT_CACHE_GEN, true);
//       if (1 == OSCacheUtils::statCache(OMRPORTLIB, cacheDirName, fullCacheName, false)) {//nameWithVGen, false)) {
// #if !defined(WIN32)
// 	OMRPortShmemStatistic statbuf;
// 	/* The shared memory may be removed without deleting the control files. So check the existence of the shared memory */
// 	IDATA ret = OSSharedCacheUtils::StatSysVMemoryHelper(OMRPORTLIB, cacheDirName, _config->_groupPerm, nameWithVGen, &statbuf);
// 
// 	if (0 == ret) {
// #endif /* !defined(WIN32) */
// 	  // Trc_SHR_OSC_Sysv_restoreFromSnapshot_cacheExist1(currentThread);
// 	  OSC_ERR_TRACE1(_configOptions, J9NLS_SHRC_OSCACHE_ERROR_RESTORE_EXISTING_CACHE, cacheName);
// 	  cacheExist = true;
// 	  omrfile_close(fd);
// 	  rc = -1;
// 	  goto done;
// #if !defined(WIN32)
// 	}
// #endif /* !defined(WIN32) */
//       }
// 
//       _configOptions->_openMode = openMode;
//       _numLocks = numLocks;
//       
//       rcStartup = startup(cacheName, ctrlDirName);
//       
//       if (false == rcStartup) {
// 	// Trc_SHR_OSC_Sysv_restoreFromSnapshot_cacheStartupFailed1(currentThread);
// 	OSC_ERR_TRACE(_configOptions, J9NLS_SHRC_OSCACHE_ERROR_STARTUP_CACHE);
// 	destroy(false);
// 	rc = -1;
//       } else if (OMRSH_OSCACHE_CREATED != getError()) {
// 	/* Another VM has created the cache */
// 	OSC_ERR_TRACE1(_configOptions, J9NLS_SHRC_OSCACHE_ERROR_RESTORE_EXISTING_CACHE, cacheName);
// 	Trc_SHR_OSC_Sysv_restoreFromSnapshot_cacheExist2(currentThread);
// 	cacheExist = true;
// 	rc = -1;
//       } else {
// 	//TODO: resume here. We haven't adapted CacheMap yet, so for now it's best
// 	//visited later.
// 	SH_CacheMap* cm = (SH_CacheMap *)vm->sharedClassConfig->sharedClassCache;
// 	bool cacheHasIntegrity = false;
// 	I_32 semid = 0;
// 	U_16 theVMCntr = 0;
// 	OSCachesysv_header_version_current*  osCacheSysvHeader = NULL;
// 	OMRSharedCacheHeader* theca = (OMRSharedCacheHeader *)attach(currentThread, &versionData);
// 	IDATA nbytes = (IDATA)fileSize;
// 	IDATA fileRc = 0;
// 
// 	if (NULL == theca) {
// 	  Trc_SHR_OSC_Sysv_restoreFromSnapshot_cacheAttachFailed(currentThread);
// 	  OSC_ERR_TRACE(_configOptions, J9NLS_SHRC_OSCACHE_SHMEM_ATTACH);
// 	  destroy(false);
// 	  omrfile_close(fd);
// 	  rc = -1;
// 	  goto done;
// 	}
// 
// 	osCacheSysvHeader = (OSCachesysv_header_version_current *)(_headerStart);
// 	semid = osCacheSysvHeader->attachedSemid;
// 	theVMCntr = theca->vmCntr;
// 	
// 				Trc_SHR_Assert_Equals(theVMCntr, 0);
// 				
// 				fileRc = omrfile_read(fd, osCacheSysvHeader, nbytes);
// 				if (fileRc < 0) {
// 					I_32 errorno = omrerror_last_error_number();
// 					const char * errormsg = omrerror_last_error_message();
// 
// 					Trc_SHR_OSC_Sysv_restoreFromSnapshot_fileReadFailed1(currentThread, pathFileName);
// 					OSC_ERR_TRACE1(_configOptions, J9NLS_SHRC_PORT_ERROR_NUMBER, errorno);
// 					Trc_SHR_Assert_True(errormsg != NULL);
// 					OSC_ERR_TRACE1(_configOptions, J9NLS_SHRC_PORT_ERROR_MESSAGE, errormsg);
// 					OSC_ERR_TRACE1(_configOptions, J9NLS_SHRC_OSCACHE_ERROR_SNAPSHOT_FILE_READ, pathFileName);
// 					destroy(false);
// 					omrfile_close(fd);
// 					rc = -1;
// 					goto done;
// 				} else if (nbytes != fileRc) {
// 					Trc_SHR_OSC_Sysv_restoreFromSnapshot_fileReadFailed2(currentThread, pathFileName, nbytes, fileRc);
// 					OSC_ERR_TRACE1(_configOptions, J9NLS_SHRC_OSCACHE_ERROR_SNAPSHOT_FILE_READ, pathFileName);
// 					destroy(false);
// 					omrfile_close(fd);
// 					rc = -1;
// 					goto done;
// 				}
// 				theca->vmCntr = theVMCntr;
// 				osCacheSysvHeader->attachedSemid = semid;
// 				/* remove OMRSHR_RUNTIMEFLAG_RESTORE and startup the cache again to check for corruption, cache header will be checked in SH_CacheMap::startup() */
// 				vm->sharedClassConfig->runtimeFlags &= ~OMRSHR_RUNTIMEFLAG_RESTORE;
// 				vm->sharedClassConfig->runtimeFlags |= OMRSHR_RUNTIMEFLAG_RESTORE_CHECK;
// 				/* free memory allocated by SH_OSCachesysv::startup() */
// 				cleanup();
// 				rc = cm->startup(currentThread, piconfig, cacheName, ctrlDirName, vm->sharedCacheAPI->cacheDirPerm, NULL, &cacheHasIntegrity);
// 				/* verboseFlags might be set to 0 in cm->startup(), set it again to ensure the NLS can be printed out */
// 				_verboseFlags = vm->sharedClassConfig->verboseFlags;
// 				if (0 == rc) {
// 					IDATA ret = 0;
// 					LastErrorInfo lastErrorInfo;
// 
// 					/* Header mutex is acquired and not released in the first call of SH_OSCachesysv::startup(), release here */
// 					ret = exitHeaderMutex(&lastErrorInfo);
// 					if (0 == ret) {
// 						/* set osCacheSysvHeader to current _headerStart as it is detached in cleanup() and re-attached in cm->startup() */
// 						osCacheSysvHeader = (OSCachesysv_header_version_current *)_headerStart;
// 						/* To prevent the cache being opened by another JVM in read-only mode, osCacheSysvHeader->oscHdr.cacheInitComplete is always 0
// 						 * before the restoring operation is finished. Set it to 1 here
// 						 */
// 						osCacheSysvHeader->oscHdr.cacheInitComplete = 1;
// 					} else {
// 						Trc_SHR_OSC_Sysv_restoreFromSnapshot_headerMutexReleaseFailed(currentThread);
// 						errorHandler(J9NLS_SHRC_OSCACHE_ERROR_EXIT_HDR_MUTEX, &lastErrorInfo);
// 						cm->destroy(currentThread);
// 						rc = -1;
// 					}
// 				} else {
// 					/* if the restored cache is corrupted, it is destroyed in SH_CacheMap::startup() */
// 					Trc_SHR_OSC_Sysv_restoreFromSnapshot_cacheStartupFailed2(currentThread);
// 				}
// 			}
// 		}
// 		/* file lock will be released when closed */
// 		omrfile_close(fd);
// 	}
// done:
// 	Trc_SHR_OSC_Sysv_restoreFromSnapshot_Exit(rc);
// 	return rc;  
// }

IDATA
OSSharedMemoryCache::detachRegion()
{
  IDATA rc = OS_SHARED_MEMORY_CACHE_FAILURE;
  OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

  Trc_SHR_OSC_detachRegion_Entry();

  if (_config->_shmhandle != NULL) {
    Trc_SHR_OSC_detachRegion_Debug(_config->getDataSectionLocation(), _config->getHeaderLocation());
    rc = omrshmem_detach(&_config->_shmhandle);
    
    if (rc == -1) {
      LastErrorInfo lastErrorInfo;
      lastErrorInfo.populate(_portLibrary);
//      lastErrorInfo.lastErrorCode = omrerror_last_error_number();
//      lastErrorInfo.lastErrorMsg = omrerror_last_error_message();
      errorHandler(J9NLS_SHRC_OSCACHE_SHMEM_DETACH, &lastErrorInfo);
    } else {
      rc = OS_SHARED_MEMORY_CACHE_SUCCESS;
    }

    _config->setHeaderLocation(NULL);
    _config->setDataSectionLocation(NULL);
  }

  Trc_SHR_OSC_detachRegion_Exit();
  return rc;
}

IDATA
OSSharedMemoryCache::detach()
{
  IDATA rc=OS_SHARED_MEMORY_CACHE_FAILURE;
  Trc_SHR_OSC_detach_Entry();

  if(_config->_shmhandle == NULL) {
    Trc_SHR_OSC_detach_Exit1();
    return OS_SHARED_MEMORY_CACHE_SUCCESS;
  }

  Trc_SHR_OSC_detach_Debug(_cacheName, _config->getDataSectionLocation());

  _attachCount--;

  if(_attachCount == 0) {
    detachRegion();
    rc=OS_SHARED_MEMORY_CACHE_SUCCESS;
  }

  Trc_SHR_OSC_detach_Exit();
  return rc;
}

/**
 * Attaches the shared memory into the process address space, and returns the address of the mapped
 * shared memory.
 *
 * This method send a request to the operating system to map the shared memory into the caller's address
 * space if it hasn't already been done so. This will also checks the memory region for the correct header
 * if the region is being mapped for the first time.
 *
 * @param [in] expectedVersionData  If not NULL, function checks the version data of the cache against the values in this struct
 *
 * @return The address of the memory mapped area for the caller's process - This is not guranteed to be the same
 * for two different process.
 *
 */
void *
OSSharedMemoryCache::attach() //OMR_VMThread *currentThread, J9PortShcVersion* expectedVersionData)
{
  //	OMR_VM *vm = currentThread->_vm;
  OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
  IDATA headerRc;

  Trc_SHR_OSC_attach_Entry();
  if (_config->_shmhandle == NULL) {
    /* _shmhandle == NULL means previous op failed */
    Trc_SHR_OSC_attach_Exit1();
    return NULL;
  }

  // J9 specific.
  /*
  if ((((_runtimeFlags & OMRSHR_RUNTIMEFLAG_CREATE_OLD_GEN) == 0) && (_activeGeneration != getCurrentCacheGen())) ||
      (((_runtimeFlags & OMRSHR_RUNTIMEFLAG_CREATE_OLD_GEN) != 0) && (_activeGeneration != getCurrentCacheGen()-1))
      ){
    Trc_SHR_OSC_attach_ExitWrongGen();
    return NULL;
  }
  */

  /* Cache is opened attached, the call here will simply return the
   * address memory already attached.
   *
   * Note: Unless ::detach was called ... which I believe does not currently occur.
   */
  //Trc_SHR_OSC_attach_Try_Attach1(UnitTest::unitTest);
  void* request = omrshmem_attach(_config->_shmhandle, OMRMEM_CATEGORY_CLASSES_SHC_CACHE);

  if (request == NULL) {
    LastErrorInfo lastErrorInfo;
    lastErrorInfo.populate(_portLibrary);    
//    lastErrorInfo.lastErrorCode = omrerror_last_error_number();
//    lastErrorInfo.lastErrorMsg = omrerror_last_error_message();
    errorHandler(J9NLS_SHRC_OSCACHE_SHMEM_ATTACH, &lastErrorInfo);
    _config->setDataSectionLocation(NULL);
    _attachCount = 0;
    
    Trc_SHR_OSC_attach_Exit2();
    return NULL;
  }

  Trc_SHR_OSC_attach_Debug1(request);
  //  Trc_SHR_OSC_attach_Debug2(sizeof(OSCachesysv_header_version_current));

  _config->setHeaderLocation(request);
  //  _headerStart = request;

  if ((headerRc = verifyCacheHeader()) != OMRSH_OSCACHE_HEADER_OK) {
    if ((headerRc == OMRSH_OSCACHE_HEADER_CORRUPT) || (headerRc == OMRSH_OSCACHE_SEMAPHORE_MISMATCH)) {
      /* Cache is corrupt, trigger hook to generate a system dump.
       * This is the last chance to get corrupt cache image in system dump.
       * After this point, cache is detached.
       */
//      if (_configOptions->disableCorruptCacheDumps()) {
//	//0 == (_runtimeFlags & OMRSHR_RUNTIMEFLAG_DISABLE_CORRUPT_CACHE_DUMPS)) {
//	// TRIGGER_J9HOOK_VM_CORRUPT_CACHE(vm->hookInterface, currentThread);
//      }
      setError(OMRSH_OSCACHE_CORRUPT);
    }
    // J9 specific:
    /* else if (headerRc == OMRSH_OSCACHE_HEADER_DIFF_BUILDID) {
       setError(OMRSH_OSCACHE_DIFF_BUILDID);
       }*/
    omrshmem_detach(&_config->_shmhandle);
    Trc_SHR_OSC_attach_ExitHeaderIsNotOk(headerRc);
    return NULL;
  }

  /*_dataStart is set here, and possibly initializeHeader if its a new cache */

  // this has to be overloaded by the header class.
  _config->_header->init(); //_config->_layout);
//  _dataStart = SHM_DATASTARTFROMHEADER(((OSCachesysv_header_version_current*)_headerStart));
//
//  _dataLength = SHM_CACHEDATASIZE(((OSCachesysv_header_version_current*)_headerStart)->oscHdr.size);
  _attachCount++;  
  
  if (_configOptions->verboseEnabled()) { //_verboseFlags & OMRSHR_VERBOSEFLAG_ENABLE_VERBOSE) {
    U_32 dataLength = _config->getDataSectionLength();
    OSC_TRACE2(_configOptions, J9NLS_SHRC_OSCACHE_ATTACH_SUCCESS, _cacheName, dataLength);
  }

  U_64* dataStart = _config->getDataSectionLocation();
  
  Trc_SHR_OSC_attach_Exit(dataStart);
  return dataStart;
}

/* OK, I am uhh, respectfully wondering why the lastErrorInfo object created by startup is not
   passed to this function. Seriously, why is that?? I will try to discover the answer! */
/* The caller should hold the mutex */
/* Me again. semCreated is not even used! Nowhere is it mentioned not even in comments. I am omitting
   it. */
IDATA
OSSharedMemoryCache::openCache(const char* cacheDirName) //, bool semCreated) , J9PortShcVersion* versionData, bool semCreated)
{
  /* we are attaching to existing cache! */
  Trc_SHR_OSC_openCache_Entry(_cacheName);
  IDATA rc;
  IDATA result = OS_SHARED_MEMORY_CACHE_FAILURE; //OS_SHARED_MEMORY_CACHE_FAILURE;
  LastErrorInfo lastErrorInfo;

  OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

  rc = _policies->openSharedMemoryWrapper(_shmFileName, &lastErrorInfo);
  Trc_SHR_OSC_openCache_shmem_open(_shmFileName, _cacheSize);

  switch (rc) {
  case OMRPORT_ERROR_SHMEM_OPEN_ATTACHED_FAILED:
    _openSharedMemory = true;
    /* FALLTHROUGH */
  case OMRPORT_ERROR_SHMEM_CREATE_ATTACHED_FAILED:
    /*We failed to attach to the memory.*/
    errorHandler(J9NLS_SHRC_OSCACHE_SHMEM_OPEN_ATTACHED_FAILED, &lastErrorInfo);
    Trc_SHR_OSC_openCache_ExitAttachFailed();
    result = OS_SHARED_MEMORY_CACHE_FAILURE;
    break;
  case OMRPORT_INFO_SHMEM_OPENED:
    /* Avoid any checks if
     * - user has specified a cache directory, or
     * - destroying an existing cache
     */
    if (!_configOptions->isUserSpecifiedCacheDir() && !_configOptions->openToDestroyExistingCache()) {
      //if (!_isUserSpecifiedCacheDir && (OMR_ARE_NO_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_DESTROY))) {
      _config->_shmAccess = _policies->checkSharedMemoryAccess(&lastErrorInfo);
    }
    /* Ignore _shmAccess when opening cache for printing stats, but we do need it later to display cache usability */
    if (_configOptions->openToStatExistingCache() // OMR_ARE_ALL_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_STATS)
	|| (OMRSH_SHM_ACCESS_ALLOWED == _config->_shmAccess)
	) {
      /* ALL SET */
      Trc_SHR_OSC_openCache_Exit_Opened(_cacheName);
      result = OS_SHARED_MEMORY_CACHE_OPENED;
    } else {
      switch (_config->_shmAccess) {
      case OMRSH_SHM_ACCESS_CANNOT_BE_DETERMINED:
	errorHandler(J9NLS_SHRC_OSCACHE_INTERNAL_ERROR_CHECKING_SHARED_MEMORY_ACCESS, &lastErrorInfo);
	break;
      case OMRSH_SHM_ACCESS_OWNER_NOT_CREATOR:
	errorHandler(J9NLS_SHRC_OSCACHE_SHARED_MEMORY_OWNER_NOT_CREATOR, NULL);
	break;
      case OMRSH_SHM_ACCESS_GROUP_ACCESS_REQUIRED:
	errorHandler(J9NLS_SHRC_OSCACHE_SHARED_MEMORY_GROUPACCESS_REQUIRED, NULL);
	break;
      case OMRSH_SHM_ACCESS_GROUP_ACCESS_READONLY_REQUIRED:
	errorHandler(J9NLS_SHRC_OSCACHE_SHARED_MEMORY_GROUPACCESS_READONLY_REQUIRED, NULL);
	break;
      case OMRSH_SHM_ACCESS_OTHERS_NOT_ALLOWED:
	errorHandler(J9NLS_SHRC_OSCACHE_SHARED_MEMORY_OTHERS_ACCESS_NOT_ALLOWED, NULL);
	break;
      default:
	Trc_SHR_Assert_ShouldNeverHappen();
      }
      Trc_SHR_OSC_openCache_ExitAccessNotAllowed(_config->_shmAccess);
      result = OMRSH_OSCACHE_FAILURE;
    }

    break;
  case OMRPORT_INFO_SHMEM_CREATED:
    /* We opened semaphore yet created the cache area -
     * we should set it up, but don't need to init semaphore
     */
    
    rc = _config->initializeHeader(cacheDirName, &lastErrorInfo); //versionData, lastErrorInfo);
    if(rc == OS_SHARED_MEMORY_CACHE_FAILURE) {
      Trc_SHR_OSC_openCache_Exit_CreatedHeaderInitFailed(_cacheName);
      result = OS_SHARED_MEMORY_CACHE_FAILURE;
      break;
    }
    Trc_SHR_OSC_openCache_Exit_Created(_cacheName);
    result = OS_SHARED_MEMORY_CACHE_CREATED;
    break;

  case OMRPORT_ERROR_SHMEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT:
    errorHandler(J9NLS_SHRC_OSCACHE_SHMEM_OPEN_WAIT_FOR_CREATION_MUTEX_TIMEDOUT, &lastErrorInfo);
    Trc_SHR_OSC_openCache_Exit4();
    result = OS_SHARED_MEMORY_CACHE_FAILURE;
    break;

  case OMRPORT_INFO_SHMEM_PARTIAL:
    /* If OMRPORT_INFO_SHMEM_PARTIAL then ::startup() was called by j9shr_destroy_cache().
     * Returning OS_SHARED_MEMORY_CACHE_OPENED will cause j9shr_destroy_cache() to call ::destroy(),
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
    if ((_configOptions->openToStatExistingCache() || _configOptions->openToDestroyExistingCache()
	 || _configOptions->openButDoNotCreate())
	//OMR_ARE_ANY_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_STATS | OMRSH_OSCACHE_OPEXIST_DESTROY | OMRSH_OSCACHE_OPEXIST_DO_NOT_CREATE)
	&& (OMRPORT_ERROR_SHMEM_OPFAILED_SHARED_MEMORY_NOT_FOUND == rc)
	) {
      if (_configOptions->openToDestroyExistingCache()) { //OMR_ARE_ALL_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_DESTROY)) {
	/* Absence of shared memory is equivalent to non-existing cache. Do not display any error message,
	 * but do call cleanupSysVResources() to remove any semaphore set in case we opened it successfully.
	 */
	cleanupSysvResources();
      } else if (_configOptions->openToStatExistingCache() || _configOptions->openButDoNotCreate()) {//OMR_ARE_ANY_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_STATS | OMRSH_OSCACHE_OPEXIST_DO_NOT_CREATE)) {
	omrnls_printf( J9NLS_ERROR, J9NLS_SHRC_OSCACHE_NOT_EXIST);
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
	errorHandler(J9NLS_SHRC_OSCACHE_SHMEM_OPFAILED_V1, &lastErrorInfo);
	if ((OMRPORT_ERROR_SHMEM_OPFAILED == rc) && (0 != shmid)) {
	  omrnls_printf(J9NLS_ERROR, J9NLS_SHRC_OSCACHE_SHARED_MEMORY_OPFAILED_DISPLAY_SHMID, shmid);
	}
      } else if (OMRPORT_ERROR_SHMEM_OPFAILED_CONTROL_FILE_LOCK_FAILED == rc) {
	errorHandler(J9NLS_SHRC_OSCACHE_SHMEM_OPFAILED_CONTROL_FILE_LOCK_FAILED, &lastErrorInfo);
      } else if (OMRPORT_ERROR_SHMEM_OPFAILED_CONTROL_FILE_CORRUPT == rc) {
	errorHandler(J9NLS_SHRC_OSCACHE_SHMEM_OPFAILED_CONTROL_FILE_CORRUPT, &lastErrorInfo);
      } else if (OMRPORT_ERROR_SHMEM_OPFAILED_SHMID_MISMATCH == rc) {
	errorHandler(J9NLS_SHRC_OSCACHE_SHMEM_OPFAILED_SHMID_MISMATCH, &lastErrorInfo);
	omrnls_printf( J9NLS_ERROR, J9NLS_SHRC_OSCACHE_SHARED_MEMORY_OPFAILED_DISPLAY_SHMID, shmid);
      } else if (OMRPORT_ERROR_SHMEM_OPFAILED_SHM_KEY_MISMATCH == rc) {
	errorHandler(J9NLS_SHRC_OSCACHE_SHMEM_OPFAILED_SHM_KEY_MISMATCH, &lastErrorInfo);
	omrnls_printf( J9NLS_ERROR, J9NLS_SHRC_OSCACHE_SHARED_MEMORY_OPFAILED_DISPLAY_SHMID, shmid);
      } else if (OMRPORT_ERROR_SHMEM_OPFAILED_SHM_GROUPID_CHECK_FAILED == rc) {
	errorHandler(J9NLS_SHRC_OSCACHE_SHMEM_OPFAILED_SHM_GROUPID_CHECK_FAILED, &lastErrorInfo);
	omrnls_printf( J9NLS_ERROR, J9NLS_SHRC_OSCACHE_SHARED_MEMORY_OPFAILED_DISPLAY_SHMID, shmid);
      } else if (OMRPORT_ERROR_SHMEM_OPFAILED_SHM_USERID_CHECK_FAILED == rc) {
	errorHandler(J9NLS_SHRC_OSCACHE_SHMEM_OPFAILED_SHM_USERID_CHECK_FAILED, &lastErrorInfo);
	omrnls_printf( J9NLS_ERROR, J9NLS_SHRC_OSCACHE_SHARED_MEMORY_OPFAILED_DISPLAY_SHMID, shmid);
      } else if (OMRPORT_ERROR_SHMEM_OPFAILED_SHM_SIZE_CHECK_FAILED == rc) {
	errorHandler(J9NLS_SHRC_OSCACHE_SHMEM_OPFAILED_SHM_SIZE_CHECK_FAILED, &lastErrorInfo);
	omrnls_printf( J9NLS_ERROR, J9NLS_SHRC_OSCACHE_SHARED_MEMORY_OPFAILED_DISPLAY_SHMID, shmid);
      }
      /* Report any error that occurred during unlinking of control file */
      if (OMRPORT_INFO_CONTROL_FILE_UNLINK_FAILED == _controlFileStatus.status) {
	omrnls_printf( J9NLS_ERROR, J9NLS_SHRC_OSCACHE_SHARED_MEMORY_CONTROL_FILE_UNLINK_FAILED, _shmFileName);
	OSC_ERR_TRACE1(_configOptions, J9NLS_SHRC_OSCACHE_PORT_ERROR_NUMBER, _controlFileStatus.errorCode);
	OSC_ERR_TRACE1(_configOptions, J9NLS_SHRC_OSCACHE_PORT_ERROR_MESSAGE, _controlFileStatus.errorMsg);
      }
#if !defined(WIN32)
      if (_configOptions->openToStatExistingCache()) { //(_createFlags & OMRSH_OSCACHE_OPEXIST_STATS)) {
	OSC_TRACE(_configOptions, J9NLS_SHRC_OSCACHE_CONTROL_FILE_RECREATE_PROHIBITED_FETCHING_CACHE_STATS);
      } else if (_configOptions->openToDestroyExistingCache()) { //OMR_ARE_ANY_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_DO_NOT_CREATE)) {
	OSC_TRACE(_configOptions, J9NLS_SHRC_OSCACHE_CONTROL_FILE_RECREATE_PROHIBITED);
      } else if (_configOptions->readOnlyOpenMode()) { // 0 != (_openMode & J9OSCACHE_OPEN_MODE_DO_READONLY)) {
	OSC_TRACE(_configOptions, J9NLS_SHRC_OSCACHE_CONTROL_FILE_RECREATE_PROHIBITED_RUNNING_READ_ONLY);
      }
#endif
      Trc_SHR_OSC_openCache_Exit3();
      result = OS_SHARED_MEMORY_CACHE_FAILURE; // OS_SHARED_MEMORY_CACHE_FAILURE;
    }
    break;
  }

  return result;
}

void
OSSharedMemoryCache::setError(IDATA ec)
{
  _errorCode = ec;
}

/**
 * Sets the protection as specified by flags for the memory pages containing all or part of the interval address->(address+len)
 *
 * @param[in] portLibrary An instance of portLibrary
 * @param[in] address 	Pointer to the shared memory region.
 * @param[in] length	The size of memory in bytes spanning the region in which we want to set protection
 * @param[in] flags 	The specified protection to apply to the pages in the specified interval
 *
 * @return 0 if the operations has been successful, -1 if an error has occured
 */
IDATA
OSSharedMemoryCache::setRegionPermissions(OSCacheRegion* region)
{
  OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

  return omrshmem_protect(_cacheLocation, _config->_groupPerm, region->getRegionStartAddress(), region->getRegionSize(),
			  region->renderToFlags());
}

/**
 * Returns the minimum sized region of a shared classes cache on which the process can set permissions, in the number of bytes.
 *
 * @param[in] portLibrary An instance of portLibrary
 *
 * @return the minimum size of region on which we can control permissions size or 0 if unsupported
 *
 */
UDATA
OSSharedMemoryCache::getPermissionsRegionGranularity()
{
  OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
  return omrshmem_get_region_granularity(_cacheLocation, _config->_groupPerm, _config->getDataSectionLocation());
}

UDATA
OSSharedMemoryCache::isCacheActive() const
{
  IDATA rc;
  OMRPortShmemStatistic statbuf;
  OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

  rc = omrshmem_stat(_cacheLocation, _config->_groupPerm, _shmFileName, &statbuf);
  if (-1 == rc) {
    /* CMVC 143141: If shared memory can not be stat'd then it
     * doesn't exist.  In this case we return 0 because a cache
     * that does not exist on the system then it should not be active.
     */
    return 0;
  }

  if(statbuf.nattach > 0) {
    return 1;
  }
  
  return 0;
}

void
OSSharedMemoryCache::errorHandler(U_32 module_name, U_32 message_num, LastErrorInfo *lastErrorInfo)
{
  OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
  
  if (module_name && message_num && _configOptions->verboseEnabled()) {
    omrnls_printf(J9NLS_ERROR, module_name, message_num);
    if ((NULL != lastErrorInfo) && (0 != lastErrorInfo->_lastErrorCode)) {
      printErrorMessage(lastErrorInfo);
    }
  }
  
  setError(OMRSH_OSCACHE_FAILURE);
  if(!_startupCompleted && _openSharedMemory == false) {
    _policies->cleanupSystemResources();
  }
}

void
OSSharedMemoryCache::printErrorMessage(LastErrorInfo *lastErrorInfo)
{
  I_32 errorCode = lastErrorInfo->_lastErrorCode;
  I_32 errorCodeMasked = (errorCode | OMRPORT_ERROR_SYSTEM_CALL_ERRNO_MASK);
  const char * errormsg = lastErrorInfo->_lastErrorMsg;
  I_32 sysFnCode = (errorCode - errorCodeMasked);
  OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
  
  if (errorCode!=0) {
    /*If errorCode is 0 then there is no error*/
    OSC_ERR_TRACE1(_configOptions, J9NLS_SHRC_OSCACHE_PORT_ERROR_NUMBER, errorCode);
    Trc_SHR_Assert_True(errormsg != NULL);
    OSC_ERR_TRACE1(_configOptions, J9NLS_SHRC_OSCACHE_PORT_ERROR_MESSAGE, errormsg);
  }

  /*Handle general errors*/
  switch(errorCodeMasked) {
  case OMRPORT_ERROR_SHMEM_TOOBIG:
  case OMRPORT_ERROR_SYSV_IPC_ERRNO_E2BIG:
#if defined(J9ZOS390)
    OSC_ERR_TRACE(_configOptions, J9NLS_SHRC_OSCACHE_ERROR_SHMEM_TOOBIG_ZOS);
#elif defined(AIXPPC)
    OSC_ERR_TRACE(_configOptions, J9NLS_SHRC_OSCACHE_ERROR_SHMEM_TOOBIG_AIX);
#else
    OSC_ERR_TRACE(_configOptions, J9NLS_SHRC_OSCACHE_ERROR_SHMEM_TOOBIG);
#endif
    break;
  case OMRPORT_ERROR_FILE_NAMETOOLONG:
    OSC_ERR_TRACE(_configOptions, J9NLS_SHRC_OSCACHE_ERROR_FILE_NAMETOOLONG);
    break;
  case OMRPORT_ERROR_SHMEM_DATA_DIRECTORY_FAILED:
  case OMRPORT_ERROR_FILE_NOPERMISSION:
  case OMRPORT_ERROR_SYSV_IPC_ERRNO_EPERM:
  case OMRPORT_ERROR_SYSV_IPC_ERRNO_EACCES:
    OSC_ERR_TRACE(_configOptions, J9NLS_SHRC_OSCACHE_ERROR_SHSEM_NOPERMISSION);
    break;
  case OMRPORT_ERROR_SYSV_IPC_ERRNO_ENOSPC:
    if (OMRPORT_ERROR_SYSV_IPC_SEMGET_ERROR == sysFnCode) {
      OSC_ERR_TRACE(_configOptions, J9NLS_SHRC_OSCACHE_ERROR_SEMAPHORE_LIMIT_REACHED);
    } else if (OMRPORT_ERROR_SYSV_IPC_SHMGET_ERROR == sysFnCode) {
      OSC_ERR_TRACE(_configOptions, J9NLS_SHRC_OSCACHE_ERROR_SHARED_MEMORY_LIMIT_REACHED);
    } else {
      OSC_ERR_TRACE(_configOptions, J9NLS_SHRC_OSCACHE_ERROR_SHSEM_NOSPACE);
    }
    break;
  case OMRPORT_ERROR_SYSV_IPC_ERRNO_ENOMEM:
    OSC_ERR_TRACE(_configOptions, J9NLS_SHRC_OSCACHE_ERROR_SHSEM_NOSPACE);
    break;
  case OMRPORT_ERROR_SYSV_IPC_ERRNO_EMFILE:
    OSC_ERR_TRACE(_configOptions, J9NLS_SHRC_OSCACHE_ERROR_MAX_OPEN_FILES_REACHED);
    break;    
  default:
    break;
  }
}

OSSharedMemoryCachePolicies*
OSSharedMemoryCache::constructSharedMemoryPolicy() {
  return new OSSharedMemoryCachePolicies(this);
}
