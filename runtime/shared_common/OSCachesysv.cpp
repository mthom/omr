/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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

/**
 * @file
 * @ingroup Shared_Common
 */

#include <string.h>
//#include "j2sever.h"
#include "omrcfg.h"
#include "omrport.h"
#include "pool_api.h"
#include "ut_omrshr.h"
#include "shrnls.h"
// #include "util_api.h"

#include "OSCachesysv.hpp"
#include "CompositeCacheImpl.hpp"
#include "CacheMap.hpp"
#include "OSCacheFile.hpp"
#include "UnitTest.hpp"

#include <string.h>

#define OSCACHESYSV_RESTART 4
#define OSCACHESYSV_OPENED 3
#define OSCACHESYSV_CREATED 2
#define OSCACHESYSV_EXIST   1
#define OSCACHESYSV_NOT_EXIST 0
#define OSCACHESYSV_FAILURE -1

#define OSCACHESYSV_SUCCESS 0

#define SEM_HEADERLOCK 0

#define OMRSH_OSCACHE_RETRYMAX 30

#define SHM_CACHEHEADERSIZE SHC_PAD(sizeof(OSCachesysv_header_version_current), SHC_WORDALIGN)
#define SHM_CACHEDATASIZE(size) (size-SHM_CACHEHEADERSIZE)
#define SHM_DATASTARTFROMHEADER(header) SRP_GET(header->oscHdr.dataStart, void*);

/**
 * Create/Open a new OSCache Instance
 *
 * This is a wrapper method for @ref SH_OSCache::startup
 * This c'tor is currently used during unit testing only. Therefore we pass OMRSH_DIRPERM_ABSENT as cacheDirPerm to startup().
 *
 * @param [in]  portLibrary The Port library
 * @param [in]  sharedClassConfig
 * @param [in]  cacheName The name of the cache to be opened/created
 * @param [in]  piconfig Pointer to a configuration structure
 * @param [in]  numLocks The number of locks to be initialized
 * @param [in]  createFlag Indicates whether cache is to be opened or created.
 * \args OMRSH_OSCACHE_CREATE Create the cache if it does not exists, otherwise open existsing cache
 * \args OMRSH_OSCACHE_OPEXIST Open an existing cache only, failed if it doesn't exist.
 * @param [in]  verboseFlags Verbose flags
 * @param [in]  openMode Mode to open the cache in. One of the following flags:
 * \args J9OSCACHE_OPEN_MODE_DO_READONLY - open the cache readonly
 * \args J9OSCACHE_OPEN_MODE_TRY_READONLY_ON_FAIL - if the cache could not be opened read/write - try readonly
 * \args J9OSCACHE_OPEN_MODE_GROUPACCESS - creates a cache with group access
 * @param [in]  versionData Version data of the cache to connect to
 * @param [in]  i Pointer to an initializer to be used to initialize the data
 * 				area of a new cache
 */
SH_OSCachesysv::SH_OSCachesysv(OMRPortLibrary* portLibrary, OMR_VM* vm, const char* cachedirname, const char* cacheName, OMRSharedCachePreinitConfig* piconfig, IDATA numLocks,
		UDATA createFlag, UDATA verboseFlags, U_64 runtimeFlags, I_32 openMode, J9PortShcVersion* versionData, SH_OSCache::SH_OSCacheInitializer* i)
{
	Trc_SHR_OSC_Constructor_Entry(cacheName, piconfig->sharedClassCacheSize, createFlag);
	initialize(portLibrary, NULL, OSCACHE_CURRENT_CACHE_GEN);
	startup(vm, cachedirname, OMRSH_DIRPERM_ABSENT, cacheName, piconfig, numLocks, createFlag, verboseFlags, runtimeFlags, openMode, 0, versionData, i, SHR_STARTUP_REASON_NORMAL);
	Trc_SHR_OSC_Constructor_Exit(cacheName);
}

/**
 * Method to initialize object variables
 *
 * @param [in]  portLib  The Port library
 * @param [in]  memForConstructor  Pointer to the memory to build the OSCachemmap into
 * @param [in]  generation  The generation of this cache
 */
void
SH_OSCachesysv::initialize(OMRPortLibrary* portLib, char* memForConstructor, UDATA generation)
{
	commonInit(portLib, generation);
	_attach_count = 0;
	_shmhandle = NULL;
	_semhandle = NULL;
	_actualCacheSize = 0;
	_shmFileName = NULL;
	_semFileName = NULL;
	_openSharedMemory = false;
	_semid = 0;
	_groupPerm = 0;
	_corruptionCode = NO_CORRUPTION;
	_corruptValue = NO_CORRUPTION;
	_semAccess = OMRSH_SEM_ACCESS_ALLOWED;
	_shmAccess = OMRSH_SHM_ACCESS_ALLOWED;
}

/**
 * setup an OSCache Instance
 * This method will create necessary Operating System resources to support cross-process shared memory
 * If successful a memory area which can be attached by other process will be created
 * User can query the result of the setup operation using getError() method. If the startup has failed,
 * all further operation on the shared cache will be failed.
 *
 * @param [in]  cacheName The name of the cache to be opened/created
 * @param [in]  sharedClassConfig
 * @param [in]  piconfig Pointer to a configuration structure
 * @param [in]  numLocks The number of locks to be initialized
 * @param [in]  createFlag Indicates whether cache is to be opened or created.
 * \args OMRSH_OSCACHE_CREATE Create the cache if it does not exists, otherwise open existsing cache
 * \args OMRSH_OSCACHE_OPEXIST Open an existing cache only, failed if it doesn't exist.
 * @param [in]  verboseFlags Verbose flags
 * @param [in]  openMode Mode to open the cache in. One of the following flags:
 * \args J9OSCACHE_OPEN_MODE_DO_READONLY - open the cache readonly
 * \args J9OSCACHE_OPEN_MODE_TRY_READONLY_ON_FAIL - if the cache could not be opened read/write - try readonly
 * \args J9OSCACHE_OPEN_MODE_GROUPACCESS - creates a cache with group access
 * @param [in]  versionData Version data of the cache to connect to
 * @param [in]  i Pointer to an initializer to be used to initialize the data
 * 				area of a new cache
 * @param [in]  reason Reason for starting up the cache. Not used for non-persistent cache startup
 *
 * @return true on success, false on failure
 */
bool
SH_OSCachesysv::startup(OMR_VM* vm, const char* ctrlDirName, UDATA cacheDirPerm, const char* cacheName, OMRSharedCachePreinitConfig* piconfig, IDATA numLocks, UDATA create,
		UDATA verboseFlags_, U_64 runtimeFlags_, I_32 openMode, UDATA storageKeyTesting, J9PortShcVersion* versionData, SH_OSCache::SH_OSCacheInitializer* i, UDATA reason)
{
	IDATA retryCount;
	IDATA shsemrc = 0;
	IDATA semLength = 0;
	LastErrorInfo lastErrorInfo;
	UDATA defaultCacheSize = J9_SHARED_CLASS_CACHE_DEFAULT_SIZE;

#if defined(J9VM_ENV_DATA64)
#if defined(OPENJ9_BUILD)
	defaultCacheSize = J9_SHARED_CLASS_CACHE_DEFAULT_SIZE_64BIT_PLATFORM;
#else /* OPENJ9_BUILD */
	if (J2SE_VERSION(vm) >= J2SE_19) {
		defaultCacheSize = J9_SHARED_CLASS_CACHE_DEFAULT_SIZE_64BIT_PLATFORM;
	}
#endif /* OPENJ9_BUILD */
#endif /* J9VM_ENV_DATA64 */
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

	Trc_SHR_OSC_startup_Entry(cacheName, (piconfig!= NULL)? piconfig->sharedClassCacheSize : defaultCacheSize, create);

	if (openMode & J9OSCACHE_OPEN_MODE_GROUPACCESS) {
		_groupPerm = 1;
	}

	versionData->cacheType = OMRPORT_SHR_CACHE_TYPE_NONPERSISTENT;
	_cacheSize = (piconfig!= NULL) ? (U_32)piconfig->sharedClassCacheSize : (U_32)defaultCacheSize;
	_initializer = i;
	_totalNumSems = numLocks + 1;		/* +1 because of header mutex */
	_userSemCntr = 0;
	retryCount=OMRSH_OSCACHE_RETRYMAX;

	_storageKeyTesting = storageKeyTesting;

	if (commonStartup(vm, ctrlDirName, cacheDirPerm, cacheName, piconfig, create, verboseFlags_, runtimeFlags_, openMode, versionData) != 0) {
		Trc_SHR_OSC_startup_commonStartupFailure();
		setError(OMRSH_OSCACHE_FAILURE);
		return false;
	}

	Trc_SHR_OSC_startup_commonStartupSuccess();

	_shmFileName = _cacheNameWithVGen;

#if defined(WIN32)
	_semFileName = _cacheNameWithVGen;
#else
	semLength = strlen(_cacheNameWithVGen) + (strlen(OMRSH_SEMAPHORE_ID) - strlen(OMRSH_MEMORY_ID)) + 1;
	/* Unfortunate case is that Java5 and early Java6 caches did not have _G append on the semaphore file,
	 * so to connect with a generation 1 or 2 cache (Java5 was only ever G01), remove the _G01 from the semaphore file name */
	if (_activeGeneration < 3) {
		semLength -= strlen("_GXX");
	}
	if (!(_semFileName = (char*)omrmem_allocate_memory(semLength, OMRMEM_CATEGORY_CLASSES))) {
		Trc_SHR_OSC_startup_nameAllocateFailure();
		OSC_ERR_TRACE(J9NLS_SHRC_OSCACHE_ALLOC_FAILED);
		return false;
	}
	getCacheVersionAndGen(OMRPORTLIB, vm, _semFileName, semLength, cacheName, versionData, _activeGeneration, false);
#endif

	while(retryCount>0) {
		IDATA rc;

		if (_openMode & J9OSCACHE_OPEN_MODE_DO_READONLY) {
			/* Try read-only mode only if shared memory control file exists
			 * because we can't create a new control file when running in read-only mode.
			 */
			if (!statCache(_portLibrary, _cacheDirName, _shmFileName, false)) {
				OSC_ERR_TRACE(J9NLS_SHRC_OSCACHE_STARTUP_CACHE_CREATION_NOT_ALLOWED_READONLY_MODE);
				Trc_SHR_OSC_startup_cacheCreationNotAllowed_readOnlyMode();
				rc = OSCACHESYSV_FAILURE;
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
			UDATA flags = OMRSHSEM_NO_FLAGS;
			if (OMRSH_OSCACHE_OPEXIST_STATS == _createFlags) {
				flags = OMRSHSEM_OPEN_FOR_STATS;
			} else if (OMRSH_OSCACHE_OPEXIST_DESTROY == _createFlags) {
				flags = OMRSHSEM_OPEN_FOR_DESTROY;
			} else if (OMRSH_OSCACHE_OPEXIST_DO_NOT_CREATE == _createFlags) {
				flags = OMRSHSEM_OPEN_DO_NOT_CREATE;
			}
			shsemrc = omrshsem_deprecated_open(_cacheDirName, _groupPerm, &_semhandle, _semFileName, (int)_totalNumSems, 0, flags, NULL);
			lastErrorInfo.lastErrorCode = omrerror_last_error_number();
			lastErrorInfo.lastErrorMsg = omrerror_last_error_message();
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
			if (OMR_ARE_NO_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_DESTROY)) {
				_openMode |= J9OSCACHE_OPEN_MODE_DO_READONLY;
			}
			shsemrc = OMRPORT_INFO_SHSEM_OPENED;
			_semhandle = NULL;
		}

		switch(shsemrc) {

		case OMRPORT_INFO_SHSEM_CREATED:
#if !defined(WIN32)
			if (OMR_ARE_ALL_BITS_SET(_openMode, J9OSCACHE_OPEN_MODE_GROUPACCESS)) {
				/* Verify if the group access has been set */
				struct J9FileStat statBuf;
				char pathFileName[OMRSH_MAXPATH];
				I_32 semid = omrshsem_deprecated_getid(_semhandle);
				I_32 groupAccessRc = verifySemaphoreGroupAccess(&lastErrorInfo);

				if (0 == groupAccessRc) {
					Trc_SHR_OSC_startup_setSemaphoreGroupAccessFailed(semid);
					OSC_WARNING_TRACE1(J9NLS_SHRC_OSCACHE_SEMAPHORE_SET_GROUPACCESS_FAILED, semid);
				} else if (-1 == groupAccessRc) {
					/* Fail to get stats of the semaphore */
					Trc_SHR_OSC_startup_badSemaphoreStat(semid);
					errorHandler(J9NLS_SHRC_OSCACHE_INTERNAL_ERROR_CHECKING_SEMAPHORE_ACCESS, &lastErrorInfo);
					rc = OSCACHESYSV_FAILURE;
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
					lastErrorInfo.lastErrorCode = omrerror_last_error_number();
					lastErrorInfo.lastErrorMsg = omrerror_last_error_message();
					errorHandler(J9NLS_SHRC_OSCACHE_ERROR_FILE_STAT, &lastErrorInfo);
					rc = OSCACHESYSV_FAILURE;
					break;
				}
			}
#endif /* !defined(WIN32) */
			/*no break here, simply fall thru to the next case and open/create the cache*/
		case OMRPORT_INFO_SHSEM_OPENED:
			/* Avoid any checks for semaphore access if
			 * - running in readonly mode as we don't use semaphore, or
			 * - user has specified a cache directory, or
			 * - destroying an existing cache
			 */
			if (OMR_ARE_NO_BITS_SET(_openMode, J9OSCACHE_OPEN_MODE_DO_READONLY)
				&& (OMRPORT_INFO_SHSEM_OPENED == shsemrc)
				&& (!_isUserSpecifiedCacheDir)
				&& (OMR_ARE_NO_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_DESTROY))
			) {
				_semAccess = checkSemaphoreAccess(&lastErrorInfo);
			}

			/* Ignore _semAccess when opening cache for printing stats, but we do need it later to display cache usability */
			if (OMR_ARE_ALL_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_STATS)
				|| (OMRSH_SEM_ACCESS_ALLOWED == _semAccess)
			) {
				IDATA headerMutexRc = 0;
				if (OMR_ARE_NO_BITS_SET(_runtimeFlags, OMRSHR_RUNTIMEFLAG_RESTORE_CHECK)) {
					/* When running "restoreFromSnapshot" utility, headerMutex has already been acquired in the first call of SH_OSCachesysv::startup()*/
					headerMutexRc = enterHeaderMutex(&lastErrorInfo);
				}
				if (0 == headerMutexRc) {
					rc = openCache(_cacheDirName, versionData, (shsemrc == OMRPORT_INFO_SHSEM_CREATED));
					if (OMR_ARE_NO_BITS_SET(_runtimeFlags, OMRSHR_RUNTIMEFLAG_RESTORE | OMRSHR_RUNTIMEFLAG_RESTORE_CHECK)) {
						/* When running "restoreFromSnapshot" utility, do not release headerMutex here */
						if (0 != exitHeaderMutex(&lastErrorInfo)) {
							errorHandler(J9NLS_SHRC_OSCACHE_ERROR_EXIT_HDR_MUTEX, &lastErrorInfo);
							rc = OSCACHESYSV_FAILURE;
						}
					}
				} else {
					errorHandler(J9NLS_SHRC_OSCACHE_ERROR_ENTER_HDR_MUTEX, &lastErrorInfo);
					rc = OSCACHESYSV_FAILURE;
				}
			} else {
				switch (_semAccess) {
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
				rc = OSCACHESYSV_FAILURE;
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
			if (OMR_ARE_ALL_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_DESTROY)
				&& (OMRPORT_ERROR_SHSEM_OPFAILED_SEMAPHORE_NOT_FOUND == shsemrc)
			) {
				/* No semaphore set was found when opening for "destroy". Avoid printing any error message. */
				rc = OSCACHESYSV_SUCCESS;
			} else {
				I_32 semid = 0;

				/* For some error codes, portlibrary stores semaphore set id obtained from the control file
				 * enabling us to display it in the error message. Retrieve the id and free memory allocated to handle.
				 */
				if (NULL != _semhandle) {
					semid = omrshsem_deprecated_getid(_semhandle);
					omrmem_free_memory(_semhandle);
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
					OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_PORT_ERROR_NUMBER, _controlFileStatus.errorCode);
					OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_PORT_ERROR_MESSAGE, _controlFileStatus.errorMsg);
				}
			}
			/* While opening shared cache for "destroy" if some error occurs when opening the semaphore set,
			 * don't bail out just yet but try to open the shared memory.
			 */
			if (OMR_ARE_ALL_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_DESTROY)) {
				_semhandle = NULL;
				/* Skip acquiring header mutex as the semaphore handle is NULL */
				rc = openCache(_cacheDirName, versionData, false);
			} else if (OMR_ARE_ALL_BITS_SET(_openMode, J9OSCACHE_OPEN_MODE_TRY_READONLY_ON_FAIL)) {
				/* Try read-only mode for 'nonfatal' option only if shared memory control file exists
				 * because we can't create a new control file when running in read-only mode.
				 */
				if (statCache(_portLibrary, _cacheDirName, _shmFileName, false)) {
					OSC_TRACE(J9NLS_SHRC_OSCACHE_STARTUP_NONFATAL_TRY_READONLY);
					Trc_SHR_OSC_startup_attemptNonfatalReadOnly();
					_openMode |= J9OSCACHE_OPEN_MODE_DO_READONLY;
					rc = OSCACHESYSV_RESTART;
				} else {
					rc=OSCACHESYSV_FAILURE;
				}
			} else {
				rc=OSCACHESYSV_FAILURE;
			}
			break;

		case OMRPORT_ERROR_SHSEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT:
			errorHandler(J9NLS_SHRC_OSCACHE_SEMAPHORE_WAIT_FOR_CREATION_MUTEX_TIMEDOUT, &lastErrorInfo);
			rc=OSCACHESYSV_FAILURE;
			break;

		default:
			errorHandler(J9NLS_SHRC_OSCACHE_UNKNOWN_ERROR, &lastErrorInfo);
			rc=OSCACHESYSV_FAILURE;
			break;
		}

		switch (rc) {
		case OSCACHESYSV_CREATED:
			if (_verboseFlags & OMRSHR_VERBOSEFLAG_ENABLE_VERBOSE) {
				OSC_TRACE1(J9NLS_SHRC_OSCACHE_SHARED_CACHE_CREATEDA, _cacheName);
			}
#if !defined(WIN32)
			if (OMR_ARE_ALL_BITS_SET(_openMode, J9OSCACHE_OPEN_MODE_GROUPACCESS)) {
				/* Verify if the group access has been set */
				struct J9FileStat statBuf;
				I_32 shmid = omrshmem_getid(_shmhandle);
				I_32 groupAccessRc = verifySharedMemoryGroupAccess(&lastErrorInfo);

				if (0 == groupAccessRc) {
					Trc_SHR_OSC_startup_setSharedMemoryGroupAccessFailed(shmid);
					OSC_WARNING_TRACE1(J9NLS_SHRC_OSCACHE_SHARED_MEMORY_SET_GROUPACCESS_FAILED, shmid);
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
						OSC_WARNING_TRACE1(J9NLS_SHRC_OSCACHE_SHM_CONTROL_FILE_SET_GROUPACCESS_FAILED, _cachePathName);
					}
				} else {
					Trc_SHR_OSC_startup_badFileStat(_cachePathName);
					lastErrorInfo.lastErrorCode = omrerror_last_error_number();
					lastErrorInfo.lastErrorMsg = omrerror_last_error_message();
					errorHandler(J9NLS_SHRC_OSCACHE_ERROR_FILE_STAT, &lastErrorInfo);
					retryCount = 0;
					continue;
				}
			}
