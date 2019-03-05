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

#if !defined(OSCACHE_PERSISTENT_HPP_INCLUDED)
#define OSCACHE_PERSISTENT_HPP_INCLUDED

#include "OSCache.hpp"

#include "omr.h"

class OSPersistentCache: public OSCache {
public:
  // old J9 cache comment:
  /**
   * Advise the OS to release resources used by a section of the shared classes cache
   */
  virtual void dontNeedMetadata(const void* startAddress, size_t length);

protected:
  virtual IDATA initCacheDirName(const char* ctrlDirName, UDATA cacheDirPermissions, I_32 openMode);
  virtual IDATA initCacheName(const char* cacheName) = 0;
  virtual void errorHandler(U_32 moduleName, U_32 id, LastErrorInfo *lastErrorInfo);
  virtual IDATA commonStartup(OMR_VM* vm, const char* ctrlDirName, UDATA cacheDirPerm, const char* cacheName,
			      OSCacheConfigOptions* configOptions, I_32 openMode);

  I_32 _openMode; 
  // was:  char *_cacheDir; // the path to the directory containing the cache file.  
  char *_cacheLocation;  // the path, or a URI, or something, to the resource containing the cache.  
  char *_cacheName; // the name of the cache file. Together with _cacheDir, we have the effective field,
                    // _cachePathName.
  char *_cachePathName; // the _cacheLocation + _cacheName, typically.
};

#endif
