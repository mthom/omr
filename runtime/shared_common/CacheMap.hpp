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

#if !defined(CACHEMAP_H_INCLUDED)
#define CACHEMAP_H_INCLUDED

/* @ddr_namespace: default */
#include "CacheMapStats.hpp"
#include "SharedCache.hpp"
#include "CompositeCacheImpl.hpp"
//#include "TimestampManager.hpp"
//#include "ClasspathManager.hpp"
//#include "ROMClassManager.hpp"
#include "ScopeManager.hpp"
#include "CompiledMethodManager.hpp"
#include "ByteDataManager.hpp"
#include "AttachedDataManager.hpp"

#define CM_READ_CACHE_FAILED -1
#define CM_CACHE_CORRUPT -2

typedef struct MethodSpecTable {
	char* className;
	char* methodName;
	char* methodSig;
	U_32 classNameMatchFlag;
	U_32 methodNameMatchFlag;
	U_32 methodSigMatchFlag;
	UDATA classNameLength;
	UDATA methodNameLength;
	UDATA methodSigLength;
	bool matchFlag;
} MethodSpecTable;

/*
 * Implementation of SH_SharedCache interface
 */
class SH_CacheMap : public SH_SharedCache, public SH_CacheMapStats
{
protected:
	void *operator new(size_t size, void *memoryPtr) { return memoryPtr; };

public:
	typedef char* BlockPtr;

	static SH_CacheMap* newInstance(OMR_VM* vm, OMRSharedCacheConfig* sharedClassConfig, SH_CacheMap* memForConstructor, const char* cacheName, I_32 newPersistentCacheReqd);

	static SH_CacheMapStats* newInstanceForStats(OMR_VM* vm, SH_CacheMap* memForConstructor, const char* cacheName);

	static UDATA getRequiredConstrBytes(bool startupForStats);

	IDATA startup(OMR_VMThread* currentThread, OMRSharedCachePreinitConfig* piconfig, const char* rootName, const char* cacheDirName, UDATA cacheDirPerm, BlockPtr cacheMemoryUT, bool* cacheHasIntegrity);

#if defined(J9SHR_CACHELET_SUPPORT)
	/* @see SharedCache.hpp */
	virtual bool serializeSharedCache(OMR_VMThread* currentThread);
#endif

	/* @see SharedCache.hpp */
	virtual IDATA enterLocalMutex(OMR_VMThread* currentThread, omrthread_monitor_t monitor, const char* name, const char* caller);

	/* @see SharedCache.hpp */
	virtual IDATA exitLocalMutex(OMR_VMThread* currentThread, omrthread_monitor_t monitor, const char* name, const char* caller);

	/* @see SharedCache.hpp */
	virtual IDATA isStale(const ShcItem* item);

	/* @see SharedCache.hpp */
  //	virtual const J9ROMClass* findROMClass(OMR_VMThread* currentThread, const char* path, ClasspathItem* cp, const OMRUTF8* partition, const OMRUTF8* modContext, IDATA confirmedEntries, IDATA* foundAtIndex);

	/* @see SharedCache.hpp */
	virtual const U_8* storeCompiledMethod(OMR_VMThread* currentThread, const MethodNameAndSignature* nameAndSignature, const U_8* dataStart, UDATA dataSize, const U_8* codeStart, UDATA codeSize, UDATA forceReplace);

	/* @see SharedCache.hpp */
	virtual const U_8* findCompiledMethod(OMR_VMThread* currentThread, const MethodNameAndSignature* nameAndSignature, UDATA* flags);

	/* @see SharedCache.hpp */
	virtual IDATA findSharedData(OMR_VMThread* currentThread, const char* key, UDATA keylen, UDATA limitDataType, UDATA includePrivateData, OMRSharedDataDescriptor* firstItem, const J9Pool* descriptorPool);

	/* @see SharedCache.hpp */
	virtual const U_8* storeSharedData(OMR_VMThread* currentThread, const char* key, UDATA keylen, const OMRSharedDataDescriptor* data);

	/* @see SharedCache.hpp */
		virtual const U_8* findAttachedDataAPI(OMR_VMThread* currentThread, const void* addressInCache, OMRSharedDataDescriptor* data, IDATA *corruptOffset) ;

 	/* @see SharedCache.hpp */
	virtual UDATA storeAttachedData(OMR_VMThread* currentThread, const void* addressInCache, const OMRSharedDataDescriptor* data, UDATA forceReplace) ;