#endif /* !defined(WIN32) */
			setError(OMRSH_OSCACHE_CREATED);
			getTotalSize();
			Trc_SHR_OSC_startup_Exit_Created(cacheName);
			_startupCompleted=true;
			return true;

		case OSCACHESYSV_OPENED:
			if (_verboseFlags & OMRSHR_VERBOSEFLAG_ENABLE_VERBOSE) {
				if (_runningReadOnly) {
					OSC_TRACE1(J9NLS_SHRC_OSCACHE_SYSV_STARTUP_OPENED_READONLY, _cacheName);
				} else {
					OSC_TRACE1(J9NLS_SHRC_OSCACHE_SHARED_CACHE_OPENEDA, _cacheName);
				}
			}
			setError(OMRSH_OSCACHE_OPENED);
			getTotalSize();
			Trc_SHR_OSC_startup_Exit_Opened(cacheName);
			_startupCompleted=true;
			return true;

		case OSCACHESYSV_RESTART:
			Trc_SHR_OSC_startup_attempt_Restart(cacheName);
			break;

		case OSCACHESYSV_FAILURE:
			retryCount = 0;
			continue;

		case OSCACHESYSV_NOT_EXIST:
			/* Currently, this case occurs only when OMRSH_OSCACHE_OPEXIST_STATS, OMRSH_OSCACHE_OPEXIST_DESTROY or OMRSH_OSCACHE_OPEXIST_DO_NOT_CREATE is set and shared memory does not exist. */
			setError(OMRSH_OSCACHE_NO_CACHE);
			Trc_SHR_OSC_startup_Exit_NoCache(cacheName);
			return false;

		default:
			break;
		}

		retryCount--;

	} /* END while(retryCount>0) */

	setError(OMRSH_OSCACHE_FAILURE);
	Trc_SHR_OSC_startup_Exit_Failed(cacheName);
	return false;
}

/* The caller should hold the mutex */
IDATA
SH_OSCachesysv::openCache(const char* cacheDirName, J9PortShcVersion* versionData, bool semCreated)
{
	/* we are attaching to existing cache! */
	Trc_SHR_OSC_openCache_Entry(_cacheName);
	IDATA rc;
	IDATA result = OSCACHESYSV_FAILURE;
	LastErrorInfo lastErrorInfo;
	
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

	rc = shmemOpenWrapper(_shmFileName, &lastErrorInfo);
	Trc_SHR_OSC_openCache_shmem_open(_shmFileName, _cacheSize);

	switch (rc) {
	case OMRPORT_ERROR_SHMEM_OPEN_ATTACHED_FAILED:
		_openSharedMemory = true;
		/* FALLTHROUGH */
	case OMRPORT_ERROR_SHMEM_CREATE_ATTACHED_FAILED:
		/*We failed to attach to the memory.*/
		errorHandler(J9NLS_SHRC_OSCACHE_SHMEM_OPEN_ATTACHED_FAILED, &lastErrorInfo);
		Trc_SHR_OSC_openCache_ExitAttachFailed();
		result = OSCACHESYSV_FAILURE;
		break;
	case OMRPORT_INFO_SHMEM_OPENED:
		/* Avoid any checks if
		 * - user has specified a cache directory, or
		 * - destroying an existing cache
		 */
		if (!_isUserSpecifiedCacheDir && (OMR_ARE_NO_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_DESTROY))) {
			_shmAccess = checkSharedMemoryAccess(&lastErrorInfo);
		}
		/* Ignore _shmAccess when opening cache for printing stats, but we do need it later to display cache usability */
		if (OMR_ARE_ALL_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_STATS)
			|| (OMRSH_SHM_ACCESS_ALLOWED == _shmAccess)
		) {
			/* ALL SET */
			Trc_SHR_OSC_openCache_Exit_Opened(_cacheName);
			result = OSCACHESYSV_OPENED;
		} else {
			switch (_shmAccess) {
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
			Trc_SHR_OSC_openCache_ExitAccessNotAllowed(_shmAccess);
			result = OSCACHESYSV_FAILURE;
		}

		break;
	case OMRPORT_INFO_SHMEM_CREATED:
		/* We opened semaphore yet created the cache area -
		 * we should set it up, but don't need to init semaphore
		 */
		rc = initializeHeader(cacheDirName, versionData, lastErrorInfo);
		if(rc == OSCACHESYSV_FAILURE) {
			Trc_SHR_OSC_openCache_Exit_CreatedHeaderInitFailed(_cacheName);
			result = OSCACHESYSV_FAILURE;
			break;
		}
		Trc_SHR_OSC_openCache_Exit_Created(_cacheName);
		result = OSCACHESYSV_CREATED;
		break;

	case OMRPORT_ERROR_SHMEM_WAIT_FOR_CREATION_MUTEX_TIMEDOUT:
		errorHandler(J9NLS_SHRC_OSCACHE_SHMEM_OPEN_WAIT_FOR_CREATION_MUTEX_TIMEDOUT, &lastErrorInfo);
		Trc_SHR_OSC_openCache_Exit4();
		result = OSCACHESYSV_FAILURE;
		break;

	case OMRPORT_INFO_SHMEM_PARTIAL:
		/* If OMRPORT_INFO_SHMEM_PARTIAL then ::startup() was called by j9shr_destroy_cache().
		 * Returning OSCACHESYSV_OPENED will cause j9shr_destroy_cache() to call ::destroy(),
		 * which will cleanup any control files that have there SysV IPC objects del
		 */
		result = OSCACHESYSV_OPENED;
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
		if (OMR_ARE_ANY_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_STATS | OMRSH_OSCACHE_OPEXIST_DESTROY | OMRSH_OSCACHE_OPEXIST_DO_NOT_CREATE)
			&& (OMRPORT_ERROR_SHMEM_OPFAILED_SHARED_MEMORY_NOT_FOUND == rc)
		) {
			if (OMR_ARE_ALL_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_DESTROY)) {
				/* Absence of shared memory is equivalent to non-existing cache. Do not display any error message,
				 * but do call cleanupSysVResources() to remove any semaphore set in case we opened it successfully.
				 */
				cleanupSysvResources();
			} else if (OMR_ARE_ANY_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_STATS | OMRSH_OSCACHE_OPEXIST_DO_NOT_CREATE)) {
				omrnls_printf( J9NLS_ERROR, J9NLS_SHRC_OSCACHE_NOT_EXIST);
			}
			Trc_SHR_OSC_openCache_Exit3();
			result = OSCACHESYSV_NOT_EXIST;
		} else {
			I_32 shmid = 0;

			/* For some error codes, portlibrary stores shared memory id obtained from the control file
			 * enabling us to display it in the error message. Retrieve the id and free memory allocated to handle.
			 */
			if (NULL != _shmhandle) {
				shmid = omrshmem_getid(_shmhandle);
				omrmem_free_memory(_shmhandle);
			}

			if ((OMRPORT_ERROR_SHMEM_OPFAILED == rc) || (OMRPORT_ERROR_SHMEM_OPFAILED_SHARED_MEMORY_NOT_FOUND == rc)) {
				errorHandler(J9NLS_SHRC_OSCACHE_SHMEM_OPFAILED_V1, &lastErrorInfo);
				if ((OMRPORT_ERROR_SHMEM_OPFAILED == rc) && (0 != shmid)) {
					omrnls_printf( J9NLS_ERROR, J9NLS_SHRC_OSCACHE_SHARED_MEMORY_OPFAILED_DISPLAY_SHMID, shmid);
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
				OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_PORT_ERROR_NUMBER, _controlFileStatus.errorCode);
				OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_PORT_ERROR_MESSAGE, _controlFileStatus.errorMsg);
			}
#if !defined(WIN32)
			if (0 != (_createFlags & OMRSH_OSCACHE_OPEXIST_STATS)) {
				OSC_TRACE(J9NLS_SHRC_OSCACHE_CONTROL_FILE_RECREATE_PROHIBITED_FETCHING_CACHE_STATS);
			} else if (OMR_ARE_ANY_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_DO_NOT_CREATE)) {
				OSC_TRACE(J9NLS_SHRC_OSCACHE_CONTROL_FILE_RECREATE_PROHIBITED);
			} else if (0 != (_openMode & J9OSCACHE_OPEN_MODE_DO_READONLY)) {
				OSC_TRACE(J9NLS_SHRC_OSCACHE_CONTROL_FILE_RECREATE_PROHIBITED_RUNNING_READ_ONLY);
			}
#endif
			Trc_SHR_OSC_openCache_Exit3();
			result = OSCACHESYSV_FAILURE;
		}
		break;
	}

	return result;
}

