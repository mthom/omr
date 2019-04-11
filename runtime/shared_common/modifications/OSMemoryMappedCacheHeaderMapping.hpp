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
#if !defined(OS_MEMORY_MAPPED_CACHE_HEADER_MAPPING_HPP_INCLUDED)
#define OS_MEMORY_MAPPED_CACHE_HEADER_MAPPING_HPP_INCLUDED

#include "CacheHeaderMapping.hpp"

#include "omr.h"

#define OMRSH_OSCACHE_MMAP_EYECATCHER "J9SCMAP"
#define OMRSH_OSCACHE_MMAP_EYECATCHER_LENGTH 7

class OSMemoryMappedCacheHeader;

template <>
struct CacheHeaderMapping<OSMemoryMappedCacheHeader> {
    typedef struct OSMemoryMappedCacheHeaderMapping mapping_type;
};

struct OSMemoryMappedCacheHeaderMapping: CacheHeaderMapping<OSMemoryMappedCacheHeader>
{
  OSMemoryMappedCacheHeaderMapping()
    : _createTime(0)
    , _lastAttachedTime(0)
    , _lastDetachedTime(0)
    , _headerLock(0)
    , _attachLock(0)
    , _dataSectionLength(0)
    , _dataLocks(NULL)
  {
    _eyecatcher[0] = '\0';
  }

  // the owning header knows the value of numLocks.
  UDATA size(UDATA numLocks) const {
      UDATA size = 0;

      size += OMRSH_OSCACHE_MMAP_EYECATCHER_LENGTH + 1;
      size += sizeof(_createTime);
      size += sizeof(_lastAttachedTime);
      size += sizeof(_lastDetachedTime);
      size += sizeof(_headerLock);
      size += sizeof(_attachLock);
      size += sizeof(_dataSectionLength);
      size += sizeof(*_dataLocks) * numLocks;

      return size;
  }

  char _eyecatcher[OMRSH_OSCACHE_MMAP_EYECATCHER_LENGTH+1];
  I_64 _createTime;   // from OSCache_mmap_header1 & 2: createTime
  I_64 _lastAttachedTime; // from OSCache_mmap_header1 & 2: lastAttachedTime
  I_64 _lastDetachedTime; // from OSCache_mmap_header1 & 2: lastDetachedTime
  I_32 _headerLock; // from OSCache_mmap_header1 & 2: headerLock
  I_32 _attachLock; // from OSCache_mmap_header1 & 2: attachLock
  U_32 _dataSectionLength; // the length of the data section.
  I_32* _dataLocks; // was _dataLocks[_numLocks]; // from OSCache_mmap_header1 & 2: dataLocks
};

#endif
