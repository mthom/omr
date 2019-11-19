/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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

/**
 * @file
 * @ingroup Shared_Common
 */

#if !defined(SHAREDCACHE_HPP_INCLUDED)
#define SHAREDCACHE_HPP_INCLUDED

/* @ddr_namespace: default */
// #include "ClasspathItem.hpp"
#include "Manager.hpp"
#include "CompositeCache.hpp"
#include "sharedconsts.h"
#include "ut_omrshr.h"

/**
 * Interface to shared cache subsystem.
 *
 * @see SH_SharedClassCache.hpp
 * @ingroup Shared_Common
 */
class SH_SharedCache
{
public:
	typedef char* BlockPtr;

#if defined(J9SHR_CACHELET_SUPPORT)
	virtual bool serializeSharedCache(OMR_VMThread* currentThread) = 0;
#endif

  //	virtual const J9ROMClass* findROMClass(OMR_VMThread* currentThread, const char* path, ClasspathItem* classpath, const J9UTF8* partition, const J9UTF8* modContext, IDATA confirmedEntries, IDATA* foundAtIndex) = 0;

        virtual const U_8* storeCompiledMethod(OMR_VMThread* currentThread, const MethodNameAndSignature* methodNameAndSignature, const U_8* dataStart, UDATA dataSize, const U_8* codeStart, UDATA codeSize, UDATA forceReplace) = 0;

	virtual const U_8* findCompiledMethod(OMR_VMThread* currentThread, const MethodNameAndSignature* methodNameAndSignature, UDATA* flags) = 0;

	virtual IDATA findSharedData(OMR_VMThread* currentThread, const char* key, UDATA keylen, UDATA limitDataType, UDATA includePrivateData, OMRSharedDataDescriptor* firstItem, const J9Pool* descriptorPool) = 0;

	virtual const U_8* storeSharedData(OMR_VMThread* currentThread, const char* key, UDATA keylen, const OMRSharedDataDescriptor* data) = 0;

	virtual const U_8* findAttachedDataAPI(OMR_VMThread* currentThread, const void* addressInCache, OMRSharedDataDescriptor* data, IDATA *corruptOffset) = 0;

	virtual UDATA storeAttachedData(OMR_VMThread* currentThread, const void* addressInCache, const OMRSharedDataDescriptor* data, UDATA forceReplace) = 0;

	virtual UDATA updateAttachedData(OMR_VMThread* currentThread, const void* addressInCache, I_32 updateAtOffset, const OMRSharedDataDescriptor* data) = 0;

	virtual UDATA updateAttachedUDATA(OMR_VMThread* currentThread, const void* addressInCache, UDATA type, I_32 updateAtOffset, UDATA value) = 0;
	
	virtual UDATA acquirePrivateSharedData(OMR_VMThread* currentThread, const OMRSharedDataDescriptor* data) = 0;

	virtual UDATA releasePrivateSharedData(OMR_VMThread* currentThread, const OMRSharedDataDescriptor* data) = 0;

  //virtual IDATA markStale(OMR_VMThread* currentThread, ClasspathEntryItem* cpei, bool hasWriteMutex) = 0;

	virtual IDATA isStale(const ShcItem* item) = 0;

  //	virtual void markItemStale(OMR_VMThread* currentThread, const ShcItem* item, bool isCacheLocked) = 0;
    
  //	virtual void markItemStaleCheckMutex(OMR_VMThread* currentThread, const ShcItem* item, bool isCacheLocked) = 0;
    
	virtual void destroy(OMR_VMThread* currentThread) = 0;

	virtual IDATA printCacheStats(OMR_VMThread* currentThread, UDATA showFlags, U_64 runtimeFlags) = 0;

	virtual void printShutdownStats(void) = 0;
	
	virtual void runExitCode(OMR_VMThread* currentThread) = 0;

	virtual void cleanup(OMR_VMThread* currentThread) = 0;

	virtual bool isBytecodeAgentInstalled(void) = 0;

	virtual IDATA enterLocalMutex(OMR_VMThread* currentThread, omrthread_monitor_t monitor, const char* name, const char* caller) = 0;

	virtual IDATA exitLocalMutex(OMR_VMThread* currentThread, omrthread_monitor_t monitor, const char* name, const char* caller) = 0;

	virtual IDATA enterStringTableMutex(OMR_VMThread* currentThread, BOOLEAN readOnly, UDATA* doRebuildLocalData, UDATA* doRebuildCacheData) = 0;

	virtual IDATA exitStringTableMutex(OMR_VMThread* currentThread, UDATA resetReason) = 0;
	
  //	virtual void notifyClasspathEntryStateChange(OMR_VMThread* currentThread, const char* path, UDATA newState) = 0;

	virtual SH_CompositeCache* getCompositeCacheAPI() = 0;
	
#if defined(J9SHR_CACHELET_SUPPORT)
	virtual IDATA startupCachelet(OMR_VMThread* currentThread, SH_CompositeCache* cachelet) = 0;
#endif
	
	virtual IDATA getAndStartManagerForType(OMR_VMThread* currentThread, UDATA dataType, SH_Manager** startedManager) = 0;
	
  //	virtual void getRomClassAreaBounds(void ** romClassAreaStart, void ** romClassAreaEnd) = 0;

	virtual SH_Managers * managers() = 0;

	virtual OMRSharedCacheConfig* getSharedClassConfig() = 0;

	virtual IDATA aotMethodOperation(OMR_VMThread* currentThread, char* methodSpecs, UDATA reason) = 0;

protected:
	/* - Virtual destructor has been added to avoid compile warnings. 
	 * - Delete operator added to avoid linkage with C++ runtime libs 
	 */
	void operator delete(void *) { return; }
	virtual ~SH_SharedCache() { Trc_SHR_Assert_ShouldNeverHappen(); };

};

#endif /* !defined(SHAREDCACHE_HPP_INCLUDED) */
