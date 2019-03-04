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


#if !defined(OS_MEMORY_MAPPED_CACHE_ATTACHING_CONTEXT_HPP_INCLUDED)
#define OS_MEMORY_MAPPED_CACHE_ATTACHING_CONTEXT_HPP_INCLUDED

#include "omr.h"

#include "OSCacheMemoryMappedInitializationContext.hpp"

class OSMemoryMappedCacheConfig;

class OSMemoryMappedCacheAttachingContext: public OSCacheMemoryMappedInitializationContext
{
public:
  OSMemoryMappedCacheAttachingContext(OSMemoryMappedCacheConfig* _config)
    : _config(_config)
  {}

  // attach originally had this parameter: ..., J9PortShcVersion* expectedVersionData)
  virtual void *attach(OMR_VMThread* currentThread);
  virtual void detach();

  virtual IDATA internalAttach();
protected:
  // this should probably be updated within the MemoryMapped class. It doesn't
  // apply to the shared memory version.
  #if defined (J9SHR_MSYNC_SUPPORT)
  virtual IDATA syncUpdates(void* start, UDATA length, U_32 flags);
  #endif

  OSMemoryMappedCacheConfig* _config;
};

#endif
