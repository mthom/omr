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
#if !defined(OS_SHARED_MEMORY_CACHE_HEADER_HPP_INCLUDED)
#define OS_SHARED_MEMORY_CACHE_HEADER_HPP_INCLUDED

#include "sharedconsts.h"

#include "OSCacheContiguousRegion.hpp"

#define OMRSH_OSCACHE_SYSV_EYECATCHER "J9SMMAP"
#define OMRSH_OSCACHE_SYSV_EYECATCHER_LENGTH 7

class OSSharedMemoryCacheHeader: public OSCacheContiguousRegion
{
public:
//  virtual U_64 getHeaderLockOffset() = 0;
//  virtual U_64 getAttachLockOffset() = 0;

  virtual void init() = 0; //OSMemoryMappedCacheLayout* layout) = 0;
  
//  U_64 getAttachLockSize() {
//    return sizeof(_attachLock);
//  }  
protected:
  virtual void serialize(OSCacheRegionSerializer* serializer) = 0;

  char _eyecatcher[OMRSH_OSCACHE_SYSV_EYECATCHER_LENGTH+1];
  
  // from the OSCache_header* and OSCachesysv_header* structs.
  // these have been moved to the OSSharedMemoryCacheLayout class.
  // U_32 _headerSize;   // from OSCache_header2: dataLength
  // J9SRP _headerStart; // from OSCache_header2: dataStart

  I_64 _createTime; // from OSCache_sysv_header1 & 2: createTime
  UDATA _inDefaultControlDir; // from OSCache_sysv_header1 & 2: inDefaultControlDir  
};

#endif
