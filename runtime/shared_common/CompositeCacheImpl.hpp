/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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

#if !defined(COMPOSITECACHEIMPL_H_INCLUDED)
#define COMPOSITECACHEIMPL_H_INCLUDED

/* @ddr_namespace: default */
#include "OSCache.hpp"
#include "OSCachesysv.hpp"
#include "CompositeCache.hpp"
#include "sharedconsts.h"
//#include "ClassDebugDataProvider.hpp"
#include "SCTransactionCTypes.h"
#include "AbstractMemoryPermission.hpp"

#define MIN_CC_SIZE 0x1000
#define MAX_CC_SIZE 0x7FFFFFF8

#if defined(WIN32)
#define OMRSHR_MIN_GAP_BEFORE_METADATA 1024
/* On Windows, 1K is reserved to try and avoid a cache corruption bug.
 * See SH_CompositeCacheImpl::allocate().
 * Use a higher threshold on Windows. 
 */
#define CC_MIN_SPACE_BEFORE_CACHE_FULL 3072
#else
#define OMRSHR_MIN_GAP_BEFORE_METADATA 0
#define CC_MIN_SPACE_BEFORE_CACHE_FULL 2048
#endif

#define OMRSHR_DUMMY_DATA_BYTE 0xD9
#define OMRSHR_MIN_DUMMY_DATA_SIZE (sizeof(ShcItem) + sizeof(ShcItemHdr) + SHC_WORDALIGN)

#define CC_STARTUP_OK 0
#define CC_STARTUP_FAILED -1
#define CC_STARTUP_CORRUPT -2
#define CC_STARTUP_RESET -3
#define CC_STARTUP_SOFT_RESET -4
#define CC_STARTUP_NO_CACHELETS -5
#define CC_STARTUP_NO_CACHE -6

/* How many bytes to sample from across the cache for calculating the CRC */
/* Need a value that has negligible impact on performance */
#define OMRSHR_CRC_MAX_SAMPLES 100000

/**
 * Represents view of the shared cache at the level of blocks of memory.
 * 
 * Allocates memory backwards from the end of the cache, apart from segment
 * memory which it allocates from the start of the cache (see allocate below).
 * Provides a mutex for accessing the shared cache and functions 
 * for walking and reading the cache.
 */
class SH_CompositeCacheImpl : public SH_CompositeCache, public AbstractMemoryPermission
{
protected:
	void *operator new(size_t size, void *memoryPtr) { return memoryPtr; };

public:
	typedef char* BlockPtr;

	static UDATA getRequiredConstrBytes(bool isNested, bool startupForStats);
	
	static UDATA getRequiredConstrBytesWithCommonInfo(bool isNested, bool startupForStats);


#if defined(J9SHR_CACHELET_SUPPORT)
	static SH_CompositeCacheImpl* newInstanceNested(OMR_VM* vm, SH_CompositeCacheImpl* parent, SH_CompositeCacheImpl* memForConstructor, UDATA nestedSize, BlockPtr nestedMemory, bool creatingCachelet);

	IDATA startupNested(OMR_VMThread* currentThread);

	static SH_CompositeCacheImpl* newInstanceChained(OMR_VM* vm, SH_CompositeCacheImpl* memForConstructor, OMRSharedCacheConfig* sharedClassConfig, I_32 cacheTypeRequired);

	IDATA startupChained(OMR_VMThread* currentThread, SH_CompositeCacheImpl* ccHead,
			OMRSharedCachePreinitConfig* piconfig, U_32* actualSize, UDATA* localCrashCntr);
#endif /* J9SHR_CACHELET_SUPPORT */
	
	static SH_CompositeCacheImpl* newInstance(OMR_VM* vm, OMRSharedCacheConfig* sharedClassConfig, SH_CompositeCacheImpl* memForConstructor, const char* cacheName, I_32 newPersistentCacheReqd, bool startupForStats);
	
	IDATA startup(OMR_VMThread* currentThread, OMRSharedCachePreinitConfig* piconfig, BlockPtr cacheMemoryUT, U_64* runtimeFlags, UDATA verboseFlags,
			const char* rootName, const char* ctrlDirName, UDATA cacheDirPerm, U_32* actualSize, UDATA* localCrashCntr, bool isFirstStart, bool* cacheHasIntegrity);

	void cleanup(OMR_VMThread* currentThread);

	IDATA deleteCache(OMR_VMThread *currentThread, bool suppressVerbose);

