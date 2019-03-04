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

#include "OSMemoryMappedCache.hpp"

OSMemoryMappedCache::OSMemoryMappedCache(OMRPortLibrary* library,
					 OMR_VM* vm,
					 const char* cacheDirName,
					 const char* cacheName,
					 IDATA numLocks,
					 OSMemoryMappedCacheConfigOptions configOptions,
					 I_32 openMode)
  : _config(OSMemoryMappedCacheConfig(numLocks))
  , _configOptions(configOptions)
{
  initialize(library);
  startup(openMode, ..);
}

void OSMemoryMappedCache::initialize(OMRPortLibrary* library)
{
  /* memForConstructor is used in J9 as a place to allocate the cache in. Now
  that we're in OMR, I don't what will take its place, if anything.
  Trc_SHR_OSC_Mmap_initialize_Entry(portLibrary, memForConstructor);
  */

  //TODO: set _portLibrary in commonInit.
  commonInit(library); //, generation);

  _config->_fileHandle = -1;
  _config->_actualFileLength = 0;
  _config->_finalised = 0;

  _mapFileHandle = NULL;

  for (UDATA i = 0; i < J9SH_OSCACHE_MMAP_LOCK_COUNT; i++) {
    _config->_lockMutex[i] = NULL;
  }

  //TODO: shouldn't this be in the OSCache constructor? Seems both universal and benign in
  //its effects.
  _corruptionCode = NO_CORRUPTION;
  _corruptValue = NO_CORRUPTION;

  _config->_cacheFileAccess = J9SH_CACHE_FILE_ACCESS_ALLOWED;
}

void OSMemoryMappedCache::finalise()
{
  //Trc_SHR_OSC_Mmap_finalise_Entry();

  commonCleanup();

  _config->_fileHandle = -1;
  _config->_actualFileLength = 0;
  _config->_finalised = 1;

  _mapFileHandle = NULL;

  for (UDATA i = 0; i < _config->_numLocks; i++) { // J9SH_OSCACHE_MMAP_LOCK_COUNT; i++) {
    if(NULL != _config->_lockMutex[i]) {
      omrthread_monitor_destroy(_config->_lockMutex[i]);
    }
  }

  //Trc_SHR_OSC_Mmap_finalise_Exit();
}

bool OSMemoryMappedCache::startup(I_32 openMode)
{
  I_32 mmapCapabilities = omrmmap_capabilities();
  LastErrorInfo lastErrorInfo;

  OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

  /*
  Trc_SHR_OSC_Mmap_startup_Entry(cacheName, ctrlDirName,
				 (piconfig!= NULL)? piconfig->sharedClassCacheSize : defaultCacheSize,
				 numLocks, createFlag, verboseFlags, openMode);
  */

  /* no longer need these, obviously:

  versionData->cacheType = OMRPORT_SHR_CACHE_TYPE_PERSISTENT;
  mmapCapabilities = omrmmap_capabilities();
  */

  // we need both writing and synchronization capabilities in order to
  // have a memory-mapped cache.
  if (!(mmapCapabilities & OMRPORT_MMAP_CAPABILITY_WRITE) || !(mmapCapabilities & OMRPORT_MMAP_CAPABILITY_MSYNC))
  {
    Trc_SHR_OSC_Mmap_startup_nommap(mmapCapabilities);
    errorHandler(J9NLS_SHRC_OSCACHE_MMAP_STARTUP_MMAPCAP, NULL);
    goto _errorPreFileOpen;
  }

  //TODO: add these parameters (ctrlDirName, cacheDirPermissions, openMode) to this function.
  if(initCacheDirName(ctrlDirName, cacheDirPermissions, openMode) != 0) {
    //Trc_SHR_OSC_Mmap_startup_commonStartupFailure();
    goto _errorPreFileOpen;
  }

  //TODO: overload this function, provide a cacheName parameter to this function.
  if(initCacheName(cacheName) != 0) {
    goto _errorPreFileOpen;
  }

  //Trc_SHR_OSC_Mmap_startup_commonStartupSuccess();
  /* Detect remote filesystem */
  if (openMode & J9OSCACHE_OPEN_MODE_CHECK_NETWORK_CACHE) {
    // _cacheLocation, or _cacheDirName as it was called, is initialized inside commonStartup.
    if (0 == omrfile_stat(_cacheLocation, 0, &statBuf)) {
      if (statBuf.isRemote) {
	//Trc_SHR_OSC_Mmap_startup_detectedNetworkCache();
	errorHandler(J9NLS_SHRC_OSCACHE_MMAP_NETWORK_CACHE, NULL);
	goto _errorPreFileOpen;
      }
    }
  }
}

/*
 * This function performs enough of an attach to start the cache, but nothing more
 * The internalDetach function is the equivalent for detach
 * isNewCache should be true if we're attaching to a completely uninitialized cache, false otherwise
 * THREADING: Pre-req caller holds the cache header write lock
 *
 * Needs to be able to work with all generations
 *
 * @param [in] isNewCache true if the cache is new and we should calculate cache size using the file size;
 * 				false if the cache is pre-existing and we can read the size fields from the cache header
 * @param [in] generation The generation of the cache header to use when calculating the lock offset
 *
 * @return 0 on success, OMRSH_OSCACHE_FAILURE on failure, OMRSH_OSCACHE_CORRUPT for corrupt cache
 */