	/* @see SharedCache.hpp */
	virtual UDATA updateAttachedData(OMR_VMThread* currentThread, const void* addressInCache, I_32 updateAtOffset, const OMRSharedDataDescriptor* data) ;

	/* @see SharedCache.hpp */
	virtual UDATA updateAttachedUDATA(OMR_VMThread* currentThread, const void* addressInCache, UDATA type, I_32 updateAtOffset, UDATA value) ;

	/* @see SharedCache.hpp */
	virtual UDATA acquirePrivateSharedData(OMR_VMThread* currentThread, const OMRSharedDataDescriptor* data);

	/* @see SharedCache.hpp */
	virtual UDATA releasePrivateSharedData(OMR_VMThread* currentThread, const OMRSharedDataDescriptor* data);

	void dontNeedMetadata(OMR_VMThread* currentThread);

	/**
	 * This function is extremely hot.
	 * Peeks to see whether compiled code exists for a given ROMMethod in the CompiledMethodManager hashtable
	 *
	 * @param[in] currentThread  The current thread
	 * @param[in] romMethod  The ROMMethod to test
	 *
	 * @return 1 if the code exists in the hashtable, 0 otherwise
	 *
	 * THREADING: This function can be called multi-threaded
	 */
         inline UDATA existsCachedCodeForROMMethodInline(OMR_VMThread* currentThread, const MethodNameAndSignature* nameAndSignature)
         {
     		Trc_SHR_CM_existsCachedCodeForROMMethod_Entry(currentThread, nameAndSignature);

		if (_cmm && _cmm->getState()==MANAGER_STATE_STARTED) {
			UDATA returnVal;
			returnVal = _cmm->existsResourceForROMAddress(currentThread, (UDATA)nameAndSignature);
			Trc_SHR_CM_existsCachedCodeForROMMethod_Exit1(currentThread, returnVal);
			return returnVal;
		}

		Trc_SHR_CM_existsCachedCodeForROMMethod_Exit2(currentThread);
		return 0;
	};

	/* @see SharedCache.hpp */
  //	virtual UDATA getJavacoreData(OMR_VM *vm, OMRSharedCacheJavacoreDataDescriptor* descriptor);

	/* @see SharedCache.hpp */
  //	virtual IDATA markStale(OMR_VMThread* currentThread, ClasspathEntryItem* cpei, bool hasWriteMutex);

	/* @see SharedCache.hpp */
  //	virtual void markItemStale(OMR_VMThread* currentThread, const ShcItem* item, bool isCacheLocked);

	/* @see SharedCache.hpp */
  //	virtual void markItemStaleCheckMutex(OMR_VMThread* currentThread, const ShcItem* item, bool isCacheLocked);

	/* @see SharedCache.hpp */
	virtual void destroy(OMR_VMThread* currentThread);

	/* @see SharedCache.hpp */
	virtual IDATA printCacheStats(OMR_VMThread* currentThread, UDATA showFlags, U_64 runtimeFlags);

	/* @see SharedCache.hpp */
	virtual void printShutdownStats(void);

	/* @see SharedCache.hpp */
	virtual void runExitCode(OMR_VMThread* currentThread);

	/* @see SharedCache.hpp */
	virtual void cleanup(OMR_VMThread* currentThread);

	/* @see SharedCache.hpp */
	virtual bool isBytecodeAgentInstalled(void);

	/* @see SharedCache.hpp */
	virtual IDATA enterStringTableMutex(OMR_VMThread* currentThread, BOOLEAN readOnly, UDATA* doRebuildLocalData, UDATA* doRebuildCacheData);

	/* @see SharedCache.hpp */
	virtual IDATA exitStringTableMutex(OMR_VMThread* currentThread, UDATA resetReason);

	/* @see SharedCache.hpp */
  //	virtual void notifyClasspathEntryStateChange(OMR_VMThread* currentThread, const char* path, UDATA newState);

  //	static IDATA createPathString(OMR_VMThread* currentThread, OMRSharedCacheConfig* config, char** pathBuf, UDATA pathBufSize, ClasspathEntryItem* cpei, const char* className, UDATA classNameLen, bool* doFreeBuffer);

#if defined(J9SHR_CACHELET_SUPPORT)
	/* @see SharedCache.hpp */
  //	virtual IDATA startupCachelet(OMR_VMThread* currentThread, SH_CompositeCache* cachelet);
#endif

	/* @see SharedCache.hpp */
	virtual IDATA getAndStartManagerForType(OMR_VMThread* currentThread, UDATA dataType, SH_Manager** startedManager);