IDATA
SH_OSCachesysv::verifyCacheHeader(J9PortShcVersion* versionData)
{
	IDATA headerRc = OMRSH_OSCACHE_HEADER_OK;
	OSCachesysv_header_version_current* header = (OSCachesysv_header_version_current*)_headerStart;
	IDATA retryCntr = 0;
	LastErrorInfo lastErrorInfo;
	IDATA rc = 0;

	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

	if (header == NULL) {
		return OMRSH_OSCACHE_HEADER_MISSING;
	}

	/* In readonly, we can't get a header lock, so if the cache is mid-init, give it a chance to complete initialization */
	if (_runningReadOnly) {
		while ((!header->oscHdr.cacheInitComplete) && (retryCntr < OMRSH_OSCACHE_READONLY_RETRY_COUNT)) {
			omrthread_sleep(OMRSH_OSCACHE_READONLY_RETRY_SLEEP_MILLIS);
			++retryCntr;
		}
		if (!header->oscHdr.cacheInitComplete) {
			return OMRSH_OSCACHE_HEADER_MISSING;
		}
	}

	if (OMR_ARE_NO_BITS_SET(_runtimeFlags, OMRSHR_RUNTIMEFLAG_RESTORE | OMRSHR_RUNTIMEFLAG_RESTORE_CHECK)) {
		/* When running "restoreFromSnapshot" utility, headerMutex is already acquired */
		rc = enterHeaderMutex(&lastErrorInfo);
	}
	if (0 == rc) {
		/* First, check the eyecatcher */
		if(strcmp(header->eyecatcher, OMRPORT_SHMEM_EYECATCHER)) {
			OSC_ERR_TRACE(J9NLS_SHRC_OSCACHE_ERROR_WRONG_EYECATCHER);
			Trc_SHR_OSC_recreateSemaphore_Exit1();
			OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_CORRUPT_CACHE_HEADER_BAD_EYECATCHER, header->eyecatcher);
			setCorruptionContext(CACHE_HEADER_BAD_EYECATCHER, (UDATA)(header->eyecatcher));
			headerRc = OMRSH_OSCACHE_HEADER_CORRUPT;
		}
		if (OMRSH_OSCACHE_HEADER_OK == headerRc) {
			headerRc = checkOSCacheHeader(&(header->oscHdr), versionData, SHM_CACHEHEADERSIZE);
		}
		if (OMRSH_OSCACHE_HEADER_OK == headerRc) {
			if (NULL != _semhandle) {
				/*Note: _semhandle will likely be NULL for two reasons:
				 * - the cache was opened readonly, due to the use of the 'readonly' option
				 * - the cache was opened readonly, due to the use of 'nonfatal' option
				 * In both cases we ignore the below check.
				 */
				_semid = omrshsem_deprecated_getid(_semhandle);
				if((_runtimeFlags & OMRSHR_RUNTIMEFLAG_ENABLE_SEMAPHORE_CHECK) && (header->attachedSemid != 0) && (header->attachedSemid != _semid)) {
					Trc_SHR_OSC_recreateSemaphore_Exit4(header->attachedSemid, _semid);
					OSC_ERR_TRACE2(J9NLS_SHRC_OSCACHE_CORRUPT_CACHE_SEMAPHORE_MISMATCH, header->attachedSemid, _semid);
					setCorruptionContext(CACHE_SEMAPHORE_MISMATCH, (UDATA)_semid);
					headerRc = OMRSH_OSCACHE_SEMAPHORE_MISMATCH;
				}
			}
		}

		if (OMR_ARE_NO_BITS_SET(_runtimeFlags, OMRSHR_RUNTIMEFLAG_RESTORE | OMRSHR_RUNTIMEFLAG_RESTORE_CHECK)) {
			/* When running "restoreFromSnapshot" utility, do not release headerMutex here */
			rc = exitHeaderMutex(&lastErrorInfo);
		}
		if (0 != rc) {
			errorHandler(J9NLS_SHRC_OSCACHE_ERROR_EXIT_HDR_MUTEX, &lastErrorInfo);
			if (OMRSH_OSCACHE_HEADER_OK == headerRc) {
				headerRc = OMRSH_OSCACHE_HEADER_MISSING;
			}
		}
	} else {
		errorHandler(J9NLS_SHRC_OSCACHE_ERROR_ENTER_HDR_MUTEX, &lastErrorInfo);
		headerRc = OMRSH_OSCACHE_HEADER_MISSING;
	}

	return headerRc;
}

IDATA
SH_OSCachesysv::detachRegion(void)
{
	IDATA rc = OSCACHESYSV_FAILURE;
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

	Trc_SHR_OSC_detachRegion_Entry();

	if (_shmhandle != NULL) {
		Trc_SHR_OSC_detachRegion_Debug(_dataStart, _headerStart);

		rc = omrshmem_detach(&_shmhandle);
		if (rc == -1) {
			LastErrorInfo lastErrorInfo;
			lastErrorInfo.lastErrorCode = omrerror_last_error_number();
			lastErrorInfo.lastErrorMsg = omrerror_last_error_message();
			errorHandler(J9NLS_SHRC_OSCACHE_SHMEM_DETACH, &lastErrorInfo);
		} else {
			rc = OSCACHESYSV_SUCCESS;
		}
		_dataStart = 0;
		_headerStart = 0;
	}

	Trc_SHR_OSC_detachRegion_Exit();
	return rc;
}

/**
 * This is the deconstructor routine.
 *
 * It will remove any memory allocated by this class.
 * However it will not remove any shared memory / mutex resources from the underlying OS.
 * User who wishes to remove a shared memory region should use @ref SH_OSCache::destroy method
 */
void
SH_OSCachesysv::cleanup(void)
{
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

	Trc_SHR_OSC_cleanup_Entry();
	detachRegion();
	/* now free the handles */
	if(_shmhandle != NULL) {
		omrshmem_close(&_shmhandle);
	}
	if(_semhandle != NULL) {
		omrshsem_deprecated_close(&_semhandle);
	}
	commonCleanup();
#if !defined(WIN32)
	if (_semFileName) {
		omrmem_free_memory(_semFileName);
	}
#endif
	Trc_SHR_OSC_cleanup_Exit();

}

IDATA
SH_OSCachesysv::detach(void)
{
	IDATA rc=OSCACHESYSV_FAILURE;
	Trc_SHR_OSC_detach_Entry();

	if(_shmhandle == NULL) {
		Trc_SHR_OSC_detach_Exit1();
		return OSCACHESYSV_SUCCESS;
	}

	Trc_SHR_OSC_detach_Debug(_cacheName, _dataStart);

	_attach_count--;

	if(_attach_count == 0) {
		detachRegion();
		rc=OSCACHESYSV_SUCCESS;
	}

	Trc_SHR_OSC_detach_Exit();
	return rc;
}


/* The caller is responsible for locking the shared memory area */
IDATA
SH_OSCachesysv::initializeHeader(const char* cacheDirName, J9PortShcVersion* versionData, LastErrorInfo lastErrorInfo)
{
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
	void* region;
	OSCachesysv_header_version_current* myHeader;
	U_32 dataLength;
	U_32 readWriteBytes = (U_32)((_config->sharedClassReadWriteBytes > 0) ? _config->sharedClassReadWriteBytes : 0);
	U_32 totalSize = getTotalSize();
	U_32 softMaxSize = (U_32)_config->sharedClassSoftMaxBytes;

	if (_config->sharedClassSoftMaxBytes < 0) {
		softMaxSize = (U_32)-1;
	} else if (softMaxSize > totalSize) {
		Trc_SHR_OSC_Sysv_initializeHeader_softMaxBytesTooBig(totalSize);
		softMaxSize = totalSize;
	}

	if(_cacheSize <= SHM_CACHEHEADERSIZE) {
		errorHandler(J9NLS_SHRC_OSCACHE_TOOSMALL, &lastErrorInfo);
		return OSCACHESYSV_FAILURE;
	} else {
		dataLength = SHM_CACHEDATASIZE(_cacheSize);
	}

	region = omrshmem_attach(_shmhandle, OMRMEM_CATEGORY_CLASSES_SHC_CACHE);

	if(region == NULL) {
		lastErrorInfo.lastErrorCode = omrerror_last_error_number();
		lastErrorInfo.lastErrorMsg = omrerror_last_error_message();
		errorHandler(J9NLS_SHRC_OSCACHE_SHMEM_ATTACH, &lastErrorInfo);
		Trc_SHR_OSC_add_Exit1();
		return OSCACHESYSV_FAILURE;
	}

	/* We can now setup the header */
	_headerStart = region;
	_dataStart = (void*)((UDATA)region + SHM_CACHEHEADERSIZE);
	_dataLength = dataLength;
	myHeader = (OSCachesysv_header_version_current*) region;

	memset(myHeader, 0, SHM_CACHEHEADERSIZE);
	strncpy(myHeader->eyecatcher, OMRPORT_SHMEM_EYECATCHER, OMRPORT_SHMEM_EYECATCHER_LENGTH);

	initOSCacheHeader(&(myHeader->oscHdr), versionData, SHM_CACHEHEADERSIZE);

	myHeader->attachedSemid = omrshsem_deprecated_getid(_semhandle);

	/* If this is not being written into the default control file dir, mark it as such so that it cannot be auto-deleted */
	myHeader->inDefaultControlDir = (cacheDirName == NULL);

	/* jump over to the data area*/
	region = SHM_DATASTARTFROMHEADER(myHeader);

	if(_initializer != NULL) {
		_initializer->init((char*)region, dataLength, (I_32)_config->sharedClassMinAOTSize, (I_32)_config->sharedClassMaxAOTSize, (I_32)_config->sharedClassMinJITSize, (I_32)_config->sharedClassMaxJITSize, readWriteBytes, softMaxSize);
	}

	if (OMR_ARE_NO_BITS_SET(_runtimeFlags, OMRSHR_RUNTIMEFLAG_RESTORE)) {
	/* Do not set oscHdr.cacheInitComplete to 1 if the cache is to be restored */
		myHeader->oscHdr.cacheInitComplete = 1;
	}

	/*ALL DONE */

	return OSCACHESYSV_SUCCESS;
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
SH_OSCachesysv::attach(OMR_VMThread *currentThread, J9PortShcVersion* expectedVersionData)
{
	OMR_VM *vm = currentThread->_vm;
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
	IDATA headerRc;

	Trc_SHR_OSC_attach_Entry();
	if (_shmhandle == NULL) {
		/* _shmhandle == NULL means previous op failed */
		Trc_SHR_OSC_attach_Exit1();
		return NULL;
	}

	if ((((_runtimeFlags & OMRSHR_RUNTIMEFLAG_CREATE_OLD_GEN) == 0) && (_activeGeneration != getCurrentCacheGen())) ||
		(((_runtimeFlags & OMRSHR_RUNTIMEFLAG_CREATE_OLD_GEN) != 0) && (_activeGeneration != getCurrentCacheGen()-1))
	){
		Trc_SHR_OSC_attach_ExitWrongGen();
		return NULL;
	}

	/* Cache is opened attached, the call here will simply return the
	 * address memory already attached.
	 *
	 * Note: Unless ::detach was called ... which I believe does not currently occur.
	 */
	Trc_SHR_OSC_attach_Try_Attach1(UnitTest::unitTest);
	void* request = omrshmem_attach(_shmhandle, OMRMEM_CATEGORY_CLASSES_SHC_CACHE);

	if (request == NULL) {
		LastErrorInfo lastErrorInfo;
		lastErrorInfo.lastErrorCode = omrerror_last_error_number();
		lastErrorInfo.lastErrorMsg = omrerror_last_error_message();
		errorHandler(J9NLS_SHRC_OSCACHE_SHMEM_ATTACH, &lastErrorInfo);
		_dataStart = NULL;
		_attach_count = 0;
		Trc_SHR_OSC_attach_Exit2();
		return NULL;
	}

	Trc_SHR_OSC_attach_Debug1(request);
	Trc_SHR_OSC_attach_Debug2(sizeof(OSCachesysv_header_version_current));

	_headerStart = request;

	if ((headerRc = verifyCacheHeader(expectedVersionData)) != OMRSH_OSCACHE_HEADER_OK) {
		if ((headerRc == OMRSH_OSCACHE_HEADER_CORRUPT) || (headerRc == OMRSH_OSCACHE_SEMAPHORE_MISMATCH)) {
			/* Cache is corrupt, trigger hook to generate a system dump.
			 * This is the last chance to get corrupt cache image in system dump.
			 * After this point, cache is detached.
			 */
			if (0 == (_runtimeFlags & OMRSHR_RUNTIMEFLAG_DISABLE_CORRUPT_CACHE_DUMPS)) {
//				TRIGGER_J9HOOK_VM_CORRUPT_CACHE(vm->hookInterface, currentThread);
			}
			setError(OMRSH_OSCACHE_CORRUPT);
		} else if (headerRc == OMRSH_OSCACHE_HEADER_DIFF_BUILDID) {
			setError(OMRSH_OSCACHE_DIFF_BUILDID);
		}
		omrshmem_detach(&_shmhandle);
		Trc_SHR_OSC_attach_ExitHeaderIsNotOk(headerRc);
		return NULL;
	}

	/*_dataStart is set here, and possibly initializeHeader if its a new cache */
	_dataStart = SHM_DATASTARTFROMHEADER(((OSCachesysv_header_version_current*)_headerStart));

	_dataLength = SHM_CACHEDATASIZE(((OSCachesysv_header_version_current*)_headerStart)->oscHdr.size);
	_attach_count++;

	if (_verboseFlags & OMRSHR_VERBOSEFLAG_ENABLE_VERBOSE) {
		OSC_TRACE2(J9NLS_SHRC_OSCACHE_ATTACH_SUCCESS, _cacheName, _dataLength);
	}

	Trc_SHR_OSC_attach_Exit(_dataStart);
	return _dataStart;
}

/**
 * Attempt to destroy the cache that is currently attached
 *
 * @param[in] suppressVerbose suppresses verbose output
 * @param[in] True if reset option is being used, false otherwise.
 *
 * This method will detach shared memory from the process address space, and attempt
 * to remove any operating system shared memory and mutex region.
 * When this call is complete you should consider the shared memory region will not be
 * available for use, and must be recreated.
 *
 * @return 0 for success and -1 for failure
 */
IDATA
SH_OSCachesysv::destroy(bool suppressVerbose, bool isReset)
{
	IDATA rc;
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
	UDATA origVerboseFlags = _verboseFlags;
	IDATA returnVal = -1;		/* Assume failure */

	Trc_SHR_OSC_destroy_Entry();

	if (suppressVerbose) {
		_verboseFlags = 0;
	}

	/* We will try our best and destroy the OSCache here */
	detachRegion();

#if !defined(WIN32)
	/*If someone is still detached, don't do it, warn and exit*/
	/* isCacheActive isn't really accurate for Win32, so can't check */
	if(isCacheActive()) {
		IDATA corruptionCode;
		OSC_TRACE1(J9NLS_SHRC_OSCACHE_SHARED_CACHE_STILL_ATTACH, _cacheName);
		/* Even if cache is active, we destroy the semaphore if the cache has been marked as corrupt due to CACHE_SEMAPHORE_MISMATCH. */
		getCorruptionContext(&corruptionCode, NULL);
		if (CACHE_SEMAPHORE_MISMATCH == corruptionCode) {
			if (_semhandle != NULL) {
#if !defined(WIN32)
				rc = DestroySysVSemHelper();
#endif
			}
		}
		goto _done;
	}
#endif

	/* Now try to remove the shared memory region */
	if (_shmhandle != NULL) {
#if !defined(WIN32)
		rc = DestroySysVMemoryHelper();
#else
		rc = omrshmem_destroy(_cacheDirName, _groupPerm, &_shmhandle);
#endif
		if(rc != 0) {
			OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_SHARED_CACHE_MEMORY_REMOVE_FAILED, _cacheName);
			goto _done;
		}
	}

	if (_semhandle != NULL) {
#if !defined(WIN32)
		rc = DestroySysVSemHelper();
#else
		rc = omrshsem_deprecated_destroy(&_semhandle);
#endif
		if(rc!=0) {
			OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_SHARED_CACHE_SEMAPHORE_REMOVE_FAILED, _cacheName);
			goto _done;
		}
	}

	returnVal = 0;
	if (_verboseFlags) {
		if (isReset) {
			OSC_TRACE1(J9NLS_SHRC_OSCACHE_SHARED_CACHE_DESTROYED, _cacheName);
		} else {
			J9PortShcVersion versionData;

			memset(&versionData, 0, sizeof(J9PortShcVersion));
			/* Do not care about the return value of getValuesFromShcFilePrefix() */
			getValuesFromShcFilePrefix(OMRPORTLIB, _cacheNameWithVGen, &versionData);
			if (OMRSH_FEATURE_COMPRESSED_POINTERS == versionData.feature) {
				OSC_TRACE1(J9NLS_SHRC_OSCACHE_SHARED_CACHE_DESTROYED_CR, _cacheName);
			} else if (OMRSH_FEATURE_NON_COMPRESSED_POINTERS == versionData.feature) {
				OSC_TRACE1(J9NLS_SHRC_OSCACHE_SHARED_CACHE_DESTROYED_NONCR, _cacheName);
			} else {
				OSC_TRACE1(J9NLS_SHRC_OSCACHE_SHARED_CACHE_DESTROYED, _cacheName);
			}
		}
	}

