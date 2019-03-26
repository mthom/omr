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
#if !defined(OS_SHARED_MEMORY_CACHE_CONFIG_HPP_INCLUDED)
#define OS_SHARED_MEMORY_CACHE_CONFIG_HPP_INCLUDED

#include "omr.h"
#include "omrport.h"
#include "pool_api.h"
#include "sharedconsts.h"

#include "OSCacheConfig.hpp"
#include "OSSharedMemoryCacheLayout.hpp"
#include "OSSharedMemoryCacheHeader.hpp"
// #include "OSSharedMemoryCacheAttachingContext.hpp"
// #include "OSSharedMemoryCacheCreatingContext.hpp"

#define OS_SHARED_MEMORY_CACHE_RESTART 4
#define OS_SHARED_MEMORY_CACHE_OPENED 3
#define OS_SHARED_MEMORY_CACHE_CREATED 2
#define OS_SHARED_MEMORY_CACHE_EXIST   1
#define OS_SHARED_MEMORY_CACHE_NOT_EXIST 0
#define OS_SHARED_MEMORY_CACHE_FAILURE -1

#define OS_SHARED_MEMORY_CACHE_SUCCESS 0

/**
 * This enum contains constants that are used to indicate the reason for not allowing access to the semaphore set.
 * It is returned by @ref SH_OSCachesysv::checkSemaphoreAccess().
 */
typedef enum SH_SysvSemAccess {
	OMRSH_SEM_ACCESS_ALLOWED 				= 0,
	OMRSH_SEM_ACCESS_CANNOT_BE_DETERMINED,
	OMRSH_SEM_ACCESS_OWNER_NOT_CREATOR,
	OMRSH_SEM_ACCESS_GROUP_ACCESS_REQUIRED,
	OMRSH_SEM_ACCESS_OTHERS_NOT_ALLOWED
} SH_SysvSemAccess;

/**
 * This enum contains constants that are used to indicate the reason for not allowing access to the shared memory.
 * It is returned by @ref SH_OSCachesysv::checkSharedMemoryAccess().
 */
typedef enum SH_SysvShmAccess {
	OMRSH_SHM_ACCESS_ALLOWED 						= 0,
	OMRSH_SHM_ACCESS_CANNOT_BE_DETERMINED,
	OMRSH_SHM_ACCESS_OWNER_NOT_CREATOR,
	OMRSH_SHM_ACCESS_GROUP_ACCESS_REQUIRED,
	OMRSH_SHM_ACCESS_GROUP_ACCESS_READONLY_REQUIRED,
	OMRSH_SHM_ACCESS_OTHERS_NOT_ALLOWED
} SH_SysvShmAccess;

#define SEM_HEADERLOCK 0

#define OMRSH_OSCACHE_RETRYMAX 30

// #define SHM_CACHEHEADERSIZE SHC_PAD(sizeof(OSCachesysv_header_version_current), SHC_WORDALIGN)
// #define SHM_CACHEDATASIZE(size) (size-SHM_CACHEHEADERSIZE)
// #define SHM_DATASTARTFROMHEADER(header) SRP_GET(header->oscHdr.dataStart, void*);

class OSSharedMemoryCacheConfig : public OSCacheConfig<OSSharedMemoryCacheLayout>
{
public:
  OSSharedMemoryCacheConfig(U_32 numLocks);
  OSSharedMemoryCacheConfig();

  virtual IDATA getWriteLockID(void);
  virtual IDATA getReadWriteLockID(void);

  virtual IDATA acquireLock(OMRPortLibrary* library, UDATA lockID, OSCacheConfigOptions* configOptions,
			    LastErrorInfo* lastErrorInfo = NULL);
  virtual IDATA releaseLock(OMRPortLibrary* library, UDATA lockID);

  // let these be decided by the classes of the generational header
  // versions. They will know where the locks lie and how large they
  // are.
  virtual U_64 getLockOffset(UDATA lockID) = 0;
  virtual U_64 getLockSize(UDATA lockID) = 0;

  virtual U_64* getHeaderLocation() = 0;
  virtual U_64* getHeaderSize() = 0;

  // this is _dataStart, wherever that ultimately ends up.
  virtual U_64* getDataSectionLocation() = 0;
  virtual U_32 getDataSectionLength() = 0;

  virtual void setHeaderLocation(void* location) = 0;
  virtual void setDataSectionLocation(void* location) = 0;

protected:
  friend class OSSharedMemoryCache;
  friend class OSSharedMemoryCachePolicies;
  friend class OSSharedMemoryCacheStats;

  IDATA acquireHeaderWriteLock(OMRPortLibrary* library, const char* cacheName, LastErrorInfo* lastErrorInfo);
  IDATA releaseHeaderWriteLock(OMRPortLibrary* library, LastErrorInfo* lastErrorInfo);

  virtual IDATA initializeHeader(const char* cacheDirName, LastErrorInfo* lastErrorInfo) = 0;

  IDATA _numLocks;
  OSSharedMemoryCacheHeader* _header;

  SH_SysvSemAccess _semAccess;
  SH_SysvShmAccess _shmAccess;

  omrshmem_handle* _shmhandle;
  omrshsem_handle* _semhandle;

  UDATA _totalNumSems;
  I_32 _semid;
};

#endif
