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

#if !defined(OS_SHARED_MEMORY_CACHE_HPP_INCLUDED)
#define OS_SHARED_MEMORY_CACHE_HPP_INCLUDED

#include "OSCacheImpl.hpp"
#include "OSSharedMemoryCacheIterator.hpp"
#include "OSSharedMemoryCacheConfig.hpp"
#include "OSSharedMemoryCachePolicies.hpp"
#include "OSSharedMemoryCacheSnapshot.hpp"
#include "OSCacheContiguousRegion.hpp"

#include "omr.h"
#include "omrport.h"

#define OS_SHARED_MEMORY_CACHE_RESTART 4
#define OS_SHARED_MEMORY_CACHE_OPENED 3
#define OS_SHARED_MEMORY_CACHE_CREATED 2
#define OS_SHARED_MEMORY_CACHE_EXIST   1
#define OS_SHARED_MEMORY_CACHE_NOT_EXIST 0
#define OS_SHARED_MEMORY_CACHE_FAILURE -1

#define OS_SHARED_MEMORY_CACHE_SUCCESS 0

#define SEM_HEADERLOCK 0

#define OMRSH_OSCACHE_RETRYMAX 30

//#define SHM_CACHEHEADERSIZE SHC_PAD(sizeof(OSCachesysv_header_version_current), SHC_WORDALIGN)
//#define SHM_CACHEDATASIZE(size) (size-SHM_CACHEHEADERSIZE)
//#define SHM_DATASTARTFROMHEADER(header) SRP_GET(header->oscHdr.dataStart, void*);

class OSSharedMemoryCache: public OSCacheImpl
{
public:
  OSSharedMemoryCache(OMRPortLibrary* library, const char* cacheName, const char* cacheDirName,
		      IDATA numLocks, OSCacheConfigOptions* configOptions);

  bool startup(const char* cacheName, const char* ctrlDirName);
  IDATA destroy(bool suppressVerbose, bool isReset = false);

  virtual void initialize();
  virtual void cleanup();

  SH_CacheAccess isCacheAccessible() const;
  UDATA isCacheActive() const;
  //  IDATA restoreFromSnapshot(const char* cacheName, UDATA numLocks, bool& cacheExists);

protected:
  friend class OSSharedMemoryCachePolicies;
  friend class OSSharedMemoryCacheSnapshot;
  friend class OSSharedMemoryCacheStats;
  
  virtual OSSharedMemoryCacheIterator* getSharedMemoryCacheIterator() = 0;

  void setError(IDATA ec);
  IDATA openCache(const char* cacheName);

  // factory method to construct a policy object that decides how the
  // interface to shared memory *and* semaphores is handled.
  virtual OSSharedMemoryCachePolicies* constructSharedMemoryPolicy();

  // factory method to construct a snapshot object. it's pure because OSSharedMemoryCacheSnapshot
  // class is abstract.
  virtual OSSharedMemoryCacheSnapshot* constructSharedMemoryCacheSnapshot() = 0;

  virtual OSCacheMemoryProtector* constructMemoryProtector();
  
  virtual void errorHandler(U_32 moduleName, U_32 id, LastErrorInfo *lastErrorInfo);
  virtual void printErrorMessage(LastErrorInfo* lastErrorInfo);
    
  virtual UDATA getPermissionsRegionGranularity();

  // this is largely J9 specific. let it be overloaded.
  virtual IDATA verifyCacheHeader() = 0;

  IDATA restoreFromSnapshot(IDATA numLocks);
  
  IDATA detachRegion();

  void* attach();
  IDATA detach();

  OSSharedMemoryCacheConfig* _config;
  OMRControlFileStatus _controlFileStatus;

  IDATA _attachCount; // was _attach_count, but then why were we mixing case styles?
  UDATA _userSemCntr;

  char* _shmFileName;
  char* _semFileName;

  bool _startupCompleted;
  bool _openSharedMemory;

  OSSharedMemoryCachePolicies* _policies;  
};
#endif