_done :
	if (suppressVerbose) {
		_verboseFlags = origVerboseFlags;
	}

	Trc_SHR_OSC_destroy_Exit2(returnVal);

	return returnVal;
}

/**
 * Get an ID for a new write lock
 *
 * @return a non-negative lockID on success, -1 on failure
 */
IDATA
SH_OSCachesysv::getNewWriteLockID(void)
{
	if (_userSemCntr < (_totalNumSems-1)) {
		return ++_userSemCntr;
	} else {
		return -1;
	}
}

/**
 * Get an ID for a write area lock
 *
 * @return a non-negative lockID on success, -1 on failure
 */
IDATA
SH_OSCachesysv::getWriteLockID() {
	return this->getNewWriteLockID();
}

/**
 * Get an ID for a readwrite area lock
 *
 * @return a non-negative lockID on success, -1 on failure
 */
IDATA
SH_OSCachesysv::getReadWriteLockID() {
	return this->getNewWriteLockID();
}

/**
 * Obtain the exclusive access right for the shared cache
 *
 * If this method succeeds, the caller will own the exclusive access right to the lock specified
 * and any other thread that attempts to call this method will be suspended.
 * If the process which owns the exclusive access right has crashed without relinquishing the access right,
 * it will automatically resume one of the waiting threads which will then own the access right.
 *
 * @param[in] lockID  The ID of the lock to acquire
 *
 * @return 0 if the operation has been successful, -1 if an error has occured
 */
IDATA
SH_OSCachesysv::acquireWriteLock(UDATA lockID)
{
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
	IDATA rc;
	Trc_SHR_OSC_enterMutex_Entry(_cacheName);
	if(_semhandle == NULL) {
		Trc_SHR_OSC_enterMutex_Exit1();
		Trc_SHR_Assert_ShouldNeverHappen();
		return -1;
	}

	if (lockID > (_totalNumSems-1)) {
		Trc_SHR_OSC_enterMutex_Exit2_V2(lockID, _totalNumSems-1);
		Trc_SHR_Assert_ShouldNeverHappen();
		return -1;
	}

	rc = omrshsem_deprecated_wait(_semhandle, lockID, OMRPORT_SHSEM_MODE_UNDO);
	if (rc == -1) {
		/* CMVC 97181 : Don't print error message because if JVM terminates with ^C signal, this function will return -1 and this is not an error*/
		I_32 myerror = omrerror_last_error_number();
		if ( ((I_32)(myerror | 0xFFFF0000)) != OMRPORT_ERROR_SYSV_IPC_ERRNO_EINTR) {
#if !defined(WIN32)
			OSC_ERR_TRACE2(J9NLS_SHRC_CC_SYSV_AQUIRE_LOCK_FAILED_ENTER_MUTEX, omrshsem_deprecated_getid(_semhandle), myerror);
#else
			OSC_ERR_TRACE1(J9NLS_SHRC_CC_AQUIRE_LOCK_FAILED_ENTER_MUTEX, myerror);
#endif
			Trc_SHR_OSC_enterMutex_Exit3(myerror);
			Trc_SHR_Assert_ShouldNeverHappen();
			return -1;
		}
	}
	Trc_SHR_OSC_enterMutex_Exit(_cacheName);
	return rc;
}

/**
 * Relinquish the exclusive access right
 *
 * If this method succeeds, the caller will return the exclusive access right to the lock specified.
 * If there is one or more thread(s) suspended on the Mutex by calling @ref SH_OSCache::acquireWriteLock,
 * then one of the threads will be resumed and become the new owner of the exclusive access rights for the lock
 *
 * @param[in] lockID  The ID of the lock to release
 *
 * @return 0 if the operations has been successful, -1 if an error has occured
 */
IDATA
SH_OSCachesysv::releaseWriteLock(UDATA lockID)
{
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
	IDATA rc;
	Trc_SHR_OSC_exitMutex_Entry(_cacheName);
	if(_semhandle == NULL) {
		Trc_SHR_OSC_exitMutex_Exit1();
		Trc_SHR_Assert_ShouldNeverHappen();
		return -1;
	}

	if (lockID > (_totalNumSems-1)) {
		Trc_SHR_OSC_exitMutex_Exit2_V2(lockID, _totalNumSems);
		Trc_SHR_Assert_ShouldNeverHappen();
		return -1;
	}

	rc = omrshsem_deprecated_post(_semhandle, lockID, OMRPORT_SHSEM_MODE_UNDO);
	Trc_SHR_OSC_exitMutex_Exit(_cacheName);
	return rc;
}

IDATA
SH_OSCachesysv::enterHeaderMutex(LastErrorInfo *lastErrorInfo)
{
  OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
  IDATA rc = 0;
  Trc_SHR_OSC_GlobalLock_getMutex(_cacheName);

  if (NULL != lastErrorInfo) {
    lastErrorInfo->lastErrorCode = 0;
  }

  if (_semhandle != NULL) {
    rc = omrshsem_deprecated_wait(_semhandle, SEM_HEADERLOCK, OMRPORT_SHSEM_MODE_UNDO);
    if (-1 == rc) {
      if (NULL != lastErrorInfo) {
	lastErrorInfo->lastErrorCode = omrerror_last_error_number();
	lastErrorInfo->lastErrorMsg = omrerror_last_error_message();
      }
    }
  }

  Trc_SHR_OSC_GlobalLock_gotMutex(_cacheName);
  return rc;
}

IDATA
SH_OSCachesysv::exitHeaderMutex(LastErrorInfo *lastErrorInfo)
{
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
	IDATA rc = 0;
	if (NULL != lastErrorInfo) {
		lastErrorInfo->lastErrorCode = 0;
	}
	if (_semhandle != NULL) {
		rc = omrshsem_deprecated_post(_semhandle, SEM_HEADERLOCK, OMRPORT_SHSEM_MODE_UNDO);
		if (-1 == rc) {
			if (NULL != lastErrorInfo) {
				lastErrorInfo->lastErrorCode = omrerror_last_error_number();
				lastErrorInfo->lastErrorMsg = omrerror_last_error_message();
			}
		}
	}
	Trc_SHR_OSC_GlobalLock_released();
	return rc;
}

UDATA
SH_OSCachesysv::isCacheActive(void)
{
	IDATA rc;
	OMRPortShmemStatistic statbuf;
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

	rc = omrshmem_stat(_cacheDirName, _groupPerm, _shmFileName, &statbuf);
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
SH_OSCachesysv::printErrorMessage(LastErrorInfo *lastErrorInfo)
{
	I_32 errorCode = lastErrorInfo->lastErrorCode;
	I_32 errorCodeMasked = (errorCode | OMRPORT_ERROR_SYSTEM_CALL_ERRNO_MASK);
	const char * errormsg = lastErrorInfo->lastErrorMsg;
	I_32 sysFnCode = (errorCode - errorCodeMasked);
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

	if (errorCode!=0) {
		/*If errorCode is 0 then there is no error*/
		OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_PORT_ERROR_NUMBER, errorCode);
		Trc_SHR_Assert_True(errormsg != NULL);
		OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_PORT_ERROR_MESSAGE, errormsg);
	}

	/*Handle general errors*/
	switch(errorCodeMasked) {
		case OMRPORT_ERROR_SHMEM_TOOBIG:
		case OMRPORT_ERROR_SYSV_IPC_ERRNO_E2BIG:
#if defined(J9ZOS390)
			OSC_ERR_TRACE(J9NLS_SHRC_OSCACHE_ERROR_SHMEM_TOOBIG_ZOS);
#elif defined(AIXPPC)
			OSC_ERR_TRACE(J9NLS_SHRC_OSCACHE_ERROR_SHMEM_TOOBIG_AIX);
#else
			OSC_ERR_TRACE(J9NLS_SHRC_OSCACHE_ERROR_SHMEM_TOOBIG);
#endif
			break;
		case OMRPORT_ERROR_FILE_NAMETOOLONG:
			OSC_ERR_TRACE(J9NLS_SHRC_OSCACHE_ERROR_FILE_NAMETOOLONG);
			break;
		case OMRPORT_ERROR_SHMEM_DATA_DIRECTORY_FAILED:
		case OMRPORT_ERROR_FILE_NOPERMISSION:
		case OMRPORT_ERROR_SYSV_IPC_ERRNO_EPERM:
		case OMRPORT_ERROR_SYSV_IPC_ERRNO_EACCES:
			OSC_ERR_TRACE(J9NLS_SHRC_OSCACHE_ERROR_SHSEM_NOPERMISSION);
			break;
		case OMRPORT_ERROR_SYSV_IPC_ERRNO_ENOSPC:
			if (OMRPORT_ERROR_SYSV_IPC_SEMGET_ERROR == sysFnCode) {
				OSC_ERR_TRACE(J9NLS_SHRC_OSCACHE_ERROR_SEMAPHORE_LIMIT_REACHED);
			} else if (OMRPORT_ERROR_SYSV_IPC_SHMGET_ERROR == sysFnCode) {
				OSC_ERR_TRACE(J9NLS_SHRC_OSCACHE_ERROR_SHARED_MEMORY_LIMIT_REACHED);
			} else {
				OSC_ERR_TRACE(J9NLS_SHRC_OSCACHE_ERROR_SHSEM_NOSPACE);
			}
			break;
		case OMRPORT_ERROR_SYSV_IPC_ERRNO_ENOMEM:
			OSC_ERR_TRACE(J9NLS_SHRC_OSCACHE_ERROR_SHSEM_NOSPACE);
			break;
		case OMRPORT_ERROR_SYSV_IPC_ERRNO_EMFILE:
			OSC_ERR_TRACE(J9NLS_SHRC_OSCACHE_ERROR_MAX_OPEN_FILES_REACHED);
			break;

		default:
			break;
	}

}

/**
 * Method to clean up semaphore set and shared memory resources as part of error handling.
 */
void
SH_OSCachesysv::cleanupSysvResources(void)
{
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

	/* Setting handles to null prevents further use of this class, only if we haven't finished startup */
	if (NULL != _shmhandle) {
		/* When ::startup() calls omrshmem_open() the cache is opened attached.
		 * So, we must detach if we want clean up to work (see isCacheActive call below)
		 */
		omrshmem_detach(&_shmhandle);
	}

#if !defined(WIN32)
	/*If someone is still attached, don't destroy it*/
	/* isCacheActive isn't really accurate for Win32, so can't check */
	if(isCacheActive()) {
		if (NULL != _semhandle) {
			omrshsem_deprecated_close(&_semhandle);
			OSC_ERR_TRACE(J9NLS_SHRC_OSCACHE_HANDLE_ERROR_ACTION_CLOSESEM);
		}
		if (NULL != _shmhandle) {
			omrshmem_close(&_shmhandle);
			OSC_ERR_TRACE(J9NLS_SHRC_OSCACHE_HANDLE_ERROR_ACTION_CLOSESM);
		}
		return;
	}
#endif

	if ((NULL != _semhandle) && (OMRSH_SEM_ACCESS_ALLOWED == _semAccess)) {
#if defined(WIN32)
		if (omrshsem_deprecated_destroy(&_semhandle) == 0) {
			OSC_TRACE(J9NLS_SHRC_OSCACHE_HANDLE_ERROR_ACTION_DESTROYED_SEM);
		} else {
			I_32 errorno = omrerror_last_error_number();
			const char * errormsg = omrerror_last_error_message();
			OSC_ERR_TRACE(J9NLS_SHRC_OSCACHE_HANDLE_ERROR_ACTION_DESTROYSEM_ERROR);
			OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_PORT_ERROR_NUMBER_SYSV_ERR_RECOVER, errorno);
			Trc_SHR_Assert_True(errormsg != NULL);
			OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_PORT_ERROR_MESSAGE_SYSV_ERR_RECOVER, errormsg);
		}
#else
		I_32 semid = omrshsem_deprecated_getid(_semhandle);

		if (omrshsem_deprecated_destroy(&_semhandle) == 0) {
			OSC_TRACE1(J9NLS_SHRC_OSCACHE_HANDLE_ERROR_ACTION_DESTROYED_SEM_WITH_SEMID, semid);
		} else {
			I_32 errorno = omrerror_last_error_number();
			const char * errormsg = omrerror_last_error_message();
			I_32 lastError = errorno | OMRPORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
			I_32 lastSysCall = errorno - lastError;

			if ((OMRPORT_ERROR_SYSV_IPC_SEMCTL_ERROR == lastSysCall) && (OMRPORT_ERROR_SYSV_IPC_ERRNO_EPERM == lastError)) {
				OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_HANDLE_ERROR_ACTION_DESTROYSEM_NOT_PERMITTED, semid);
			} else {
				OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_HANDLE_ERROR_ACTION_DESTROYSEM_ERROR_V1, semid);
				OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_PORT_ERROR_NUMBER_SYSV_ERR_RECOVER, errorno);
				Trc_SHR_Assert_True(errormsg != NULL);
				OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_PORT_ERROR_MESSAGE_SYSV_ERR_RECOVER, errormsg);
			}
		}