	IDATA enterReadWriteAreaMutex(OMR_VMThread* currentThread, BOOLEAN readOnly, UDATA* doRebuildLocalData, UDATA* doRebuildCacheData);

	IDATA exitReadWriteAreaMutex(OMR_VMThread* currentThread, UDATA resetReason);

	IDATA enterWriteMutex(OMR_VMThread* currentThread, bool lockCache, const char* caller);

	IDATA exitWriteMutex(OMR_VMThread* currentThread, const char* caller, bool doDecWriteCounter=true);

	void doLockCache(OMR_VMThread* currentThread);

	void doUnlockCache(OMR_VMThread* currentThread);

	bool isLocked(void);
	
	IDATA enterReadMutex(OMR_VMThread* currentThread, const char* caller);

	void exitReadMutex(OMR_VMThread* currentThread, const char* caller);

	BlockPtr allocateBlock(OMR_VMThread* currentThread, ShcItem* itemToWrite, U_32 align, U_32 alignOffset);

	BlockPtr allocateWithSegment(OMR_VMThread* currentThread, ShcItem* itemToWrite, U_32 segBufSize, BlockPtr* segBuf);

	BlockPtr allocateAOT(OMR_VMThread* currentThread, ShcItem* itemToWrite, U_32 dataBytes);

	BlockPtr allocateJIT(OMR_VMThread* currentThread, ShcItem* itemToWrite, U_32 dataBytes);

	BlockPtr allocateWithReadWriteBlock(OMR_VMThread* currentThread, ShcItem* itemToWrite, U_32 readWriteBufferSize, BlockPtr* readWriteBuffer);

	UDATA checkUpdates(OMR_VMThread* currentThread);

	void doneReadUpdates(OMR_VMThread* currentThread, IDATA updates);

	void updateStoredSegmentUsedBytes(U_32 usedBytes);

	void commitUpdate(OMR_VMThread* currentThread, bool isCachelet);

	void initBlockData(ShcItem** itemBuf, U_32 dataLen, U_16 dataType);

	void rollbackUpdate(OMR_VMThread* currentThread);

	void startCriticalUpdate(OMR_VMThread* currentThread);

	void endCriticalUpdate(OMR_VMThread* currentThread);

	bool isCacheCorrupt(void);

	void setCorruptCache(OMR_VMThread *currentThread, IDATA corruptionCode, UDATA corruptValue);

	bool crashDetected(UDATA* localCrashCntr);

	void reset(OMR_VMThread* currentThread);

	BlockPtr nextEntry(OMR_VMThread* currentThread, UDATA* staleItems);
	
	void markStale(OMR_VMThread* currentThread, BlockPtr block, bool isCacheLocked);

	UDATA stale(BlockPtr block);
	
	void findStart(OMR_VMThread* currentThread);
	
	void* getBaseAddress(void);

	OMRSharedCacheHeader* getCacheHeaderAddress(void);
	
	void* getStringTableBase(void);
	
//#if defined(J9SHR_CACHELETS_SAVE_READWRITE_AREA)
//	void* getReadWriteAllocPtr(void);
//#endif

	UDATA getCacheMemorySize(void);

	void* getCacheEndAddress(void);
	
	void* getCacheLastEffectiveAddress(void);

	void* getSegmentAllocPtr(void);

	void* getMetaAllocPtr(void);

	UDATA getTotalUsableCacheSize(void);

	void getMinMaxBytes(U_32 *softmx, I_32 *minAOT, I_32 *maxAOT, I_32 *minJIT, I_32 *maxJIT);

	UDATA getFreeBytes(void);
	
	UDATA getFreeAvailableBytes(void);

	U_32 getUsedBytes(void);

  //	BOOLEAN isAddressInCacheDebugArea(void *address, UDATA length);

  //	U_32 getDebugBytes(void);

  //	U_32 getFreeDebugSpaceBytes(void);

  //	U_32 getLineNumberTableBytes(void);

  //	U_32 getLocalVariableTableBytes(void);

	UDATA getFreeReadWriteBytes(void);
	
	UDATA getTotalStoredBytes(void);

	void getUnstoredBytes(U_32* softmxUnstoredBytes, U_32* maxAOTUnstoredBytes, U_32* maxJITUnstoredBytes) const;

	UDATA getAOTBytes(void);

	UDATA getJITBytes(void);

	UDATA getReadWriteBytes(void);
	