	/* @see SharedCache.hpp */
	virtual SH_CompositeCache* getCompositeCacheAPI();

	/* @see SharedCache.hpp */
	virtual SH_Managers * managers();

	/* @see SharedCache.hpp */
	virtual IDATA aotMethodOperation(OMR_VMThread* currentThread, char* methodSpecs, UDATA action);

	/* @see CacheMapStats.hpp */
	IDATA startupForStats(OMR_VMThread* currentThread, SH_OSCache * oscache, U_64 * runtimeflags);

	/* @see CacheMapStats.hpp */
	IDATA shutdownForStats(OMR_VMThread* currentThread);


	//New Functions To Support New ROM Class Builder
	IDATA startClassTransaction(OMR_VMThread* currentThread, bool lockCache, const char* caller);
	IDATA exitClassTransaction(OMR_VMThread* currentThread, const char* caller);
  //	SH_ROMClassManager* getROMClassManager(OMR_VMThread* currentThread);

  //	ClasspathWrapper* updateClasspathInfo(OMR_VMThread* currentThread, ClasspathItem* cp, I_16 cpeIndex, const OMRUTF8* partition, const OMRUTF8** cachedPartition, const OMRUTF8* modContext, const OMRUTF8** cachedModContext, bool haveWriteMutex);

  //	bool allocateROMClass(OMR_VMThread* currentThread, const J9RomClassRequirements * sizes, J9SharedRomClassPieces * pieces, U_16 classnameLength, const char* classnameData, ClasspathWrapper* cpw, const OMRUTF8* partitionInCache, const OMRUTF8* modContextInCache, IDATA callerHelperID, bool modifiedNoContext, void * &newItemInCache, void * &cacheAreaForAllocate);

  //	IDATA commitROMClass(OMR_VMThread* currentThread, ShcItem* itemInCache, SH_CompositeCacheImpl* cacheAreaForAllocate, ClasspathWrapper* cpw, I_16 cpeIndex, const OMRUTF8* partitionInCache, const OMRUTF8* modContextInCache, BlockPtr romClassBuffer, bool commitOutOfLineData);

  //	IDATA commitOrphanROMClass(OMR_VMThread* currentThread, ShcItem* itemInCache, SH_CompositeCacheImpl* cacheAreaForAllocate, ClasspathWrapper* cpw, BlockPtr romClassBuffer);

  //	IDATA commitMetaDataROMClassIfRequired(OMR_VMThread* currentThread, ClasspathWrapper* cpw, I_16 cpeIndex, IDATA helperID, const OMRUTF8* partitionInCache, const OMRUTF8* modContextInCache, J9ROMClass * romclass);

  //	const J9ROMClass* findNextROMClass(OMR_VMThread* currentThread, void * &findNextIterator, void * &firstFound, U_16 classnameLength, const char* classnameData);

  //	bool isAddressInROMClassSegment(const void* address);

  //	void getRomClassAreaBounds(void ** romClassAreaStart, void ** romClassAreaEnd);

	UDATA getReadWriteBytes(void);

	UDATA getStringTableBytes(void);

	void* getStringTableBase(void);

	void setStringTableInitialized(bool);

	bool isStringTableInitialized(void);

  //	BOOLEAN isAddressInCacheDebugArea(void *address, UDATA length);

  //    U_32 getDebugBytes(void);

	bool isCacheInitComplete(void);

	bool isCacheCorruptReported(void);

	IDATA runEntryPointChecks(OMR_VMThread* currentThread, void* isAddressInCache, const char** subcstr);

	void protectPartiallyFilledPages(OMR_VMThread *currentThread);

	I_32 tryAdjustMinMaxSizes(OMR_VMThread* currentThread, bool isJCLCall = false);

	void updateRuntimeFullFlags(OMR_VMThread* currentThread);

  //    void increaseTransactionUnstoredBytes(U_32 segmentAndDebugBytes, OMRSharedCacheTransaction* obj);

	void increaseUnstoredBytes(U_32 blockBytes, U_32 aotBytes = 0, U_32 jitBytes = 0);

	void getUnstoredBytes(U_32 *softmxUnstoredBytes, U_32 *maxAOTUnstoredBytes, U_32 *maxJITUnstoredBytes) const;

private:
 	SH_CompositeCacheImpl* _cc;					/* current cache */