#endif
	}

	if ((NULL != _shmhandle) && (OMRSH_SHM_ACCESS_ALLOWED == _shmAccess)) {
#if defined(WIN32)
		if (omrshmem_destroy(_cacheDirName, _groupPerm, &_shmhandle) == 0) {
			OSC_TRACE(J9NLS_SHRC_OSCACHE_HANDLE_ERROR_ACTION_DESTROYED_SHM);
		} else {
			I_32 errorno = omrerror_last_error_number();
			const char * errormsg = omrerror_last_error_message();
			OSC_ERR_TRACE(J9NLS_SHRC_OSCACHE_HANDLE_ERROR_ACTION_DESTROYSM_ERROR);
			OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_PORT_ERROR_NUMBER_SYSV_ERR_RECOVER, errorno);
			Trc_SHR_Assert_True(errormsg != NULL);
			OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_PORT_ERROR_MESSAGE_SYSV_ERR_RECOVER, errormsg);
		}
#else
		I_32 shmid = omrshmem_getid(_shmhandle);

		if (omrshmem_destroy(_cacheDirName, _groupPerm, &_shmhandle) == 0) {
			OSC_TRACE1(J9NLS_SHRC_OSCACHE_HANDLE_ERROR_ACTION_DESTROYED_SHM_WITH_SHMID, shmid);
		} else {
			I_32 errorno = omrerror_last_error_number();
			const char * errormsg = omrerror_last_error_message();
			I_32 lastError = errorno | OMRPORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
			I_32 lastSysCall = errorno - lastError;

			if ((OMRPORT_ERROR_SYSV_IPC_SHMCTL_ERROR == lastSysCall) && (OMRPORT_ERROR_SYSV_IPC_ERRNO_EPERM == lastError)) {
				OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_HANDLE_ERROR_ACTION_DESTROYSHM_NOT_PERMITTED, shmid);
			} else {
				OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_HANDLE_ERROR_ACTION_DESTROYSM_ERROR_V1, shmid);
				OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_PORT_ERROR_NUMBER_SYSV_ERR_RECOVER, errorno);
				Trc_SHR_Assert_True(errormsg != NULL);
				OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_PORT_ERROR_MESSAGE_SYSV_ERR_RECOVER, errormsg);
			}
		}
#endif
	}
}

void
SH_OSCachesysv::errorHandler(U_32 module_name, U_32 message_num, LastErrorInfo *lastErrorInfo)
{
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

	if (module_name && message_num && _verboseFlags) {
		omrnls_printf(J9NLS_ERROR, module_name, message_num);
		if ((NULL != lastErrorInfo) && (0 != lastErrorInfo->lastErrorCode)) {
			printErrorMessage(lastErrorInfo);
		}
	}

	setError(OMRSH_OSCACHE_FAILURE);
	if(!_startupCompleted && _openSharedMemory == false) {
		cleanupSysvResources();
	}
}

void
SH_OSCachesysv::setError(IDATA ec)
{
	_errorCode = ec;
}

/**
 * Returns the error code for the previous operation.
 * This is not yet thread safe and currently should only use for detecting error in the constructor.
 *
 * @return Error Code
 */
IDATA
SH_OSCachesysv::getError(void)
{
	return _errorCode;
}

IDATA
SH_OSCachesysv::shmemOpenWrapper(const char *cacheName, LastErrorInfo *lastErrorInfo)
{
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
	IDATA rc = 0;
	U_32 perm = (_openMode & J9OSCACHE_OPEN_MODE_DO_READONLY) ? OMRSH_SHMEM_PERM_READ : OMRSH_SHMEM_PERM_READ_WRITE;
	LastErrorInfo localLastErrorInfo;
	UDATA flags = OMRSHMEM_NO_FLAGS;

	Trc_SHR_OSC_shmemOpenWrapper_Entry(cacheName);

	if (OMR_ARE_ANY_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_STATS)) {
		flags |= OMRSHMEM_OPEN_FOR_STATS;
	} else if (OMR_ARE_ANY_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_DESTROY)) {
		flags |= OMRSHMEM_OPEN_FOR_DESTROY;
	} else if (OMR_ARE_ANY_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_DO_NOT_CREATE)) {
		flags |= OMRSHMEM_OPEN_DO_NOT_CREATE;
	}

#if !defined(WIN32)
	rc = OpenSysVMemoryHelper(cacheName, perm, &localLastErrorInfo);
#else
	rc = omrshmem_open(_cacheDirName, _groupPerm, &_shmhandle, cacheName, _cacheSize, perm, OMRMEM_CATEGORY_CLASSES_SHC_CACHE, flags, NULL);
	localLastErrorInfo.lastErrorCode = omrerror_last_error_number();
	localLastErrorInfo.lastErrorMsg = omrerror_last_error_message();
#endif

#if defined(J9ZOS390)
	if (OMRPORT_ERROR_SHMEM_ZOS_STORAGE_KEY_READONLY == rc) {
		Trc_SHR_OSC_Event_OpenReadOnly_StorageKey();
		_openMode |= J9OSCACHE_OPEN_MODE_DO_READONLY;
		perm = OMRSH_SHMEM_PERM_READ;
		rc = OpenSysVMemoryHelper(cacheName, perm, &localLastErrorInfo);
	}
#endif

	if (OMRPORT_ERROR_SHMEM_OPFAILED == rc) {
#if !defined(WIN32)
		if (_activeGeneration >= 7) {
#endif
			if (_openMode & J9OSCACHE_OPEN_MODE_TRY_READONLY_ON_FAIL) {
				_openMode |= J9OSCACHE_OPEN_MODE_DO_READONLY;
				perm = OMRSH_SHMEM_PERM_READ;
				rc = omrshmem_open(_cacheDirName, _groupPerm, &_shmhandle, cacheName, _cacheSize, perm, OMRMEM_CATEGORY_CLASSES_SHC_CACHE, OMRSHMEM_NO_FLAGS, &_controlFileStatus);
				/* if omrshmem_open is successful, portable error number is set to 0 */
				localLastErrorInfo.lastErrorCode = omrerror_last_error_number();
				localLastErrorInfo.lastErrorMsg = omrerror_last_error_message();
			}
#if !defined(WIN32)
		}
#endif
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

void
SH_OSCachesysv::runExitCode(void)
{
	/* No action required */
}

#if defined (OMRSHR_MSYNC_SUPPORT)
/**
 * Synchronise cache updates to disk
 *
 * This function is not supported for the shared memory cache
 *
 * @return -1 as this function is not supported
 */
IDATA
SH_OSCachesysv::syncUpdates(void* start, UDATA length, U_32 flags)
{
	return -1;
}
#endif

/**
 * Return the locking capabilities of this shared classes cache implementation
 *
 * Write locks (only) are supported for this implementation
 *
 * @return J9OSCACHE_DATA_WRITE_LOCK
 */
IDATA
SH_OSCachesysv::getLockCapabilities(void)
{
	return J9OSCACHE_DATA_WRITE_LOCK;
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
SH_OSCachesysv::setRegionPermissions(struct OMRPortLibrary* portLibrary, void *address, UDATA length, UDATA flags)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	return omrshmem_protect(_cacheDirName, _groupPerm, address, length, flags);
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
SH_OSCachesysv::getPermissionsRegionGranularity(struct OMRPortLibrary* portLibrary)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	return omrshmem_get_region_granularity(_cacheDirName, _groupPerm, (void*)_dataStart);
}

/**
 * Returns the total size of the cache memory
 *
 * This value is not derived from the cache header, so is reliable even if the cache is corrupted
 *
 * @return size of cache memory
 */
U_32
SH_OSCachesysv::getTotalSize()
{
	OMRPortShmemStatistic statbuf;
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

	if (_actualCacheSize > 0) {
		return _actualCacheSize;
	}

	if (omrshmem_stat(_cacheDirName, _groupPerm, _shmFileName, &statbuf) == (UDATA)-1) {
		/* CMVC 143141: If shared memory can not be stat'd then it
		 * doesn't exist.  In this case we return 0 because a cache
		 * that does not exist on the system then it has zero size.
		 */
		return 0;
	}

	_actualCacheSize = (U_32)statbuf.size;
	return _actualCacheSize;
}


UDATA
SH_OSCachesysv::getHeaderSize(void)
{
	return SHM_CACHEHEADERSIZE;
}

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
SH_OSCachesysv::getCacheStatsHelper(OMR_VM* vm, const char* cacheDirName, UDATA groupPerm, const char* cacheNameWithVGen, SH_OSCache_Info* cacheInfo, UDATA reason)
{
	OMRPORT_ACCESS_FROM_OMRPORT(vm->_runtime->_portLibrary);
	OMRPortShmemStatistic statbuf;
	UDATA versionLen;
	UDATA statrc = 0;

	Trc_SHR_OSC_Sysv_getCacheStatsHelper_Entry(cacheNameWithVGen);

//#if defined(WIN32)
//	versionLen = J9SH_VERSION_STRING_LEN;
//#else
//	versionLen = J9SH_VERSION_STRING_LEN + strlen(OMRSH_MEMORY_ID) - 1;
//#endif

	if (removeCacheVersionAndGen(cacheInfo->name, CACHE_ROOT_MAXLEN, versionLen, cacheNameWithVGen) != 0) {
		Trc_SHR_OSC_Sysv_getCacheStatsHelper_removeCacheVersionAndGenFailed();
		return -1;
	}
#if !defined(WIN32)
	statrc = SH_OSCachesysv::StatSysVMemoryHelper(OMRPORTLIB, cacheDirName, groupPerm, cacheNameWithVGen, &statbuf);
#else
	statrc = omrshmem_stat(cacheDirName, groupPerm, cacheNameWithVGen, &statbuf);
#endif

	if (statrc == 0) {
#if defined(J9ZOS390)
		if ((J9SH_ADDRMODE != cacheInfo->versionData.addrmode) && (0 == statbuf.size)) {
			/* JAZZ103 PR 79909 + PR 88930: On z/OS, if the shared memory segment of the cache is created by a 64-bit JVM but
			 * is being accessed by a 31-bit JVM, then the size of shared memory segment may not be verifiable.
			 * In such cases, we should avoid doing any operation - destroy or list -
			 * to avoid accidental access to shared memory segment belonging to some other application.
			 */
			Trc_SHR_Assert_True(J9SH_ADDRMODE_32 == J9SH_ADDRMODE);
			omrnls_printf( J9NLS_INFO, J9NLS_SHRC_OSCACHE_SHARED_CACHE_SIZE_ZERO_AND_DIFF_ADDRESS_MODE, J9SH_ADDRMODE_64, J9SH_ADDRMODE_64, cacheInfo->name, J9SH_ADDRMODE_32);
			Trc_SHR_OSC_Sysv_getCacheStatsHelper_shmSizeZeroDiffAddrMode();
			return -1;
		}
#endif /* defined(J9ZOS390) */
		cacheInfo->os_shmid = (statbuf.shmid!=(UDATA)-1)?statbuf.shmid:(UDATA)OMRSH_OSCACHE_UNKNOWN;
		/* os_semid is populated in getJavacoreData() as we can't access _semid here */
		cacheInfo->os_semid = (UDATA)OMRSH_OSCACHE_UNKNOWN;
		cacheInfo->lastattach = (statbuf.lastAttachTime!=-1)?(statbuf.lastAttachTime*1000):OMRSH_OSCACHE_UNKNOWN;
		cacheInfo->lastdetach = (statbuf.lastDetachTime!=-1)?(statbuf.lastDetachTime*1000):OMRSH_OSCACHE_UNKNOWN;
		cacheInfo->createtime = OMRSH_OSCACHE_UNKNOWN;
		cacheInfo->nattach = (statbuf.nattach!=(UDATA)-1)?statbuf.nattach:(UDATA)OMRSH_OSCACHE_UNKNOWN;
	} else if ((SHR_STATS_REASON_DESTROY == reason) || (SHR_STATS_REASON_EXPIRE == reason)) {
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
IDATA
SH_OSCachesysv::getCacheStats(OMR_VM* vm, const char* ctrlDirName, UDATA groupPerm, const char* cacheNameWithVGen, SH_OSCache_Info* cacheInfo, UDATA reason)
{
	IDATA retval = 0;
	OMRPORT_ACCESS_FROM_VMC(vm);
	char cacheDirName[OMRSH_MAXPATH];

	SH_OSCache::getCacheDir(vm, ctrlDirName, cacheDirName, OMRSH_MAXPATH, OMRPORT_SHR_CACHE_TYPE_NONPERSISTENT);
	if (SH_OSCachesysv::getCacheStatsHelper(vm, cacheDirName, groupPerm, cacheNameWithVGen, cacheInfo, reason) == 0) {
		/* Using 'SH_OSCachesysv cacheStruct' breaks the pattern of calling getRequiredConstrBytes(), and then allocating memory.
		 * However it is consistent with 'SH_OSCachemmap::getCacheStats'.
		 */
		SH_OSCachesysv cacheStruct;
		SH_OSCachesysv * cache = NULL;
		J9PortShcVersion versionData;
		OMRSharedCachePreinitConfig piconfig;
		bool attachedMem = false;

		getValuesFromShcFilePrefix(OMRPORTLIB, cacheNameWithVGen, &versionData);
		versionData.cacheType = OMRPORT_SHR_CACHE_TYPE_NONPERSISTENT;

		if ((SHR_STATS_REASON_ITERATE == reason) || (SHR_STATS_REASON_LIST == reason)) {
			cache = (SH_OSCachesysv *) SH_OSCache::newInstance(OMRPORTLIB, &cacheStruct, cacheInfo->name, cacheInfo->generation, &versionData);

			if (!cache->startup(vm, ctrlDirName, vm->sharedCacheAPI->cacheDirPerm, cacheInfo->name, &piconfig, SH_CompositeCacheImpl::getNumRequiredOSLocks(), OMRSH_OSCACHE_OPEXIST_STATS, 0, 0/*runtime flags*/, J9OSCACHE_OPEN_MODE_TRY_READONLY_ON_FAIL, vm->sharedCacheAPI->storageKeyTesting, &versionData, NULL, reason)) {
				goto done;
			}

			/* Avoid calling attach() for incompatible cache since its anyway going to fail.
			 * But we can still get semid from _semhandle as it got populated during startup itself.
			 */
			if (0 == cacheInfo->isCompatible) {
				/* if the cache was opened read-only, there is no _semhandle */
				if (NULL != cache->_semhandle) {
					/* SHR_STATS_REASON_LIST needs to populate the os_semid */
					cacheInfo->os_semid = cache->_semid = omrshsem_deprecated_getid(cache->_semhandle);
				}
			} else {
				/* Attach to the cache, so we can read the fields in the header */
			       if (NULL == cache->attach(omr_vmthread_getCurrent(vm), NULL)) {
					cache->cleanup();
					goto done;
				}
				attachedMem = true;

				/* SHR_STATS_REASON_LIST needs to populate the os_semid */
				if (0 != cache->_semid) {
					cacheInfo->os_semid = cache->_semid;
				}

				if (SHR_STATS_REASON_ITERATE == reason) {
					getCacheStatsCommon(vm, cache, cacheInfo);
				}

				if (attachedMem == true) {
					cache->detach();
				}
			}
			cache->cleanup();
		}
	} else {
		retval = -1;
	}
done:
	return retval;
}

/**
 * Find the first cache file in a given cacheDir
 * Follows the format of omrfile_findfirst
 *
 * @param [in] portLibrary  A port library
 * @param [in] sharedClassConfig
 * @param [out] resultbuf  The name of the file found
 *
 * @return A handle to the first cache file found or -1 if the cache doesn't exist
 */
UDATA
SH_OSCachesysv::findfirst(struct OMRPortLibrary *portLibrary, char *cacheDir, char *resultbuf)
{
	UDATA rc;
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	Trc_SHR_OSC_Sysv_findfirst_Entry();

	rc = omrshmem_findfirst(cacheDir, resultbuf);

	Trc_SHR_OSC_Sysv_findfirst_Exit(rc);
	return rc;
}

/**
 * Find the next file in a given cacheDir
 * Follows the format of omrfile_findnext
 *
 * @param [in] portLibrary  A port library
 * @param [in] findHandle  The handle of the last file found
 * @param [out] resultbuf  The name of the file found
 *
 * @return A handle to the cache file found or -1 if the cache doesn't exist
 */
I_32
SH_OSCachesysv::findnext(struct OMRPortLibrary *portLibrary, UDATA findHandle, char *resultbuf)
{
	I_32 rc;
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	Trc_SHR_OSC_Sysv_findnext_Entry(findHandle);

	rc = omrshmem_findnext(findHandle, resultbuf);

	Trc_SHR_OSC_Sysv_findnext_Exit(rc);
	return rc;
}

/**
 * Finish finding caches
 * Follows the format of omrfile_findclose
 *
 * @param [in] portLibrary  A port library
 * @param [in] findHandle  The handle of the last file found
 */
void
SH_OSCachesysv::findclose(struct OMRPortLibrary *portLibrary, UDATA findhandle)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	Trc_SHR_OSC_Sysv_findclose_Entry();
	omrshmem_findclose(findhandle);
	Trc_SHR_OSC_Sysv_findclose_Exit();
}

/* Used for connecting to legacy cache headers */
void*
SH_OSCachesysv::getSysvHeaderFieldAddressForGen(void* header, UDATA headerGen, UDATA fieldID)
{
	return (U_8*)header + getSysvHeaderFieldOffsetForGen(headerGen, fieldID);
}

/* Used for connecting to legacy cache headers */
IDATA
SH_OSCachesysv::getSysvHeaderFieldOffsetForGen(UDATA headerGen, UDATA fieldID)
{
	if ((4 < headerGen) && (headerGen <= OSCACHE_CURRENT_CACHE_GEN)) {
		switch (fieldID) {
		case OSCACHESYSV_HEADER_FIELD_IN_DEFAULT_CONTROL_DIR :
			return offsetof(OSCachesysv_header_version_current, inDefaultControlDir);
		default :
			return offsetof(OSCachesysv_header_version_current, oscHdr) + getHeaderFieldOffsetForGen(headerGen, fieldID);
		}
	} else if (4 == headerGen) {
		switch (fieldID) {
		case OSCACHESYSV_HEADER_FIELD_IN_DEFAULT_CONTROL_DIR :
			return offsetof(OSCachesysv_header_version_G04, inDefaultControlDir);
		default :
			return offsetof(OSCachesysv_header_version_G04, oscHdr) + getHeaderFieldOffsetForGen(headerGen, fieldID);
		}
	} else if (3 == headerGen) {
		switch (fieldID) {
		case OSCACHESYSV_HEADER_FIELD_IN_DEFAULT_CONTROL_DIR :
			return offsetof(OSCachesysv_header_version_G03, inDefaultControlDir);
		case OSCACHESYSV_HEADER_FIELD_CACHE_INIT_COMPLETE :
			return offsetof(OSCachesysv_header_version_G03, cacheInitComplete);
		default :
			return offsetof(OSCachesysv_header_version_G03, oscHdr) + getHeaderFieldOffsetForGen(headerGen, fieldID);
		}
	}
	Trc_SHR_Assert_ShouldNeverHappen();
	return 0;
}

//UDATA
//SH_OSCachesysv::getJavacoreData(OMR_VM *vm, J9SharedClassJavacoreDataDescriptor* descriptor)
//{
//#if !defined(WIN32)
//	SH_OSCache_Info cacheInfo;
//#endif
//
//	descriptor->cacheGen = _activeGeneration;
//#if defined(WIN32)
//	descriptor->shmid = descriptor->semid = -2;
//#else
//	if (getCacheStatsHelper(vm, _cacheDirName, _groupPerm, _cacheNameWithVGen, &cacheInfo, SHR_STATS_REASON_ITERATE) != 0) {
//		return 0;
//	}
//	descriptor->shmid = cacheInfo.os_shmid;
//	descriptor->semid = cacheInfo.os_semid;
//	if (_semid != -1) {
//		descriptor->semid = cacheInfo.os_semid = _semid;
//	}
//#endif
//	descriptor->cacheDir = _cachePathName;
//	return 1;
//}

/**
 * This method performs additional checks to catch scenarios that are not handled by permission and/or mode settings provided by operating system,
 * to avoid any unintended access to sempahore set.
 *
 * @param[in] lastErrorInfo	Pointer to store last portable error code and/or message
 *
 * @return enum SH_SysvSemAccess indicating if the process can access the semaphore set or not
 */
SH_SysvSemAccess
SH_OSCachesysv::checkSemaphoreAccess(LastErrorInfo *lastErrorInfo)
{
	SH_SysvSemAccess semAccess = OMRSH_SEM_ACCESS_ALLOWED;

	if (NULL != lastErrorInfo) {
		lastErrorInfo->lastErrorCode = 0;
	}

#if !defined(WIN32)
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

	if (NULL == _semhandle) {
		semAccess = OMRSH_SEM_ACCESS_ALLOWED;
	} else {
		IDATA rc;
		OMRPortShsemStatistic statBuf;
		I_32 semid = omrshsem_deprecated_getid(_semhandle);

		memset(&statBuf, 0, sizeof(statBuf));
		rc = omrshsem_deprecated_handle_stat(_semhandle, &statBuf);

		if (OMRPORT_INFO_SHSEM_STAT_PASSED == rc) {
			UDATA uid = omrsysinfo_get_euid();

			if (statBuf.cuid != uid) {
				if (statBuf.ouid == uid) {
					/* Current process is owner but not the creator of the semaphore set.
					 * This implies external entity assigned ownership of the semaphore set to this process.
					 */
					semAccess = OMRSH_SEM_ACCESS_OWNER_NOT_CREATOR;
					Trc_SHR_OSC_Sysv_checkSemaphoreAccess_OwnerNotCreator(uid, semid, statBuf.cuid, statBuf.ouid);
				} else {
					UDATA gid = omrsysinfo_get_egid();
					bool sameGroup = false;

					if ((statBuf.cgid == gid) || (statBuf.ogid == gid)) {
						sameGroup = true;
						Trc_SHR_OSC_Sysv_checkSemaphoreAccess_GroupIDMatch(gid, semid, statBuf.cgid, statBuf.ogid);
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
									Trc_SHR_OSC_Sysv_checkSemaphoreAccess_SupplementaryGroupMatch(list[i], statBuf.cgid, statBuf.ogid, semid);
									break;
								}
							}
						} else {
							semAccess = OMRSH_SEM_ACCESS_CANNOT_BE_DETERMINED;
							if (NULL != lastErrorInfo) {
								lastErrorInfo->lastErrorCode = omrerror_last_error_number();
								lastErrorInfo->lastErrorMsg = omrerror_last_error_message();
							}
							Trc_SHR_OSC_Sysv_checkSemaphoreAccess_GetGroupsFailed();
							goto _end;
						}
						if (NULL != list) {
							omrmem_free_memory(list);
						}
					}
					if (sameGroup) {
						/* This process belongs to same group as owner or creator of the semaphore set. */
						if (0 == _groupPerm) {
							/* If 'groupAccess' option is not set, it implies this process wants to attach to a shared cache that it owns or created.
							 * But this process is neither creator nor owner of the semaphore set.
							 * This implies we should not allow this process to use the cache.
							 */
							semAccess = OMRSH_SEM_ACCESS_GROUP_ACCESS_REQUIRED;
							Trc_SHR_OSC_Sysv_checkSemaphoreAccess_GroupAccessRequired(semid);
						}
					} else {
						/* This process does not belong to same group as owner or creator of the semaphore set.
						 * Do not allow access to the cache.
						 */
						semAccess = OMRSH_SEM_ACCESS_OTHERS_NOT_ALLOWED;
						Trc_SHR_OSC_Sysv_checkSemaphoreAccess_OthersNotAllowed(semid);
					}
				}
			}
		} else {
			semAccess = OMRSH_SEM_ACCESS_CANNOT_BE_DETERMINED;
			if (NULL != lastErrorInfo) {
				lastErrorInfo->lastErrorCode = omrerror_last_error_number();
				lastErrorInfo->lastErrorMsg = omrerror_last_error_message();
			}
			Trc_SHR_OSC_Sysv_checkSemaphoreAccess_ShsemStatFailed(semid);
		}
	}