/* additions: isNewCache is a question answered by the OSMemoryMappedInitializationContext object.
   Similar, the generation is determined by the concrete type of _header, inside _config.. so neither
   of these parameters is needed. */
IDATA OSMemoryMappedCache::internalAttach() //bool isNewCache, UDATA generation)
{
  OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
  U_32 accessFlags = _config->_runningReadOnly ? OMRPORT_MMAP_FLAG_READ : OMRPORT_MMAP_FLAG_WRITE;
  LastErrorInfo lastErrorInfo;
  IDATA rc = OMRSH_OSCACHE_FAILURE;

  Trc_SHR_OSC_Mmap_internalAttach_Entry();

  /* Get current length of file */
  accessFlags |= OMRPORT_MMAP_FLAG_SHARED;

  _config->_actualFileLength = _cacheSize;
  Trc_SHR_Assert_True(_config->_actualFileLength > 0);

  if (0 != _config->acquireAttachReadLock(&lastErrorInfo)) {
    Trc_SHR_OSC_Mmap_internalAttach_badAcquireAttachedReadLock();
    errorHandler(J9NLS_SHRC_OSCACHE_MMAP_STARTUP_ERROR_ACQUIRING_ATTACH_READ_LOCK, &lastErrorInfo);
    rc = OMRSH_OSCACHE_FAILURE;
    goto error;
  }

  Trc_SHR_OSC_Mmap_internalAttach_goodAcquireAttachReadLock();

#ifndef WIN32
  {
    OMRFileStatFilesystem fileStatFilesystem;
    /* check for free disk space */
    rc = omrfile_stat_filesystem(_cachePathName, 0, &fileStatFilesystem);
    if (0 == rc) {
      if (fileStatFilesystem.freeSizeBytes < (U_64)_actualFileLength) {
	OSC_ERR_TRACE2(OMRNLS_SHRC_OSCACHE_MMAP_DISK_FULL, (U_64)fileStatFilesystem.freeSizeBytes, (U_64)_actualFileLength);
	rc = OMRSH_OSCACHE_FAILURE;
	goto error;
      }
    }
  }
#endif

  /* Map the file */
  _mapFileHandle = omrmmap_map_file(_config->_fileHandle, 0, (UDATA)_actualFileLength, _cachePathName, accessFlags, OMRMEM_CATEGORY_CLASSES_SHC_CACHE);
  if ((NULL == _mapFileHandle) || (NULL == _mapFileHandle->pointer)) {
    lastErrorInfo.populate();
    /*
      lastErrorInfo.lastErrorCode = omrerror_last_error_number();
      lastErrorInfo.lastErrorMsg = omrerror_last_error_message();
    */
    Trc_SHR_OSC_Mmap_internalAttach_badmapfile();
    errorHandler(J9NLS_SHRC_OSCACHE_MMAP_ATTACH_ERROR_MAPPING_FILE, &lastErrorInfo);
    rc = OMRSH_OSCACHE_FAILURE;
    goto error;
  }

  _layout->_headerStart = _mapFileHandle->pointer;
  Trc_SHR_OSC_Mmap_internalAttach_goodmapfile(_layout->_headerStart);

  if(_init_context->internalAttach(this) == OMRSH_OSCACHE_CORRUPT) {
    goto error;
  }

  Trc_SHR_OSC_Mmap_internalAttach_Exit(_dataStart, sizeof(OSCachemmap_header_version_current));
  return 0;

 error:
  internalDetach();
  return rc;
}

/* Perform enough work to detach from the cache after having called internalAttach */
void OSMemoryMappedCache::internalDetach()
{
  OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

  Trc_SHR_OSC_Mmap_internalDetach_Entry();

  if (NULL == _config->_layout->_headerStart) {
    Trc_SHR_OSC_Mmap_internalDetach_notattached();
    return;
  }

  if (_mapFileHandle) {
    omrmmap_unmap_file(_mapFileHandle);
    _mapFileHandle = NULL;
  }

  if (0 != releaseAttachReadLock()) {
    Trc_SHR_OSC_Mmap_internalDetach_badReleaseAttachReadLock();
  }
  Trc_SHR_OSC_Mmap_internalDetach_goodReleaseAttachReadLock();

  _config->_layout->_headerStart = NULL;
  _config->_layout->_dataStart = NULL;
  _config->_layout->_dataLength = 0;
  /* The member variable '_actualFileLength' is not set to zero b/c
   * the cache size may be needed to reset the cache (e.g. in the
   * case of a build id mismatch, the cache may be reset, and
   * ::getTotalSize() may be called to ensure the new cache is the
   * same size).
   */

  Trc_SHR_OSC_Mmap_internalDetach_Exit(_config->_layout->_headerStart,
				       _config->_layout->_dataStart,
				       _config->_layout->_dataLength);
  return;
}