	UDATA getStringTableBytes(void);
	
	UDATA testAndSetWriteHash(OMR_VMThread *currentThread, UDATA hash);

	UDATA tryResetWriteHash(OMR_VMThread *currentThread, UDATA hash);

	void setWriteHash(OMR_VMThread *currentThread, UDATA hash);

	bool peekForWriteHash(OMR_VMThread *currentThread);

	bool isAddressInROMClassSegment(const void* address);

	bool isAddressInCache(const void* address);

	void runExitCode(OMR_VMThread *currentThread);
	
	U_16 getJVMID(void);

	void setInternCacheHeaderFields(J9SRP** sharedTail, J9SRP** sharedHead, U_32** totalSharedNodes, U_32** totalSharedWeight);
	
	UDATA getReaderCount(OMR_VMThread* currentThread);

	bool hasWriteMutex(OMR_VMThread* currentThread);

	bool hasReadWriteMutex(OMR_VMThread* currentThread);

	void notifyRefreshMutexEntered(OMR_VMThread* currentThread);

	void notifyRefreshMutexExited(OMR_VMThread* currentThread);
	
	bool isRunningReadOnly(void);

	bool isVerbosePages(void);

	bool isMemProtectEnabled(void);

	bool isMemProtectPartialPagesEnabled(void);

	U_32 getTotalSize(void);
	
  //	UDATA getJavacoreData(OMR_VM *vm, J9SharedClassJavacoreDataDescriptor* descriptor);

	void updateMetadataSegment(OMR_VMThread* currentThread);
	
	BOOLEAN isReadWriteAreaHeaderReadOnly();

	SH_CompositeCacheImpl* getNext(void);
	
	void setNext(SH_CompositeCacheImpl* next);

	U_32 getBytesRequiredForItemWithAlign(ShcItem* itemToWrite, U_32 align, U_32 alignOffset);

	U_32 getBytesRequiredForItem(ShcItem* itemToWrite);

	J9MemorySegment* getCurrentROMSegment(void);
	
	void setCurrentROMSegment(J9MemorySegment* segment);
	
	BlockPtr getFirstROMClassAddress(bool isNested);

	bool isStarted(void);
	
	bool getIsNoLineNumberEnabled(void);

	void setIsNoLineNumberEnabled(bool value);
	
	bool getIsNoLineNumberContentEnabled(void);

	void setNoLineNumberContentEnabled(OMR_VMThread *currentThread);

	bool getIsLineNumberContentEnabled(void);

	void setLineNumberContentEnabled(OMR_VMThread* currentThread);

	UDATA getOSPageSize(void);

#if defined(J9SHR_CACHELET_SUPPORT)
	bool copyFromCacheChain(OMR_VMThread* currentThread, SH_CompositeCacheImpl* _ccHead, IDATA* metadataOffset);
	
	SH_CompositeCacheImpl* getParent(void);
	
	IDATA getDeployedOffset(void);

	void setDeployedOffset(IDATA offset);
	
	IDATA fixupSerializedCompiledMethods(OMR_VMThread* currentThread, void*
			serializedROMClassStartAddress);
	
#if defined(J9SHR_CACHELETS_SAVE_READWRITE_AREA)
	UDATA computeDeployedReadWriteOffsets(OMR_VMThread* currentThread, SH_CompositeCacheImpl* ccHead);
	IDATA getDeployedReadWriteOffset(void);
	void setDeployedReadWriteOffset(IDATA offset);
#endif

#if 0
	void growCacheInPlace(UDATA rwGrowth, UDATA freeGrowth);
#endif
	
	BlockPtr getNestedMemory(void);

	UDATA countROMSegments(OMR_VMThread* currentThread);
	UDATA writeROMSegmentMetadata(OMR_VMThread* currentThread, UDATA numSegments, BlockPtr dest, UDATA* lastSegmentAlloc); 

	IDATA lockStartupMonitor(OMR_VMThread* currentThread);
	void unlockStartupMonitor(OMR_VMThread* currentThread);
#endif /* J9SHR_CACHELET_SUPPORT */
	
	bool getContainsCachelets(void);

	void setStringTableInitialized(bool);
	
