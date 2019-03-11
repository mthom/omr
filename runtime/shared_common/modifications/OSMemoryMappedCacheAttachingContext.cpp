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

#include "OSMemoryMappedCacheConfigOptions.hpp"
#include "OSMemoryMappedCacheAttachingContext.hpp"
#include "OSMemoryMappedCache.hpp"

// TODO: move constructAttachingContext and constructCreatingContext to OSMemoryMappedCache.
// since the Context classes own their own cache ptr.

IDATA OSMemoryMappedCacheAttachingContext::initAttach()
{
  IDATA rc = OMRSH_OSCACHE_SUCCESS;

  J9SRP* dataStartField;
  U_32* dataLengthField;

  if ((dataLengthField = _config->getDataLengthFieldLocation())) {
    _cache->_config->_layout->_dataLength = *dataLengthField;
  }

  if ((dataStartField = _config->getDataSectionLocation())) {
    _cache->_config->_layout->_dataStart = *dataStartField; //whaat?: MMAP_DATASTARTFROMHEADER(dataStartField);
  }

  if (NULL == *dataStartField) {
    Trc_SHR_OSC_Mmap_internalAttach_corruptcachefile();
    OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_CORRUPT_CACHE_DATA_START_NULL, *dataStartField);

    _cache->setCorruptionContext(CACHE_DATA_NULL, (UDATA)*dataStartField);
    rc = OMRSH_OSCACHE_CORRUPT;
  }

  return rc;
}

// we probably also need a OSMemoryMappedStartup class, since the method is so huge,
// to encapsulate all that state and share it between objects of other classes, ie.
// this one.

/*
 * here's an inventory of the state contained in here:
 *
 * errorCode    (reference!)
 * retryCntr    (should be a reference)
 * cacheOptions (OSMemoryMappedCacheConfigOptions, a copy! It's immutable)
 * cache
 * initCompleteAddr (the address of the initComplete flag, which comes from the header, I think)
 *
 * Return a boolean: false if _errorPostAttach is reached, true otherwise.
 */
bool
OSMemoryMappedCacheAttachingContext::startup(IDATA& errorCode, OSCacheConfigOptions& configOptions)
{
  IDATA retryCntr = 0;
  IDATA rc;
  /* We are opening an existing cache */
  // Trc_SHR_OSC_Mmap_startup_fileOpened();

  /* no need for this check with the proper configuration. at least.. I don't think. Since we're attaching,
     there should be some sort of version check.

  if (_cacheSize <= sizeof(OSCachemmap_header_version_current)) {
    Trc_SHR_OSC_Mmap_startup_cacheTooSmall();
    errorCode = OMRSH_OSCACHE_CORRUPT;
    OSC_ERR_TRACE1(J9NLS_SHRC_CC_STARTUP_CORRUPT_CACHE_SIZE_INVALID, _cacheSize);
    setCorruptionContext(CACHE_SIZE_INVALID, (UDATA)_cacheSize);
    goto _errorPostHeaderLock;
  }
  */

  /* At this point, don't check the cache version - we need to attach to older versions in order to destroy */
  rc = _cache->internalAttach(); //false, _activeGeneration);
  
  if (0 != rc) {
    errorCode = rc;
    // Trc_SHR_OSC_Mmap_startup_badAttach();
    //goto _errorPostAttach;
    return false;
  }

  if (cache->_config->_runningReadOnly)
  {
    retryCntr = 0;
    U_32* initCompleteAddr = (U_32*)cache->_config->getInitCompleteLocation();

    /* In readonly, we can't get a header lock, so if the cache is
       mid-init, give it a chance to complete initialization */    
    while ((!*initCompleteAddr) && (retryCntr < OMRSH_OSCACHE_READONLY_RETRY_COUNT)) {
      omrthread_sleep(OMRSH_OSCACHE_READONLY_RETRY_SLEEP_MILLIS);
      ++retryCntr;
    }

    if (!*initCompleteAddr) {
      cache->errorHandler(J9NLS_SHRC_OSCACHE_MMAP_STARTUP_ERROR_READONLY_CACHE_NOTINITIALIZED, NULL);
      Trc_SHR_OSC_Mmap_startup_cacheNotInitialized();
      //goto _errorPostAttach;
      return false;
    }
  }

  if (configOptions.verboseEnabled()) { // _verboseFlags & OMRSHR_VERBOSEFLAG_ENABLE_VERBOSE) {
    if (_config->_runningReadOnly) {
      OSC_TRACE1(OMRNLS_SHRC_OSCACHE_MMAP_STARTUP_OPENED_READONLY, cache->_cacheName);
    } else {
      OSC_TRACE1(J9NLS_SHRC_OSCACHE_MMAP_STARTUP_OPENED, _cacheName);
    }
  }

  return true;
}

bool OSMemoryMappedCacheAttachingContext::creatingNewCache() {
  return false;
}