_end:
#endif /* !defined(WIN32) */

	return semAccess;
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
SH_OSCachesysv::checkSharedMemoryAccess(LastErrorInfo *lastErrorInfo)
{
	SH_SysvShmAccess shmAccess = OMRSH_SHM_ACCESS_ALLOWED;

	if (NULL != lastErrorInfo) {
		lastErrorInfo->lastErrorCode = 0;
	}

#if !defined(WIN32)
	IDATA rc = -1;
	OMRPortShmemStatistic statBuf;
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
	I_32 shmid = omrshmem_getid(_shmhandle);

	memset(&statBuf, 0, sizeof(statBuf));
	rc = omrshmem_handle_stat(_shmhandle, &statBuf);
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
							lastErrorInfo->lastErrorCode = omrerror_last_error_number();
							lastErrorInfo->lastErrorMsg = omrerror_last_error_message();
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
			lastErrorInfo->lastErrorCode = omrerror_last_error_number();
			lastErrorInfo->lastErrorMsg = omrerror_last_error_message();
		}
		Trc_SHR_OSC_Sysv_checkSharedMemoryAccess_ShmemStatFailed(shmid);
	}

_end:
#endif /* !defined(WIN32) */

	return shmAccess;
}

/**
 * Returns if the cache is accessible by current user or not
 *
 * @return enum SH_CacheAccess
 */
SH_CacheAccess
SH_OSCachesysv::isCacheAccessible(void) const
{
	if (OMRSH_SHM_ACCESS_ALLOWED == _shmAccess) {
		return J9SH_CACHE_ACCESS_ALLOWED;
	} else if (OMRSH_SHM_ACCESS_GROUP_ACCESS_REQUIRED == _shmAccess) {
		return J9SH_CACHE_ACCESS_ALLOWED_WITH_GROUPACCESS;
	} else if (OMRSH_SHM_ACCESS_GROUP_ACCESS_READONLY_REQUIRED == _shmAccess) {
		return J9SH_CACHE_ACCESS_ALLOWED_WITH_GROUPACCESS_READONLY;
	} else {
		return J9SH_CACHE_ACCESS_NOT_ALLOWED;
	}
}

#if !defined(WIN32)
/*Helpers for opening Unix SysV Semaphores and control files*/

IDATA
SH_OSCachesysv::OpenSysVSemaphoreHelper(J9PortShcVersion* versionData, LastErrorInfo *lastErrorInfo)
{
	IDATA rc = -1;
	UDATA flags = OMRSHSEM_NO_FLAGS;
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

	Trc_SHR_OSC_Sysv_OpenSysVSemaphoreHelper_Enter();

	U_64 cacheVMVersion = getCacheVersionToU64(versionData->esVersionMajor, versionData->esVersionMinor);
	UDATA action;

	if (NULL != lastErrorInfo) {
		lastErrorInfo->lastErrorCode = 0;
	}

	action = SH_OSCachesysv::SysVCacheFileTypeHelper(cacheVMVersion, _activeGeneration);

	if (OMR_ARE_ANY_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_STATS)) {
		flags |= OMRSHSEM_OPEN_FOR_STATS;
	} else if (OMR_ARE_ANY_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_DESTROY)) {
		flags |= OMRSHSEM_OPEN_FOR_DESTROY;
	} else if (OMR_ARE_ANY_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_DO_NOT_CREATE)) {
		flags |= OMRSHSEM_OPEN_DO_NOT_CREATE;
	}

	switch(action){
		case OMRSH_SYSV_REGULAR_CONTROL_FILE:
			rc = omrshsem_deprecated_open(_cacheDirName, _groupPerm, &_semhandle, _semFileName, (int)_totalNumSems, 0, flags, &_controlFileStatus);
			break;
		case OMRSH_SYSV_OLDER_EMPTY_CONTROL_FILE:
			rc = omrshsem_deprecated_openDeprecated(_cacheDirName, _groupPerm, &_semhandle, _semFileName, OMRSH_SYSV_OLDER_EMPTY_CONTROL_FILE);
			break;
		case OMRSH_SYSV_OLDER_CONTROL_FILE:
			rc = omrshsem_deprecated_openDeprecated(_cacheDirName, _groupPerm, &_semhandle, _semFileName, OMRSH_SYSV_OLDER_CONTROL_FILE);
			break;
		default:
			Trc_SHR_Assert_ShouldNeverHappen();
			break;
	}
	/* if above portLibrary call is successful, portable error number is set to 0 */
	if (NULL != lastErrorInfo) {
		lastErrorInfo->lastErrorCode = omrerror_last_error_number();
		lastErrorInfo->lastErrorMsg = omrerror_last_error_message();
	}
	Trc_SHR_OSC_Sysv_OpenSysVSemaphoreHelper_Exit(rc);
	return rc;
}

