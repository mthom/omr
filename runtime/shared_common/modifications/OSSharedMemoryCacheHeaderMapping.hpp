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
#if !defined(OS_SHARED_MEMORY_CACHE_HEADER_MAPPING_HPP_INCLUDED)
#define OS_SHARED_MEMORY_CACHE_HEADER_MAPPING_HPP_INCLUDED

#include "CacheHeaderMapping.hpp"

#include "omr.h"

#define OMRSH_OSCACHE_SYSV_EYECATCHER "OMRSMMAP"
#define OMRSH_OSCACHE_SYSV_EYECATCHER_LENGTH 8

class OSSharedMemoryCacheHeader;

template <>
struct HeaderMapping<OSSharedMemoryCacheHeader> {
    typedef struct OSSharedMemoryCacheHeaderMapping mapping_type;
};

struct OSSharedMemoryCacheHeaderMapping: HeaderMapping<OSSharedMemoryCacheHeader>
{
  OSSharedMemoryCacheHeaderMapping()
    : _createTime(0)
    , _inDefaultControlDir(1)
    , _dataSectionLength(0)
  {}

  // the owning header knows the value of numLocks.
  UDATA size(UDATA numLocks) const {
      UDATA size = 0;

      size += OMRSH_OSCACHE_SYSV_EYECATCHER_LENGTH + 1;
      size += sizeof(_createTime);
      size += sizeof(_inDefaultControlDir);
      
      return size;
  }

  char _eyecatcher[OMRSH_OSCACHE_SYSV_EYECATCHER_LENGTH+1];
  I_64 _createTime; // from OSCache_sysv_header1 & 2: createTime
  UDATA _inDefaultControlDir; // from OSCache_sysv_header1 & 2: inDefaultControlDir
};

#endif
