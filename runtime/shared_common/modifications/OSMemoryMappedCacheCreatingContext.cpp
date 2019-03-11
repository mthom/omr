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

#include "OSMemoryMappedCacheCreatingContext.hpp"

IDATA OSMemoryMappedCacheCreatingContext::initAttach()
{
  _cache->_config->_layout->_dataLength = _cache->_config->getDataLength();
  _cache->_config->_layout->_dataStart  = _cache->_config->getHeaderLocation()
    + _cache->_config->getHeaderSize();

  return OMRSH_OSCACHE_SUCCESS;
}

bool OSMemoryMappedCacheCreatingContext::startup(IDATA& errorCode, OSCacheConfigOptions configOptions)
{
  // this is accessible through the Config object.
  //OSCachemmap_header_version_current *cacheHeader;
  IDATA rc;

  //creatingNewCache = true;

  /* File is wrong length, so we are creating the cache */
  Trc_SHR_OSC_Mmap_startup_fileCreated();

  /* We can't create the cache when we're running read-only */
  if (_cache->_config->_runningReadOnly) {
    Trc_SHR_OSC_Mmap_startup_runningReadOnlyAndWrongLength();
    errorHandler(OMRNLS_SHRC_OSCACHE_MMAP_STARTUP_ERROR_OPENING_CACHE_READONLY, NULL);
    //goto _errorPostHeaderLock;
    return false;
  }

  /* Set cache to the correct length */
  // if (!setCacheLength((U_32)piconfig->sharedClassCacheSize, &lastErrorInfo)) {
  if(_cache->_config->setCacheLength(&lastErrorInfo)) {
    // Trc_SHR_OSC_Mmap_startup_badSetCacheLength(piconfig->sharedClassCacheSize);
    errorHandler(OMRNLS_SHRC_OSCACHE_MMAP_STARTUP_ERROR_SETTING_CACHE_LENGTH, &lastErrorInfo);
    //goto _errorPostHeaderLock;
    return false;
  }

  // Trc_SHR_OSC_Mmap_startup_goodSetCacheLength(piconfig->sharedClassCacheSize);

  /* Verify if the group access has been set */
  if (configOptions.groupAccessEnabled()) { //OMR_ARE_ALL_BITS_SET(_openMode, J9OSCACHE_OPEN_MODE_GROUPACCESS)) {
    I_32 groupAccessRc = OSMemoryMappedCacheUtils::verifyCacheFileGroupAccess(_cache->_portLibrary,
									      _cache->_config->_fileHandle,
									      &lastErrorInfo);

    if (0 == groupAccessRc) {
      Trc_SHR_OSC_Mmap_startup_setGroupAccessFailed(_cache->_cachePathName);
      OSC_WARNING_TRACE(OMRNLS_SHRC_OSCACHE_MMAP_SET_GROUPACCESS_FAILED);
    } else if (-1 == groupAccessRc) {
      /* Failed to get stats of the cache file */
      Trc_SHR_OSC_Mmap_startup_badFileStat(_cachePathName);
      _cache->errorHandler(OMRNLS_SHRC_OSCACHE_ERROR_FILE_STAT, &lastErrorInfo);
      // goto _errorPostHeaderLock;
      return false;
    }
  }

  rc = _cache->internalAttach(); //true, _activeGeneration);
  if (0 != rc) {
    errorCode = rc;
    Trc_SHR_OSC_Mmap_startup_badAttach();
    // goto _errorPostAttach;
    return false;
  }

  //(OSCachemmap_header_version_current *)_headerStart;
  cacheHeader = _config->_layout->_headerStart;

  /* Create the cache header */
  if (!createCacheHeader(cacheHeader)) { //, versionData)) {
    Trc_SHR_OSC_Mmap_startup_badCreateCacheHeader();
    _cache->errorHandler(J9NLS_SHRC_OSCACHE_MMAP_STARTUP_ERROR_CREATING_CACHE_HEADER, NULL);
    // goto _errorPostAttach;
    return false;
  }

  Trc_SHR_OSC_Mmap_startup_goodCreateCacheHeader();

  /* TODO: move initializeDataHeader stuff to this class.
  if (initializer) {
    if (!initializeDataHeader(initializer)) {
      Trc_SHR_OSC_Mmap_startup_badInitializeDataHeader();
      errorHandler(J9NLS_SHRC_OSCACHE_MMAP_STARTUP_ERROR_INITIALISING_DATA_HEADER, NULL);
      goto _errorPostAttach;
    }
    Trc_SHR_OSC_Mmap_startup_goodInitializeDataHeader();
  }
  */

  if (configOptions.verboseEnabled()) { //_verboseFlags & OMRSHR_VERBOSEFLAG_ENABLE_VERBOSE) {
    OSC_TRACE1(OMRNLS_SHRC_OSCACHE_MMAP_STARTUP_CREATED, _cacheName);
  }
}

bool OSMemoryMappedCacheCreatingContext::creatingNewCache() {
  return true;
}

/**
 * Method to create the cache header for a new persistent cache
 *
 * @param [in] cacheHeader  A pointer to the cache header
 * @param [in] versionData  The version data of the cache
 *
 * @return true on success, false on failure
 * THREADING: Pre-req caller holds the cache header write lock
 */
I_32
OSMemoryMappedCacheCreatingContext::createCacheHeader()
{
  OMRPORT_ACCESS_FROM_OMRPORT(_cache->_portLibrary);
  U_32 headerLen = _cache->_config->getHeaderSize();  // MMAP_CACHEHEADERSIZE;
  OSMemoryMappedCacheHeader* cacheHeader = _cache->_config->_header;

  if(NULL = cacheHeader) {
    return false;
  }

  // commented out because we now lack versionData.
  // Trc_SHR_OSC_Mmap_createCacheHeader_Entry(cacheHeader, headerLen, versionData);

  memset(cacheHeader, 0, headerLen);
  strncpy(cacheHeader->eyecatcher, J9SH_OSCACHE_MMAP_EYECATCHER, J9SH_OSCACHE_MMAP_EYECATCHER_LENGTH);

  cacheHeader->createTime = omrtime_current_time_millis();
  cacheHeader->lastAttachedTime = omrtime_current_time_millis();
  cacheHeader->lastDetachedTime = omrtime_current_time_millis();

  cacheHeader->init(); //initOSCacheHeader(&(cacheHeader->oscHdr), versionData, headerLen);

// commented out because we now lack some of this data. sort of. some of it is in fact
// contained inside the layout object. It will probably be written to the header. who knows.

//  Trc_SHR_OSC_Mmap_createCacheHeader_header(cacheHeader->eyecatcher,
//					    cacheHeader->oscHdr.size,
//					    cacheHeader->oscHdr.dataStart,
//					    cacheHeader->oscHdr.dataLength,
//					    cacheHeader->createTime,
//					    cacheHeader->lastAttachedTime);

  Trc_SHR_OSC_Mmap_createCacheHeader_Exit();
  return true;
}