IDATA
SH_OSCachesysv::DestroySysVSemHelper()
{
	IDATA rc = -1;
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

	Trc_SHR_OSC_Sysv_DestroySysVSemHelper_Enter();

	J9PortShcVersion versionData;
	U_64 cacheVMVersion;
	UDATA genVersion;
	UDATA action;

	genVersion = getGenerationFromName(_semFileName);
	if (0 == getValuesFromShcFilePrefix(OMRPORTLIB, _semFileName, &versionData)) {
		goto done;
	}

	cacheVMVersion = getCacheVersionToU64(versionData.esVersionMajor, versionData.esVersionMinor);

	action = SH_OSCachesysv::SysVCacheFileTypeHelper(cacheVMVersion, genVersion);

	switch(action){
		case OMRSH_SYSV_REGULAR_CONTROL_FILE:
			rc = omrshsem_deprecated_destroy(&_semhandle);
			break;
		case OMRSH_SYSV_OLDER_EMPTY_CONTROL_FILE:
			rc = omrshsem_deprecated_destroyDeprecated(&_semhandle, OMRSH_SYSV_OLDER_EMPTY_CONTROL_FILE);
			break;
		case OMRSH_SYSV_OLDER_CONTROL_FILE:
			rc = omrshsem_deprecated_destroyDeprecated(&_semhandle, OMRSH_SYSV_OLDER_CONTROL_FILE);
			break;
		default:
			Trc_SHR_Assert_ShouldNeverHappen();
			break;
	}

	if (-1 == rc) {
#if !defined(WIN32)
		I_32 errorno = omrerror_last_error_number();
		I_32 lastError = errorno | OMRPORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
		I_32 lastSysCall = errorno - lastError;

		if ((OMRPORT_ERROR_SYSV_IPC_SEMCTL_ERROR == lastSysCall) && (OMRPORT_ERROR_SYSV_IPC_ERRNO_EPERM == lastError)) {
			OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_SEMAPHORE_DESTROY_NOT_PERMITTED, omrshsem_deprecated_getid(_semhandle));
		} else {
			const char * errormsg = omrerror_last_error_message();

			OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_DESTROYSEM_ERROR_WITH_SEMID, omrshsem_deprecated_getid(_semhandle));
			OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_PORT_ERROR_NUMBER_SYSV_ERR, errorno);
			Trc_SHR_Assert_True(errormsg != NULL);
			OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_PORT_ERROR_MESSAGE_SYSV_ERR, errormsg);
		}
#else /* !defined(WIN32) */
		I_32 errorno = omrerror_last_error_number();
		const char * errormsg = omrerror_last_error_message();

		OSC_ERR_TRACE(J9NLS_SHRC_OSCACHE_DESTROYSEM_ERROR);
		OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_PORT_ERROR_NUMBER_SYSV_ERR, errorno);
		Trc_SHR_Assert_True(errormsg != NULL);
		OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_PORT_ERROR_MESSAGE_SYSV_ERR, errormsg);
#endif /* !defined(WIN32) */
	}

done:
	Trc_SHR_OSC_Sysv_DestroySysVSemHelper_Exit(rc);
	return rc;

}

IDATA
SH_OSCachesysv::OpenSysVMemoryHelper(const char* cacheName, U_32 perm, LastErrorInfo *lastErrorInfo)
{
	IDATA rc = -1;
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

	Trc_SHR_OSC_Sysv_OpenSysVMemoryHelper_Enter();

	J9PortShcVersion versionData;
	U_64 cacheVMVersion;
	UDATA genVersion;
	UDATA action;
	UDATA flags = OMRSHMEM_NO_FLAGS;

	if (NULL != lastErrorInfo) {
		lastErrorInfo->lastErrorCode = 0;
	}
	genVersion = getGenerationFromName(cacheName);
	if (0 == getValuesFromShcFilePrefix(OMRPORTLIB, cacheName, &versionData)) {
		goto done;
	}

	cacheVMVersion = getCacheVersionToU64(versionData.esVersionMajor, versionData.esVersionMinor);

	action = SH_OSCachesysv::SysVCacheFileTypeHelper(cacheVMVersion, genVersion);

	switch(action){
		case OMRSH_SYSV_REGULAR_CONTROL_FILE:
			if (OMR_ARE_ANY_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_STATS)) {
				flags |= OMRSHMEM_OPEN_FOR_STATS;
			} else if (OMR_ARE_ANY_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_DESTROY)) {
				flags |= OMRSHMEM_OPEN_FOR_DESTROY;
			} else if (OMR_ARE_ANY_BITS_SET(_createFlags, OMRSH_OSCACHE_OPEXIST_DO_NOT_CREATE)) {
				flags |= OMRSHMEM_OPEN_DO_NOT_CREATE;
			}
#if defined(J9ZOS390)
			if (0 != (_runtimeFlags & OMRSHR_RUNTIMEFLAG_ENABLE_STORAGEKEY_TESTING)) {
				flags |=  OMRSHMEM_STORAGE_KEY_TESTING;
				flags |=  _storageKeyTesting << OMRSHMEM_STORAGE_KEY_TESTING_SHIFT;
			}
			flags |= OMRSHMEM_PRINT_STORAGE_KEY_WARNING;
#endif
			rc = omrshmem_open(_cacheDirName, _groupPerm, &_shmhandle, cacheName, _cacheSize, perm, OMRMEM_CATEGORY_CLASSES, flags, &_controlFileStatus);
			break;
		case OMRSH_SYSV_OLDER_EMPTY_CONTROL_FILE:
			rc = omrshmem_openDeprecated(_cacheDirName, _groupPerm, &_shmhandle, cacheName, perm, OMRSH_SYSV_OLDER_EMPTY_CONTROL_FILE, OMRMEM_CATEGORY_CLASSES);
			break;
		case OMRSH_SYSV_OLDER_CONTROL_FILE:
			rc = omrshmem_openDeprecated(_cacheDirName, _groupPerm, &_shmhandle, cacheName, perm, OMRSH_SYSV_OLDER_CONTROL_FILE, OMRMEM_CATEGORY_CLASSES);
			break;
		default:
			Trc_SHR_Assert_ShouldNeverHappen();
			break;
	}
	done:
	/* if above portLibrary call is successful, portable error number is set to 0 */
	if (NULL != lastErrorInfo) {
		lastErrorInfo->lastErrorCode = omrerror_last_error_number();
		lastErrorInfo->lastErrorMsg = omrerror_last_error_message();
	}
	Trc_SHR_OSC_Sysv_OpenSysVMemoryHelper_Exit(rc);
	return rc;
}

IDATA
SH_OSCachesysv::StatSysVMemoryHelper(OMRPortLibrary* portLibrary, const char* cacheDirName, UDATA groupPerm, const char* cacheNameWithVGen, OMRPortShmemStatistic * statbuf)
{
	IDATA rc = -1;
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	Trc_SHR_OSC_Sysv_StatSysVMemoryHelper_Enter();

	J9PortShcVersion versionData;
	U_64 cacheVMVersion;
	UDATA genVersion;
	UDATA action;

	genVersion = getGenerationFromName(cacheNameWithVGen);
	if (0 == getValuesFromShcFilePrefix(OMRPORTLIB, cacheNameWithVGen, &versionData)) {
		goto done;
	}

	cacheVMVersion = getCacheVersionToU64(versionData.esVersionMajor, versionData.esVersionMinor);

	action = SH_OSCachesysv::SysVCacheFileTypeHelper(cacheVMVersion, genVersion);

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

	done:
	Trc_SHR_OSC_Sysv_StatSysVMemoryHelper_Exit(rc);
	return rc;
}


IDATA
SH_OSCachesysv::DestroySysVMemoryHelper()
{
	IDATA rc = -1;
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

	Trc_SHR_OSC_Sysv_DestroySysVMemoryHelper_Enter();

	J9PortShcVersion versionData;
	U_64 cacheVMVersion;
	UDATA genVersion;
	UDATA action;

	genVersion = getGenerationFromName(_shmFileName);
	if (0 == getValuesFromShcFilePrefix(OMRPORTLIB, _shmFileName, &versionData)) {
		goto done;
	}

	cacheVMVersion = getCacheVersionToU64(versionData.esVersionMajor, versionData.esVersionMinor);

	action = SH_OSCachesysv::SysVCacheFileTypeHelper(cacheVMVersion, genVersion);

	switch(action){
		case OMRSH_SYSV_REGULAR_CONTROL_FILE:
			rc = omrshmem_destroy(_cacheDirName, _groupPerm, &_shmhandle);
			break;
		case OMRSH_SYSV_OLDER_EMPTY_CONTROL_FILE:
			rc = omrshmem_destroyDeprecated(_cacheDirName, _groupPerm, &_shmhandle, OMRSH_SYSV_OLDER_EMPTY_CONTROL_FILE);
			break;
		case OMRSH_SYSV_OLDER_CONTROL_FILE:
			rc = omrshmem_destroyDeprecated(_cacheDirName, _groupPerm, &_shmhandle, OMRSH_SYSV_OLDER_CONTROL_FILE);
			break;
		default:
			Trc_SHR_Assert_ShouldNeverHappen();
			break;
	}

	if (-1 == rc) {
#if !defined(WIN32)
		I_32 errorno = omrerror_last_error_number();
		I_32 lastError = errorno | OMRPORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
		I_32 lastSysCall = errorno - lastError;

		if ((OMRPORT_ERROR_SYSV_IPC_SHMCTL_ERROR == lastSysCall) && (OMRPORT_ERROR_SYSV_IPC_ERRNO_EPERM == lastError)) {
			OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_SHARED_MEMORY_DESTROY_NOT_PERMITTED, omrshmem_getid(_shmhandle));
		} else {
			const char * errormsg = omrerror_last_error_message();
			OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_DESTROYSHM_ERROR_WITH_SHMID, omrshmem_getid(_shmhandle));
			OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_PORT_ERROR_NUMBER_SYSV_ERR, errorno);
			Trc_SHR_Assert_True(errormsg != NULL);
			OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_PORT_ERROR_MESSAGE_SYSV_ERR, errormsg);
		}
#else /* !defined(WIN32) */
		I_32 errorno = omrerror_last_error_number();
		const char * errormsg = omrerror_last_error_message();
		OSC_ERR_TRACE(J9NLS_SHRC_OSCACHE_DESTROYSHM_ERROR);
		OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_PORT_ERROR_NUMBER_SYSV_ERR, errorno);
		Trc_SHR_Assert_True(errormsg != NULL);
		OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_PORT_ERROR_MESSAGE_SYSV_ERR, errormsg);
#endif /* !defined(WIN32) */
	}

done:
	Trc_SHR_OSC_Sysv_DestroySysVMemoryHelper_Exit(rc);
	return rc;

}


UDATA
SH_OSCachesysv::SysVCacheFileTypeHelper(U_64 currentVersion, UDATA genVersion)
{
	UDATA rc = OMRSH_SYSV_REGULAR_CONTROL_FILE;
	/*
	 * C230 VM is used by Java 5
	 * C240 VM is used by Java 6
	 * C250 VM is used by Java 6 WRT and SRT
	 * C260 VM is used by Java 7, possibly Java 6
	 */
	U_64 C230VMversion   = getCacheVersionToU64(2, 30);
	U_64 C240VMversion   = getCacheVersionToU64(2, 40);
	U_64 C250VMversion   = getCacheVersionToU64(2, 50);
	U_64 C260VMversion   = getCacheVersionToU64(2, 60);

	if (currentVersion >= C260VMversion) {
		switch(genVersion) {
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
				rc = OMRSH_SYSV_OLDER_CONTROL_FILE;
				break;
			default:
				rc = OMRSH_SYSV_REGULAR_CONTROL_FILE;
				break;
		}

	} else if (currentVersion >= C250VMversion) {
		switch(genVersion) {
			case 1:
			case 2:
			case 3:
				rc = OMRSH_SYSV_OLDER_CONTROL_FILE;
				break;
			default:
				rc = OMRSH_SYSV_OLDER_EMPTY_CONTROL_FILE;
				break;
		}

	} else if (currentVersion >= C240VMversion) {
		switch(genVersion) {
			case 1:
			case 2:
			case 3:
				rc = OMRSH_SYSV_OLDER_CONTROL_FILE;
				break;
			case 4:
			case 5:
			case 6:
			case 7:
				rc = OMRSH_SYSV_OLDER_EMPTY_CONTROL_FILE;
				break;
			default:
				rc = OMRSH_SYSV_OLDER_CONTROL_FILE;
				break;
		}

	} else if (currentVersion >= C230VMversion) {
		rc = OMRSH_SYSV_OLDER_CONTROL_FILE;
	} else {
		Trc_SHR_Assert_ShouldNeverHappen();
	}


	Trc_SHR_OSC_Sysv_SysVCacheFileTypeHelper_Event(currentVersion, rc);
	return rc;
}

/**
 * This method detects whether the given control file is non-readable, or read-only.
 *
 * @param [in] cacheDirName directory containing cache control file
 * @param [in] filename "semaphore" or "memory" depending on type of control file
 * @param [out]	isNotReadable true if the file cannot be read, false otherwise
 * @param [out] isReadOnly true if the file is read-only, false otherwise
 *
 * @return -1 if it fails to get file stats, 0 otherwise.
 */
I_32
SH_OSCachesysv::getControlFilePerm(char *cacheDirName, char *filename, bool *isNotReadable, bool *isReadOnly)
{
	char baseFile[OMRSH_MAXPATH];
	struct J9FileStat statbuf;
	I_32 rc;
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

	omrstr_printf(baseFile, OMRSH_MAXPATH, "%s%s", cacheDirName, filename);
	rc = omrfile_stat(baseFile, 0, &statbuf);
	if (0 == rc) {
		UDATA euid = omrsysinfo_get_euid();
		if (euid == statbuf.ownerUid) {
			if (0 == statbuf.perm.isUserReadable) {
				*isNotReadable = true;
				*isReadOnly = false;
			} else {
				*isNotReadable = false;
				if (0 == statbuf.perm.isUserWriteable) {
					*isReadOnly = true;
				} else {
					*isReadOnly = false;
				}
			}
		} else {
			if (0 == statbuf.perm.isGroupReadable) {
				*isNotReadable = true;
				*isReadOnly = false;
			} else {
				*isNotReadable = false;
				if (0 == statbuf.perm.isGroupWriteable) {
					*isReadOnly = true;
				} else {
					*isReadOnly = false;
				}
			}
		}
	}
	return rc;
}

#endif