	/* See other _writeHash fields below. Put U_64 at the top so the debug
	 * extensions can more easily mirror the shape.
	 */
	U_64 _writeHashStartTime;
	OMRSharedCacheConfig* _sharedClassConfig;

	SH_CompositeCacheImpl* _ccHead;				/* head of supercache list */
	SH_CompositeCacheImpl* _cacheletHead;		/* head of all known cachelets */
	SH_CompositeCacheImpl* _ccCacheletHead;		/* head of cachelet list for current cache */
	SH_CompositeCacheImpl* _cacheletTail;		/* tail of all known cachelets */
	SH_CompositeCacheImpl* _prevCClastCachelet;	/* Reference to the last allocated cachelet in the last supercache */
//	SH_ClasspathManager* _cpm;
//	SH_TimestampManager* _tsm;
//	SH_ROMClassManager* _rcm;
	SH_ScopeManager* _scm;
	SH_CompiledMethodManager* _cmm;
  	SH_ByteDataManager* _bdm;
	SH_AttachedDataManager* _adm;
	OMRPortLibrary* _portlib;
	omrthread_monitor_t _refreshMutex;
	bool _cacheCorruptReported;
	U_64* _runtimeFlags;
	const char* _cacheName;
	const char* _cacheDir;
	UDATA _localCrashCntr;

	UDATA _writeHashAverageTimeMicros;
	UDATA _writeHashMaxWaitMicros;
	UDATA _writeHashSavedMaxWaitMicros;
	UDATA _writeHashContendedResetHash;
	/* Also see U_64 _writeHashStartTime above */

	UDATA _verboseFlags;
	UDATA _bytesRead;
	U_32 _actualSize;
	UDATA _cacheletCntr;
	J9Pool* _ccPool;
	uintptr_t  _minimumAccessedShrCacheMetadata;
	uintptr_t _maximumAccessedShrCacheMetadata;
	bool _metadataReleased;

	/* True iff (*_runtimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_NESTED). Set in startup().
	 * This flag is a misnomer. It indicates the cache is growable (chained), which also
	 * implies it contains cachelets. However, the cache may contain cachelets even
	 * if this flag is not set.
	 * NOT equivalent to SH_Manager::_isRunningNested.
	 */
	bool _runningNested;

	/* True iff we allow growing the cache via chained supercaches. Set in startup().
	 * _runningNested requests the growing capability, but _growEnabled controls the
	 * support for it.
	 * Currently always false, because cache growing is unstable.
	 * Internal: Requires cachelets.
	 */
	bool _growEnabled;

	/* For growable caches, the cache can only be serialized once, because serialization "corrupts"
	 * the original cache and renders it unusable. e.g. We fix up offsets in AOT methods.
	 * This flag indicates whether the cache has already been serialized.
	 * Access to this is not currently synchronized.
	 */
	bool _isSerialized;

	bool _isAssertEnabled; /* flag to turn on/off assertion before acquiring local mutex */

	SH_Managers * _managers;

	void initialize(OMR_VM* vm, OMRSharedCacheConfig* sharedClassConfig, BlockPtr memForConstructor, const char* cacheName, I_32 newPersistentCacheReqd, bool startupForStats);

	IDATA readCacheUpdates(OMR_VMThread* currentThread);

	IDATA readCache(OMR_VMThread* currentThread, SH_CompositeCacheImpl* cache, IDATA expectedUpdates, bool startupForStats);

	IDATA refreshHashtables(OMR_VMThread* currentThread, bool hasClassSegmentMutex);

  //	ClasspathWrapper* addClasspathToCache(OMR_VMThread* currentThread, ClasspathItem* obj);

  	const OMRUTF8* addScopeToCache(OMR_VMThread* currentThread, const OMRUTF8* scope);

  	const void* addROMClassResourceToCache(OMR_VMThread* currentThread, const void* romAddress, SH_ROMClassResourceManager* localRRM, SH_ROMClassResourceManager::SH_ResourceDescriptor* resourceDescriptor, const char** p_subcstr);

  //	BlockPtr addByteDataToCache(OMR_VMThread* currentThread, SH_Manager* localBDM, const OMRUTF8* tokenKeyInCache, const OMRSharedDataDescriptor* data, SH_CompositeCacheImpl* forceCache, bool writeWithoutMetadata);

  //	J9MemorySegment* addNewROMImageSegment(OMR_VMThread* currentThread, U_8* segmentBase, U_8* segmentEnd);

  //	J9MemorySegment* createNewSegment(OMR_VMThread* currentThread, UDATA type, J9MemorySegmentList* segmentList, U_8* baseAddress, U_8* heapBase, U_8* heapTop, U_8* heapAlloc);