	bool isStringTableInitialized(void);

//	IDATA allocateClassDebugData(OMR_VMThread* currentThread, U_16 classnameLength, const char* classnameData, const J9RomClassRequirements * sizes, J9SharedRomClassPieces * pieces);
//
//	void rollbackClassDebugData(OMR_VMThread* currentThread, U_16 classnameLength, const char* classnameData);

//	void commitClassDebugData(OMR_VMThread* currentThread, U_16 classnameLength, const char* classnameData);

//	void * getClassDebugDataStartAddress(void);

	IDATA startupForStats(OMR_VMThread* currentThread, SH_OSCache * oscache, U_64 * runtimeFlags, UDATA verboseFlags);

	IDATA shutdownForStats(OMR_VMThread* currentThread);

#if defined(J9SHR_CACHELET_SUPPORT)
	IDATA startupNestedForStats(OMR_VMThread* currentThread);
#endif

	static IDATA getNumRequiredOSLocks();

	void getCorruptionContext(IDATA *corruptionCode, UDATA *corruptValue);

	void setCorruptionContext(IDATA corruptionCode, UDATA corruptValue);

	I_32 getFreeBlockBytes(void);

	bool getIsBCIEnabled(void);

	void setRuntimeCacheFullFlags(OMR_VMThread *currentThread);

	void protectLastUnusedPages(OMR_VMThread *currentThread);

	bool isAllRuntimeCacheFullFlagsSet(void) const;

	void markReadOnlyCacheFull(void);

	I_32 getAvailableReservedAOTBytes(OMR_VMThread *currentThread);

	I_32 getAvailableReservedJITBytes(OMR_VMThread *currentThread);

	bool isCacheMarkedFull(OMR_VMThread *currentThread);

	void setCacheHeaderFullFlags(OMR_VMThread *currentThread, UDATA flags, bool setRuntimeFlags);

	void clearCacheHeaderFullFlags(OMR_VMThread *currentThread);

	void fillCacheIfNearlyFull(OMR_VMThread* currentThread);

	bool isUsingWriteHash(void) const {
		return _useWriteHash;
	}

	void setCacheHeaderExtraFlags(OMR_VMThread *currentThread, UDATA extraFlags);

	bool checkCacheCompatibility(OMR_VMThread *currentThread);

	void setAOTHeaderPresent(OMR_VMThread *currentThread);

	bool isAOTHeaderPresent(OMR_VMThread *currentThread);

	bool isMprotectPartialPagesSet(OMR_VMThread *currentThread);

	bool isMprotectPartialPagesOnStartupSet(OMR_VMThread *currentThread);

	SH_CacheAccess isCacheAccessible(void) const;
	
	bool isRestrictClasspathsSet(OMR_VMThread *currentThread);

	bool canStoreClasspaths(void) const;

	IDATA restoreFromSnapshot(OMR_VM* vm, const char* cacheName, bool* cacheExist);
	void dontNeedMetadata(OMR_VMThread *currentThread, const void* startAddress, size_t length);

	void changePartialPageProtection(OMR_VMThread *currentThread, void *addr, bool readOnly, bool phaseCheck = true);

	void protectPartiallyFilledPages(OMR_VMThread *currentThread, bool protectSegmentPage = true, bool protectMetadataPage = true, bool protectDebugDataPages = true, bool phaseCheck = true);

	void unprotectPartiallyFilledPages(OMR_VMThread *currentThread, bool unprotectSegmentPage = true, bool unprotectMetadataPage = true, bool unprotectDebugDataPages = true, bool phaseCheck = true);

	I_32 tryAdjustMinMaxSizes(OMR_VMThread *currentThread, bool isJCLCall = false);

	void updateRuntimeFullFlags(OMR_VMThread* currentThread);
	
	void increaseUnstoredBytes(U_32 blockBytes, U_32 aotBytes, U_32 jitBytes);

private:
  	OMRSharedCacheConfig* _sharedClassConfig;
	SH_OSCache* _oscache;
	omrthread_monitor_t _utMutex, _headerProtectMutex, _runtimeFlagsProtectMutex;
	OMRPortLibrary* _portlib;

	OMRSharedCacheHeader* _theca;
	bool _started;
	const char* _cacheName;
	
	SH_CompositeCacheImpl* _next;
	SH_CompositeCacheImpl* _parent;
	SH_CompositeCacheImpl* _ccHead; /* first supercache, if chained */
	
	ShcItemHdr* _scan;
	ShcItemHdr* _prevScan;
	ShcItemHdr* _storedScan;
	ShcItemHdr* _storedPrevScan;
	BlockPtr _romClassProtectEnd;

	UDATA _oldUpdateCount;