void *
SH_OSCachesysv::getAttachedMemory()
{
	/* This method should only be called between calls to attach and detach
	 */
	return _dataStart;
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
IDATA
SH_OSCachesysv::restoreFromSnapshot(OMR_VM* vm, const char* cacheName, UDATA numLocks, SH_OSCache::SH_OSCacheInitializer* i, bool* cacheExist)
{
	OMRPORT_ACCESS_FROM_VMC(vm);
	IDATA rc = 0;
	char cacheDirName[OMRSH_MAXPATH];		/* OMRSH_MAXPATH defined to be EsMaxPath which is 1024 */
	char nameWithVGen[CACHE_ROOT_MAXLEN];	/* CACHE_ROOT_MAXLEN defined to be 88 */
	char pathFileName[OMRSH_MAXPATH];
	J9PortShcVersion versionData;
	IDATA fd = 0;
	const char* ctrlDirName = vm->sharedClassConfig->ctrlDirName;

	Trc_SHR_OSC_Sysv_restoreFromSnapshot_Entry();

	_verboseFlags = vm->sharedClassConfig->verboseFlags;
//	setCurrentCacheVersion(vm, J2SE_VERSION(vm), &versionData);
	versionData.cacheType = OMRPORT_SHR_CACHE_TYPE_SNAPSHOT;

	if (-1 == SH_OSCache::getCacheDir(vm, ctrlDirName, cacheDirName, OMRSH_MAXPATH, OMRPORT_SHR_CACHE_TYPE_SNAPSHOT)) {
		Trc_SHR_OSC_Sysv_restoreFromSnapshot_getCacheDirFailed();
		OSC_ERR_TRACE(J9NLS_SHRC_GETSNAPSHOTDIR_FAILED);
		rc = -1;
		goto done;
	}

	SH_OSCache::getCacheVersionAndGen(OMRPORTLIB, vm, nameWithVGen, CACHE_ROOT_MAXLEN, cacheName, &versionData, OSCACHE_CURRENT_CACHE_GEN, false);
	/* No check for the return value of getCachePathName() as it always returns 0 */
	SH_OSCache::getCachePathName(OMRPORTLIB, cacheDirName, pathFileName, OMRSH_MAXPATH, nameWithVGen);

	fd = omrfile_open(pathFileName, EsOpenRead | EsOpenWrite, 0);
	if (-1 == fd) {
		I_32 errorno = omrerror_last_error_number();

		if (OMRPORT_ERROR_FILE_NOENT == errorno) {
			Trc_SHR_OSC_Sysv_restoreFromSnapshot_fileNotFound(pathFileName);
			OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_ERROR_SNAPSHOT_FILE_NOT_FOUND, pathFileName);
		} else {
			const char * errormsg = omrerror_last_error_message();

			Trc_SHR_OSC_Sysv_restoreFromSnapshot_fileOpenFailed(pathFileName);
			OSC_ERR_TRACE1(J9NLS_SHRC_PORT_ERROR_NUMBER, errorno);
			Trc_SHR_Assert_True(errormsg != NULL);
			OSC_ERR_TRACE1(J9NLS_SHRC_PORT_ERROR_MESSAGE, errormsg);
			OSC_ERR_TRACE1(J9NLS_SHRC_ERROR_SNAPSHOT_FILE_OPEN, pathFileName);
		}
		rc = -1;
	} else {
		I_64 fileSize = omrfile_flength(fd);
		LastErrorInfo lastErrorInfo;
		I_32 openMode = 0;
		SH_CacheFileAccess cacheFileAccess = J9SH_CACHE_FILE_ACCESS_ALLOWED;

		if (OMR_ARE_ALL_BITS_SET(vm->sharedClassConfig->runtimeFlags, OMRSHR_RUNTIMEFLAG_ENABLE_GROUP_ACCESS)) {
			openMode |= J9OSCACHE_OPEN_MODE_GROUPACCESS;
			_groupPerm = 1;
		} else {
			_groupPerm = 0;
		}

		cacheFileAccess = SH_OSCacheFile::checkCacheFileAccess(OMRPORTLIB, fd, openMode, &lastErrorInfo);

		if (J9SH_CACHE_FILE_ACCESS_ALLOWED != cacheFileAccess) {
			switch (cacheFileAccess) {
			case J9SH_CACHE_FILE_ACCESS_GROUP_ACCESS_REQUIRED:
				OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_SNAPSHOT_GROUPACCESS_REQUIRED, pathFileName);
				break;
			case J9SH_CACHE_FILE_ACCESS_OTHERS_NOT_ALLOWED:
				OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_SNAPSHOT_OTHERS_ACCESS_NOT_ALLOWED, pathFileName);
				break;
			case J9SH_CACHE_FILE_ACCESS_CANNOT_BE_DETERMINED:
				printErrorMessage(&lastErrorInfo);
				OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_SNAPSHOT_INTERNAL_ERROR_CHECKING_CACHEFILE_ACCESS, pathFileName);
				break;
			default:
				Trc_SHR_Assert_ShouldNeverHappen();
			}
			omrfile_close(fd);
			rc = -1;
			Trc_SHR_OSC_Sysv_restoreFromSnapshot_fileAccessNotAllowed(pathFileName);
			goto done;
		}

		if ((fileSize < MIN_CC_SIZE) || (fileSize > MAX_CC_SIZE)) {
			Trc_SHR_OSC_Sysv_restoreFromSnapshot_fileSizeInvalid(pathFileName, fileSize);
			OSC_ERR_TRACE4(J9NLS_SHRC_OSCACHE_ERROR_SNAPSHOT_FILE_LENGTH, pathFileName, fileSize, MIN_CC_SIZE, MAX_CC_SIZE);
			rc = -1;
			/* lock the file to prevent reading and writing */
		} else if (omrfile_lock_bytes(fd, OMRPORT_FILE_WRITE_LOCK | OMRPORT_FILE_WAIT_FOR_LOCK, 0, fileSize) < 0) {
			I_32 errorno = omrerror_last_error_number();
			const char * errormsg = omrerror_last_error_message();

			Trc_SHR_OSC_Sysv_restoreFromSnapshot_fileLockFailed(pathFileName);
			OSC_ERR_TRACE1(J9NLS_SHRC_PORT_ERROR_NUMBER, errorno);
			Trc_SHR_Assert_True(errormsg != NULL);
			OSC_ERR_TRACE1(J9NLS_SHRC_PORT_ERROR_MESSAGE, errormsg);
			OSC_ERR_TRACE1(J9NLS_SHRC_ERROR_SNAPSHOT_FILE_LOCK, pathFileName);
			rc = -1;
		} else {
			OMRSharedCachePreinitConfig* piconfig = vm->sharedCachePreinitConfig;
			OMR_VMThread* currentThread = omr_vmthread_getCurrent(vm); //vm->internalVMFunctions->currentVMThread(vm);
			bool rcStartup = false;

			piconfig->sharedClassCacheSize = (UDATA)fileSize;
			versionData.cacheType = OMRPORT_SHR_CACHE_TYPE_NONPERSISTENT;
			SH_OSCache::getCacheVersionAndGen(OMRPORTLIB, vm, nameWithVGen, CACHE_ROOT_MAXLEN, cacheName, &versionData, OSCACHE_CURRENT_CACHE_GEN, true);
			if (1 == SH_OSCache::statCache(OMRPORTLIB, cacheDirName, nameWithVGen, false)) {
#if !defined(WIN32)
				OMRPortShmemStatistic statbuf;
				/* The shared memory may be removed without deleting the control files. So check the existence of the shared memory */
				IDATA ret = StatSysVMemoryHelper(OMRPORTLIB, cacheDirName, _groupPerm, nameWithVGen, &statbuf);

				if (0 == ret) {
#endif /* !defined(WIN32) */
					Trc_SHR_OSC_Sysv_restoreFromSnapshot_cacheExist1(currentThread);
					OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_ERROR_RESTORE_EXISTING_CACHE, cacheName);
					*cacheExist = true;
					omrfile_close(fd);
					rc = -1;
					goto done;
#if !defined(WIN32)
				}
#endif /* !defined(WIN32) */
			}

			rcStartup = startup(vm, ctrlDirName, vm->sharedCacheAPI->cacheDirPerm, cacheName, piconfig, numLocks, OMRSH_OSCACHE_CREATE, vm->sharedClassConfig->verboseFlags,
					vm->sharedClassConfig->runtimeFlags, openMode, vm->sharedCacheAPI->storageKeyTesting, &versionData, i, SHR_STARTUP_REASON_NORMAL);

			if (false == rcStartup) {
				Trc_SHR_OSC_Sysv_restoreFromSnapshot_cacheStartupFailed1(currentThread);
				OSC_ERR_TRACE(J9NLS_SHRC_OSCACHE_ERROR_STARTUP_CACHE);
				destroy(false);
				rc = -1;
			} else if (OMRSH_OSCACHE_CREATED != getError()) {
				/* Another VM has created the cache */
				OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_ERROR_RESTORE_EXISTING_CACHE, cacheName);
				Trc_SHR_OSC_Sysv_restoreFromSnapshot_cacheExist2(currentThread);
				*cacheExist = true;
				rc = -1;
			} else {
				SH_CacheMap* cm = (SH_CacheMap *)vm->sharedClassConfig->sharedClassCache;
				bool cacheHasIntegrity = false;
				I_32 semid = 0;
				U_16 theVMCntr = 0;
				OSCachesysv_header_version_current*  osCacheSysvHeader = NULL;
				OMRSharedCacheHeader* theca = (OMRSharedCacheHeader *)attach(currentThread, &versionData);
				IDATA nbytes = (IDATA)fileSize;
				IDATA fileRc = 0;

				if (NULL == theca) {
					Trc_SHR_OSC_Sysv_restoreFromSnapshot_cacheAttachFailed(currentThread);
					OSC_ERR_TRACE(J9NLS_SHRC_OSCACHE_SHMEM_ATTACH);
					destroy(false);
					omrfile_close(fd);
					rc = -1;
					goto done;
				}

				osCacheSysvHeader = (OSCachesysv_header_version_current *)(_headerStart);
				semid = osCacheSysvHeader->attachedSemid;
				theVMCntr = theca->vmCntr;

				Trc_SHR_Assert_Equals(theVMCntr, 0);

				fileRc = omrfile_read(fd, osCacheSysvHeader, nbytes);
				if (fileRc < 0) {
					I_32 errorno = omrerror_last_error_number();
					const char * errormsg = omrerror_last_error_message();

					Trc_SHR_OSC_Sysv_restoreFromSnapshot_fileReadFailed1(currentThread, pathFileName);
					OSC_ERR_TRACE1(J9NLS_SHRC_PORT_ERROR_NUMBER, errorno);
					Trc_SHR_Assert_True(errormsg != NULL);
					OSC_ERR_TRACE1(J9NLS_SHRC_PORT_ERROR_MESSAGE, errormsg);
					OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_ERROR_SNAPSHOT_FILE_READ, pathFileName);
					destroy(false);
					omrfile_close(fd);
					rc = -1;
					goto done;
				} else if (nbytes != fileRc) {
					Trc_SHR_OSC_Sysv_restoreFromSnapshot_fileReadFailed2(currentThread, pathFileName, nbytes, fileRc);
					OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_ERROR_SNAPSHOT_FILE_READ, pathFileName);
					destroy(false);
					omrfile_close(fd);
					rc = -1;
					goto done;
				}
				theca->vmCntr = theVMCntr;
				osCacheSysvHeader->attachedSemid = semid;
				/* remove OMRSHR_RUNTIMEFLAG_RESTORE and startup the cache again to check for corruption, cache header will be checked in SH_CacheMap::startup() */
				vm->sharedClassConfig->runtimeFlags &= ~OMRSHR_RUNTIMEFLAG_RESTORE;
				vm->sharedClassConfig->runtimeFlags |= OMRSHR_RUNTIMEFLAG_RESTORE_CHECK;
				/* free memory allocated by SH_OSCachesysv::startup() */
				cleanup();
				rc = cm->startup(currentThread, piconfig, cacheName, ctrlDirName, vm->sharedCacheAPI->cacheDirPerm, NULL, &cacheHasIntegrity);
				/* verboseFlags might be set to 0 in cm->startup(), set it again to ensure the NLS can be printed out */
				_verboseFlags = vm->sharedClassConfig->verboseFlags;
				if (0 == rc) {
					IDATA ret = 0;
					LastErrorInfo lastErrorInfo;

					/* Header mutex is acquired and not released in the first call of SH_OSCachesysv::startup(), release here */
					ret = exitHeaderMutex(&lastErrorInfo);
					if (0 == ret) {
						/* set osCacheSysvHeader to current _headerStart as it is detached in cleanup() and re-attached in cm->startup() */
						osCacheSysvHeader = (OSCachesysv_header_version_current *)_headerStart;
						/* To prevent the cache being opened by another JVM in read-only mode, osCacheSysvHeader->oscHdr.cacheInitComplete is always 0
						 * before the restoring operation is finished. Set it to 1 here
						 */
						osCacheSysvHeader->oscHdr.cacheInitComplete = 1;
					} else {
						Trc_SHR_OSC_Sysv_restoreFromSnapshot_headerMutexReleaseFailed(currentThread);
						errorHandler(J9NLS_SHRC_OSCACHE_ERROR_EXIT_HDR_MUTEX, &lastErrorInfo);
						cm->destroy(currentThread);
						rc = -1;
					}
				} else {
					/* if the restored cache is corrupted, it is destroyed in SH_CacheMap::startup() */
					Trc_SHR_OSC_Sysv_restoreFromSnapshot_cacheStartupFailed2(currentThread);
				}
			}
		}
		/* file lock will be released when closed */
		omrfile_close(fd);
	}
done:
	Trc_SHR_OSC_Sysv_restoreFromSnapshot_Exit(rc);
	return rc;
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
SH_OSCachesysv::verifySemaphoreGroupAccess(LastErrorInfo *lastErrorInfo)
{
	I_32 rc = 1;
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
	OMRPortShsemStatistic statBuf;

	memset(&statBuf, 0, sizeof(statBuf));
	if (OMRPORT_INFO_SHSEM_STAT_PASSED != omrshsem_deprecated_handle_stat(_semhandle, &statBuf)) {
		if (NULL != lastErrorInfo) {
			lastErrorInfo->lastErrorCode = omrerror_last_error_number();
			lastErrorInfo->lastErrorMsg = omrerror_last_error_message();
		}
		rc = -1;
	} else {
		if ((1 != statBuf.perm.isGroupWriteable)
			|| (1 != statBuf.perm.isGroupReadable)
		) {
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
SH_OSCachesysv::verifySharedMemoryGroupAccess(LastErrorInfo *lastErrorInfo)
{
	I_32 rc = 1;
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
	OMRPortShmemStatistic statBuf;

	memset(&statBuf, 0, sizeof(statBuf));
	if (OMRPORT_INFO_SHMEM_STAT_PASSED != omrshmem_handle_stat(_shmhandle, &statBuf)) {
		if (NULL != lastErrorInfo) {
			lastErrorInfo->lastErrorCode = omrerror_last_error_number();
			lastErrorInfo->lastErrorMsg = omrerror_last_error_message();
		}
		rc = -1;
	} else {
		if ((1 != statBuf.perm.isGroupWriteable)
			|| (1 != statBuf.perm.isGroupReadable)
		) {
			rc = 0;
		}
	}
	return rc;
}
#endif /* !defined(WIN32) */