  	const void* storeROMClassResource(OMR_VMThread* currentThread, const void* romAddress, SH_ROMClassResourceManager* localRRM, SH_ROMClassResourceManager::SH_ResourceDescriptor* resourceDescriptor, UDATA forceReplace, const char** p_subcstr);

  	const void* findROMClassResource(OMR_VMThread* currentThread, const void* romAddress, SH_ROMClassResourceManager* localRRM, SH_ROMClassResourceManager::SH_ResourceDescriptor* resourceDescriptor, bool useReadMutex, const char** p_subcstr, UDATA* flags);

  	UDATA updateROMClassResource(OMR_VMThread* currentThread, const void* addressInCache, I_32 updateAtOffset, SH_ROMClassResourceManager* localRRM, SH_ROMClassResourceManager::SH_ResourceDescriptor* resourceDescriptor, const OMRSharedDataDescriptor* data, bool isUDATA, const char** p_subcstr);

  	const U_8* findAttachedData(OMR_VMThread* currentThread, const void* addressInCache, OMRSharedDataDescriptor* data, IDATA *corruptOffset, const char** p_subcstr) ;

//  	void updateROMSegmentList(OMR_VMThread* currentThread, bool hasClassSegmentMutex);

//  	void updateROMSegmentListForCache(OMR_VMThread* currentThread, SH_CompositeCacheImpl* forCache);

	const char* attachedTypeString(UDATA type);

  //	UDATA initializeROMSegmentList(OMR_VMThread* currentThread);

	IDATA checkForCrash(OMR_VMThread* currentThread, bool hasClassSegmentMutex);

	void reportCorruptCache(OMR_VMThread* currentThread);

	void resetCorruptState(OMR_VMThread* currentThread, UDATA hasRefreshMutex);

	IDATA enterRefreshMutex(OMR_VMThread* currentThread, const char* caller);
	IDATA exitRefreshMutex(OMR_VMThread* currentThread, const char* caller);
	IDATA enterReentrantLocalMutex(OMR_VMThread* currentThread, omrthread_monitor_t monitor, const char* name, const char* caller);
	IDATA exitReentrantLocalMutex(OMR_VMThread* currentThread, omrthread_monitor_t monitor, const char* name, const char* caller);

  //	UDATA sanityWalkROMClassSegment(OMR_VMThread* currentThread, SH_CompositeCacheImpl* cache);

	void updateBytesRead(UDATA numBytes);

	const OMRUTF8* getCachedUTFString(OMR_VMThread* currentThread, const char* local, U_16 localLen);

	IDATA printAllCacheStats(OMR_VMThread* currentThread, UDATA showFlags, SH_CompositeCacheImpl* cache, U_32* staleBytes);

	IDATA resetAllManagers(OMR_VMThread* currentThread);

	void updateAllManagersWithNewCacheArea(OMR_VMThread* currentThread, SH_CompositeCacheImpl* newArea);
	void updateAccessedShrCacheMetadataBounds(OMR_VMThread* currentThread, uintptr_t const  * result);

	SH_CompositeCacheImpl* getCacheAreaForDataType(OMR_VMThread* currentThread, UDATA dataType, UDATA dataLength);

	IDATA startManager(OMR_VMThread* currentThread, SH_Manager* manager);

	SH_ScopeManager* getScopeManager(OMR_VMThread* currentThread);

  //	SH_ClasspathManager* getClasspathManager(OMR_VMThread* currentThread);

  	SH_ByteDataManager* getByteDataManager(OMR_VMThread* currentThread);

	SH_CompiledMethodManager* getCompiledMethodManager(OMR_VMThread* currentThread);

	SH_AttachedDataManager* getAttachedDataManager(OMR_VMThread* currentThread);

	void updateAverageWriteHashTime(UDATA actualTimeMicros);

#if defined(J9SHR_CACHELET_SUPPORT)
	UDATA startAllManagers(OMR_VMThread* currentThread);

  //	void getBoundsForCache(SH_CompositeCacheImpl* cache, BlockPtr* cacheStart, BlockPtr* romClassEnd, BlockPtr* metaStart, BlockPtr* cacheEnd);

