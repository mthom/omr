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

#if !defined(OS_MEMORY_MAPPED_CACHE_HPP_INCLUDED)
#define OS_MEMORY_MAPPED_CACHE_HPP_INCLUDED

#include "OSPersistentCache.hpp"
#include "OSMemoryMappedCacheConfig.hpp"
#include "OSMemoryMappedInitializationContext.hpp"

#include "omr.h"
#include "omrport.h"

// an implementation of a persistent shared cache that uses omrmmap primitives
// and region-based locks on sections of files.
class OSMemoryMappedCache: public OSPersistentCache {
public:  
  virtual void getError();

  OSMemoryMappedCache::OSMemoryMappedCache(OMRPortLibrary* library,
					   OMR_VM* vm,
					   const char* cacheDirName,
					   const char* cacheName,
					   IDATA numLocks,
					   OSMemoryMappedCacheConfigOptions configOptions,
					   I_32 openMode);

  bool startup(I_32 openMode);

  //TODO: should these functions be virtual?
  virtual void initialize(OMRPortLibrary* library);
  virtual void finalise();
  
protected:
  IDATA internalAttach();
  void internalDetach();
  
  OSMemoryMappedCacheIterator* getMemoryMappedCacheIterator();

  OSMemoryMappedInitializationContext* _init_context;
  OSMemoryMappedCacheConfig* _config;

  OMRMmapHandle *_mapFileHandle;
};

#endif