	U_32 _storedSegmentUsedBytes;
	U_32 _storedMetaUsedBytes;
	U_32 _storedAOTUsedBytes;
	U_32 _storedJITUsedBytes;
	U_32 _storedReadWriteUsedBytes;
	U_32 _softmxUnstoredBytes;
	U_32 _maxAOTUnstoredBytes;
	U_32 _maxJITUnstoredBytes;
	I_32 _maxAOT;
	I_32 _maxJIT;

	U_64* _runtimeFlags;
	UDATA _verboseFlags;
	UDATA _cacheFullFlags;

	U_32 _totalStoredBytes;

	UDATA _lastFailedWriteHash;
	U_32 _lastFailedWHCount;

	BlockPtr _readWriteAreaStart;
	U_32 _readWriteAreaBytes;
	BlockPtr _readWriteAreaPageStart;
	U_32 _readWriteAreaPageBytes;
	
	BlockPtr _cacheHeaderPageStart;
	U_32 _cacheHeaderPageBytes;

	UDATA _osPageSize;
	
	UDATA _nestedSize;
	BlockPtr _nestedMemory;
	
	UDATA _localReadWriteCrashCntr;

	J9MemorySegment** _metadataSegmentPtr;
	J9MemorySegment* _currentROMSegment;
	
	bool _doReadWriteSync;
	bool _doHeaderReadWriteProtect;
	bool _readWriteAreaHeaderIsReadOnly;
	bool _doHeaderProtect;
	bool _doSegmentProtect;
	bool _doMetaProtect;
	bool _doPartialPagesProtect;
	bool _readOnlyOSCache;
#if defined (J9SHR_MSYNC_SUPPORT)
	bool _doMetaSync;
	bool _doHeaderSync;
	bool _doSegmentSync;
#endif
	
	IDATA _headerProtectCntr;
	IDATA _readWriteProtectCntr;
	IDATA _readOnlyReaderCount;
	bool _incrementedRWCrashCntr;
	
	bool _useWriteHash;
	
	bool _canStoreClasspaths;

	bool _reduceStoreContentionDisabled;

#if defined(J9SHR_CACHELET_SUPPORT)
	/**
	 * @bug THIS IS A HORRIBLE HACK FOR CMVC 141328. THIS WILL NOT WORK FOR NON-READONLY CACHES.
	 * In the non-readonly case, cachelet startup is completely broken due to a lock ordering 
	 * problem involving the write area mutex. Deadlock scenario:  
	 * <table>
	 * <tr><th>Thread A<th>Thread B</tr>
	 * <tr><td>storeSharedClass()<td>findCompiledMethod()</tr>
	 * <tr><td>enterWriteMutex()<td>enterReadMutex()</tr>
	 * <tr><td>startupHintCachelets()<td>startupHintCachelets()</tr>
	 * <tr><td>block on _startupMonitor<td>lock _startupMonitor</tr>
	 * <tr><td>- <td>run startup(), block in enterWriteMutex()</tr>
	 * </table>
	 * 
	 * _startupMonitor protects against:
	 * <ol>
	 * <li>the cachelet being simultaneously started by different threads from the same manager
	 * <li>the cachelet being simultaneously started by different threads from different managers
	 * <li>one thread seeing the cachelet as started before readCache() on it was done
	 * </ol>
	 * Always get _startupMonitor before the refresh mutex. We should never need to startup a
	 * cachelet while holding the refresh mutex.
	 * @see SH_Manager::startupHintCachelets, SH_ROMClassResourceManager::rrmTableLookup
	 */
	omrthread_monitor_t _startupMonitor;
	IDATA _deployedOffset;
#if defined(J9SHR_CACHELETS_SAVE_READWRITE_AREA)
	IDATA _deployedReadWriteOffset;
	bool _commitParent;
#endif
#endif

	/* All instances of this class share a common debug & raw class data region
	 */
  //	ClassDebugDataProvider * _debugData;
	
	/* All composite caches within a JVM point to the same 'J9ShrCompositeCacheCommonInfo' 
	 * structure.
	 */
	J9ShrCompositeCacheCommonInfo * _commonCCInfo;
	
	BlockPtr next(OMR_VMThread* currentThread);

	void incReaderCount(OMR_VMThread* currentThread);
	void decReaderCount(OMR_VMThread* currentThread);

