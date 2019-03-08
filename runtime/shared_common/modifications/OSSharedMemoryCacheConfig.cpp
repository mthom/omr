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

#include "OSSharedMemoryCacheConfig.hpp"

IDATA
OSSharedMemoryCacheConfig::acquireHeaderWriteLock(OMRPortLibrary* library, const char* cacheName, LastErrorInfo *lastErrorInfo)
{
  PORT_ACCESS_FROM_PORT(_portLibrary);
  IDATA rc = 0;
  Trc_SHR_OSC_GlobalLock_getMutex(cacheName);
  
  if (NULL != lastErrorInfo) {
    lastErrorInfo->lastErrorCode = 0;
  }
  
  if (_semhandle != NULL) {
    rc = j9shsem_deprecated_wait(_semhandle, SEM_HEADERLOCK, J9PORT_SHSEM_MODE_UNDO);
    if (-1 == rc) {
      if (NULL != lastErrorInfo) {
	lastErrorInfo->populate(OMRPORTLIB);
//	lastErrorInfo->lastErrorCode = j9error_last_error_number();
//	lastErrorInfo->lastErrorMsg = j9error_last_error_message();
      }
    }
  }
  Trc_SHR_OSC_GlobalLock_gotMutex(cacheName);
  return rc;
}

IDATA OSSharedMemoryCacheConfig::releaseHeaderWriteLock(OMRPortLibrary* library, LastErrorInfo* lastErrorInfo);
{	
  OMRPORT_ACCESS_FROM_OMRPORT(library);
  IDATA rc = 0;
  if (NULL != lastErrorInfo) {
    lastErrorInfo->lastErrorCode = 0;
  }

  if (_semhandle != NULL) {
    rc = j9shsem_deprecated_post(_semhandle, SEM_HEADERLOCK, J9PORT_SHSEM_MODE_UNDO);
    if (-1 == rc) {
      if (NULL != lastErrorInfo) {
	lastErrorInfo->populate(OMRPORTLIB);
//	lastErrorInfo->lastErrorCode = j9error_last_error_number();
//	lastErrorInfo->lastErrorMsg = j9error_last_error_message();
      }
    }
  }
  
  Trc_SHR_OSC_GlobalLock_released();
  return rc;
}
