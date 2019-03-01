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