	void initialize(OMR_VM* vm, BlockPtr memForConstructor, OMRSharedCacheConfig* sharedClassConfig, const char* cacheName, I_32 cacheTypeRequired, bool startupForStats);
	void initializeWithCommonInfo(OMR_VM* vm, OMRSharedCacheConfig* sharedClassConfig, BlockPtr memForConstructor, const char* cacheName, I_32 newPersistentCacheReqd, bool startupForStats);
	void initCommonCCInfoHelper();

#if defined(J9SHR_CACHELET_SUPPORT)
	void initializeNested(OMR_VM* vm, SH_CompositeCacheImpl* parent, BlockPtr memForConstructor, UDATA nestedSize, BlockPtr nestedMemory, bool creatingCachelet);
#endif

	void commonInit(OMR_VM* vm);
	
	BlockPtr allocate(OMR_VMThread* currentThread, U_8 type, ShcItem* itemToWrite, U_32 len2, U_32 segBufSize, 
			BlockPtr* segBuf, BlockPtr* readWriteBuffer, U_32 align, U_32 alignOffset);

	BlockPtr allocateMetadataEntry(OMR_VMThread* currentThread, BlockPtr allocPtr, ShcItem *itemToWrite, U_32 itemLen);

#if defined(J9SHR_CACHELETS_SAVE_READWRITE_AREA)
	BlockPtr allocateReadWrite(U_32 separateBufferSize);
#endif
	void setCacheAreaBoundaries(OMR_VMThread* currentThread, OMRSharedCachePreinitConfig* piConfig);

	void notifyPagesRead(BlockPtr start, BlockPtr end, UDATA expectedDirection, bool protect);
	void notifyPagesCommitted(BlockPtr start, BlockPtr end, UDATA expectedDirection);

	void unprotectHeaderReadWriteArea(OMR_VMThread* currentThread, bool changeReadWrite);
	void protectHeaderReadWriteArea(OMR_VMThread* currentThread, bool changeReadWrite);
	
	void unprotectMetadataArea();
	void protectMetadataArea(OMR_VMThread *currentThread);

	U_32 getCacheCRC(void);
	U_32 getCacheAreaCRC(U_8* areaStart, U_32 areaSize);
	void updateCacheCRC(void);
	bool checkCacheCRC(bool* cacheHasIntegrity, UDATA *crcValue);

#if defined(J9SHR_CACHELET_SUPPORT)
	void setContainsCachelets(OMR_VMThread* currentThread);
#endif

	IDATA setRegionPermissions(OMRPortLibrary* portLibrary, void *address, UDATA length, UDATA flags);

	void commitUpdateHelper(OMR_VMThread* currentThread, bool isCachelet);

	void setIsLocked(bool value);

	bool isCacheInitComplete(void);

	void setCorruptCache(OMR_VMThread *currentThread);

	I_32 getFreeAOTBytes(OMR_VMThread *currentThread);
	I_32 getFreeJITBytes(OMR_VMThread *currentThread);
	
	void setSoftMaxBytes(OMR_VMThread *currentThread, U_32 softMaxBytes, bool isJCLCall = false);

	void unsetCacheHeaderFullFlags(OMR_VMThread *currentThread, UDATA flagsToUnset);

	BlockPtr getRomClassProtectEnd() {
		return _romClassProtectEnd;
	}
	void setRomClassProtectEnd(BlockPtr pointer) {
		_romClassProtectEnd = pointer;
	}

	class SH_SharedCacheHeaderInit : public SH_OSCache::SH_OSCacheInitializer
	{
	protected:
		void *operator new(size_t size, void *memoryPtr) { return memoryPtr; };

	public:
		static SH_SharedCacheHeaderInit* newInstance(SH_SharedCacheHeaderInit* memForConstructor);
		virtual void init(BlockPtr data, U_32 len, I_32 minAOT, I_32 maxAOT, I_32 minJIT, I_32 maxJIT, U_32 readWriteLen, U_32 softMaxBytes);
	};

	SH_SharedCacheHeaderInit* _newHdrPtr;

	friend IDATA testProtectNewROMClassData_test1(OMR_VM* vm);
	friend IDATA testProtectSharedCacheData_test1(OMR_VM* vm);
	friend IDATA testProtectSharedCacheData_test2(OMR_VM* vm);
	friend IDATA testProtectSharedCacheData_test3(OMR_VM* vm);
	friend class OpenCacheHelper;
};

#endif /* !defined(COMPOSITECACHEIMPL_H_INCLUDED) */