	OMRSharedCacheDescriptor* appendCacheDescriptorList(OMR_VMThread* currentThread, OMRSharedCacheConfig* sharedClassConfig);
#endif
	void resetCacheDescriptorList(OMR_VMThread* currentThread, OMRSharedCacheConfig* sharedClassConfig);

#if defined(J9SHR_CACHELET_SUPPORT)
	SH_CompositeCacheImpl* initCachelet(OMR_VMThread* currentThread, BlockPtr cacheletMemory, bool creatingCachelet);

  //	SH_CompositeCacheImpl* createNewCachelet(OMR_VMThread* currentThread);

	IDATA readCacheletHints(OMR_VMThread* currentThread, SH_CompositeCacheImpl* cachelet, CacheletWrapper* cacheletWrapper);
	bool readCacheletSegments(OMR_VMThread* currentThread, SH_CompositeCacheImpl* cachelet, CacheletWrapper* cacheletWrapper);

	IDATA buildCacheletMetadata(OMR_VMThread* currentThread, SH_Manager::CacheletMetadataArray** metadataArray);
	IDATA writeCacheletMetadata(OMR_VMThread* currentThread, SH_Manager::CacheletMetadata* cacheletMetadata);
	void freeCacheletMetadata(OMR_VMThread* currentThread, SH_Manager::CacheletMetadataArray* metaArray);
	void fixupCacheletMetadata(OMR_VMThread* currentThread, SH_Manager::CacheletMetadataArray* metaArray,
		SH_CompositeCacheImpl* serializedCache, IDATA metadataOffset);

	bool serializeOfflineCache(OMR_VMThread* currentThread);

	SH_CompositeCacheImpl* createNewChainedCache(OMR_VMThread* currentThread, UDATA requiredSize);

	void setDeployedROMClassStarts(OMR_VMThread* currentThread, void* serializedROMClassStartAddress);
	IDATA fixupCompiledMethodsForSerialization(OMR_VMThread* currentThread, void* serializedROMClassStartAddress);

#if defined(J9SHR_CACHELETS_SAVE_READWRITE_AREA)
	void fixCacheletReadWriteOffsets(OMR_VMThread* currentThread);
#endif

#if 0
	IDATA growCacheInPlace(OMR_VMThread* currentThread, UDATA rwGrowth, UDATA freeGrowth);
#endif
#endif

  //	const J9ROMClass* allocateROMClassOnly(OMR_VMThread* currentThread, U_32 sizeToAlloc, U_16 classnameLength, const char* classnameData, ClasspathWrapper* cpw, const OMRUTF8* partitionInCache, const OMRUTF8* modContextInCache, IDATA callerHelperID, bool modifiedNoContext, void * &newItemInCache, void * &cacheAreaForAllocate);

  //	const J9ROMClass* allocateFromCache(OMR_VMThread* currentThread, U_32 sizeToAlloc, U_32 wrapperSize, U_16 wrapperType, void * &newItemInCache, void * &cacheAreaForAllocate);

  //	IDATA allocateClassDebugData(OMR_VMThread* currentThread, U_16 classnameLength, const char* classnameData, const J9RomClassRequirements * sizes, J9SharedRomClassPieces * pieces);

  //	void rollbackClassDebugData(OMR_VMThread* currentThread, U_16 classnameLength, const char* classnameData);

  //	void commitClassDebugData(OMR_VMThread* currentThread, U_16 classnameLength, const char* classnameData);

  //	void tokenStoreStaleCheckAndMark(OMR_VMThread* currentThread, U_16 classnameLength, const char* classnameData, ClasspathWrapper* cpw, const OMRUTF8* partitionInCache, const OMRUTF8* modContextInCache, UDATA callerHelperID);

	OMRSharedCacheConfig* getSharedClassConfig();

	void updateLineNumberContentInfo(OMR_VMThread* currentThread);

  	const IDATA aotMethodOperationHelper(OMR_VMThread* currentThread, MethodSpecTable* specTable, IDATA numSpecs, UDATA action);

  	const bool matchAotMethod(MethodSpecTable* specTable, IDATA numSpecs, OMRUTF8* romMethodName, OMRUTF8* romMethodSig);

	const IDATA fillMethodSpecTable(MethodSpecTable* specTable, char* inputOption);

	const bool parseWildcardMethodSpecTable(MethodSpecTable* specTable, IDATA numSpecs);

#if defined(J9SHR_CACHELET_SUPPORT)
	IDATA startupCacheletForStats(OMR_VMThread* currentThread, SH_CompositeCache* cachelet);
#endif /*J9SHR_CACHELET_SUPPORT*/

};

#endif /* !defined(CACHEMAP_H_INCLUDED) */
