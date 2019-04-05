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
#if !defined(OS_MEMORY_MAPPED_CACHE_CONFIG_HPP_INCLUDED)
#define OS_MEMORY_MAPPED_CACHE_CONFIG_HPP_INCLUDED

#include "sharedconsts.h"

#include "OSCacheImpl.hpp"
#include "OSCacheConfig.hpp"

#include "OSMemoryMappedCacheAttachingContext.hpp"
#include "OSMemoryMappedCacheCreatingContext.hpp"
#include "OSMemoryMappedCacheHeader.hpp"

#define OMRSH_OSCACHE_MMAP_LOCK_COUNT 5
#define OMRSH_OSCACHE_MMAP_LOCKID_WRITELOCK 0
#define OMRSH_OSCACHE_MMAP_LOCKID_READWRITELOCK 1

#define RETRY_OBTAIN_WRITE_LOCK_SLEEP_NS 100000
#define RETRY_OBTAIN_WRITE_LOCK_MAX_MS 160
#define NANOSECS_PER_MILLISEC (I_64)1000000

class OSMemoryMappedCacheConfig : public OSCacheConfig
{
public:
  typedef OSMemoryMappedCacheHeader header_type;
  
  OSMemoryMappedCacheConfig(U_32 numLocks);
  OSMemoryMappedCacheConfig();

  virtual IDATA getWriteLockID();
  virtual IDATA getReadWriteLockID();

  virtual IDATA acquireLock(OMRPortLibrary* library, UDATA lockID, LastErrorInfo* lastErrorInfo = NULL);
  virtual IDATA releaseLock(OMRPortLibrary* library, UDATA lockID);
  
  // let these be decided by the classes of the generational header versions. They will
  // know where the locks lie and how large they are.
  virtual U_64 getLockOffset(UDATA lockID) = 0;
  virtual U_64 getLockSize(UDATA lockID) = 0;  
  virtual U_64 getHeaderLockOffset() = 0;

  virtual bool initHeader() = 0;
  
  virtual U_64* getHeaderLocation() = 0;
  virtual U_64 getHeaderSize() = 0;
  
  // these are _headerStart + _dataStart, wherever that ultimately ends up (in the header is the only
  // part of the answer we assume).
  virtual J9SRP* getDataSectionLocation() = 0;
  virtual U_64  getDataSectionSize() = 0;
  virtual U_32* getDataLengthFieldLocation() = 0;

  // what is the length of the data section in the header?
  virtual U_32 getDataLength(); 

  virtual U_64* getLastAttachTimeLocation() = 0;
  virtual U_64* getLastDetachTimeLocation() = 0;
  virtual U_64* getLastCreateTimeLocation() = 0;

  virtual U_64* getInitCompleteLocation() = 0;

  virtual bool setCacheLength(LastErrorInfo* lastErrorInfo) = 0;
  virtual bool setCacheInitComplete() = 0; // a header dependent thing. hence it's a pure virtual function.

  // replaces SH_OSCachemmap::isCacheHeaderValid(..).
  virtual bool isCacheHeaderValid() = 0;

  virtual bool updateLastAttachedTime(OMRPortLibrary* library, UDATA runningReadOnly);
  virtual bool updateLastDetachedTime(OMRPortLibrary* library, UDATA runningReadOnly);

  // this is a mediating method called once the mapping is established
  // in memory.  here, the baton is probably handed to the _layout
  // object to configure the OSCacheRegion objects.
  virtual void notifyRegionMappingStartAddress(void*) = 0;
  
protected:
  friend class OSMemoryMappedCache;
  friend class OSMemoryMappedCacheCreatingContext;
  friend class OSMemoryMappedCacheAttachingContext;
  
  // this function is meant to eliminate dangling pointers once the
  // memory mapped file is detached in detach().
  virtual void detachRegions() = 0;
  
  IDATA acquireHeaderWriteLock(OMRPortLibrary* library, UDATA runningReadOnly, LastErrorInfo* lastErrorInfo);
  IDATA releaseHeaderWriteLock(OMRPortLibrary* library, UDATA runningReadOnly, LastErrorInfo* lastErrorInfo);

  // the generation is only used for determining the location of
  // the header lock. if we use the _config object to do that instead,
  // we can omit the generation parameter.
  IDATA acquireAttachReadLock(OMRPortLibrary* library, LastErrorInfo *lastErrorInfo);
  IDATA releaseAttachReadLock(OMRPortLibrary* library);

  IDATA tryAcquireAttachWriteLock(OMRPortLibrary* library);
  IDATA releaseAttachWriteLock(OMRPortLibrary* library);

  inline bool cacheFileAccessAllowed() const;
  bool isCacheAccessible() const;

  OSMemoryMappedCacheHeader* _header;
  OSCacheLayout* _layout;
  
  I_64 _actualFileLength;
  UDATA _fileHandle;

  UDATA _finalised; // is the cache finalised, resources returned before the cache is destroyed
  SH_CacheFileAccess _cacheFileAccess; // the status of the cache file access.

  omrthread_monitor_t* _lockMutex; // there should be _numLocks of these.

  const U_32 _numLocks; // this is new. serialize will write _dataLocks as an array of numLocks I_32 entry.
  const U_32 _writeLockID;     // an index between 0 and _numLocks-1 inclusive. for the write exclusive lock.
  const U_32 _readWriteLockID; // an index between 0 and _numLocks-1 inclusive. for the read/write lock.
};

#endif
