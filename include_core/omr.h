/*******************************************************************************
 * Copyright (c) 2013, 2019 IBM Corp. and others
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

#ifndef omr_h
#define omr_h

/*
 * @ddr_namespace: default
 */

#include "omrport.h"
#include "j9pool.h"
#include "omrsrp.h"

#include <string.h> // for memcmp

#define OMRPORT_ACCESS_FROM_OMRRUNTIME(omrRuntime) OMRPortLibrary *privateOmrPortLibrary = (omrRuntime)->_portLibrary
#define OMRPORT_ACCESS_FROM_OMRVM(omrVM) OMRPORT_ACCESS_FROM_OMRRUNTIME((omrVM)->_runtime)
#define OMRPORT_ACCESS_FROM_OMRVMTHREAD(omrVMThread) OMRPORT_ACCESS_FROM_OMRVM((omrVMThread)->_vm)

#if defined(J9ZOS390)
#include "edcwccwi.h"
/* Convert function pointer to XPLINK calling convention */
#define OMR_COMPATIBLE_FUNCTION_POINTER(fp) ((void*)__bldxfd(fp))
#else /* J9ZOS390 */
#define OMR_COMPATIBLE_FUNCTION_POINTER(fp) ((void*)(fp))
#endif /* J9ZOS390 */

#if !defined(OMR_GC_COMPRESSED_POINTERS)
#define OMR_GC_FULL_POINTERS
#endif /* defined(J9VM_GC_FULL_POINTERS) */

#ifdef __cplusplus
extern "C" {
#endif

#define OMR_OS_STACK_SIZE	256 * 1024 /* Corresponds to desktopBigStack in builder */

typedef enum {
	OMR_ERROR_NONE = 0,
	OMR_ERROR_OUT_OF_NATIVE_MEMORY,
	OMR_ERROR_FAILED_TO_ATTACH_NATIVE_THREAD,
	OMR_ERROR_MAXIMUM_VM_COUNT_EXCEEDED,
	OMR_ERROR_MAXIMUM_THREAD_COUNT_EXCEEDED,
	OMR_THREAD_STILL_ATTACHED,
	OMR_VM_STILL_ATTACHED,
	OMR_ERROR_FAILED_TO_ALLOCATE_MONITOR,
	OMR_ERROR_INTERNAL,
	OMR_ERROR_ILLEGAL_ARGUMENT,
	OMR_ERROR_NOT_AVAILABLE,
	OMR_THREAD_NOT_ATTACHED,
	OMR_ERROR_FILE_UNAVAILABLE,
	OMR_ERROR_RETRY
} omr_error_t;

struct OMR_Agent;
struct OMR_RuntimeConfiguration;
struct OMR_Runtime;
struct OMR_SysInfo;
struct OMR_TI;
struct OMRTraceEngine;
struct OMR_VMConfiguration;
struct OMR_VM;
struct OMR_VMThread;
struct UtInterface;
struct UtThreadData;
struct OMR_TraceThread;

typedef struct OMR_RuntimeConfiguration {
	uintptr_t _maximum_vm_count;		/* 0 for unlimited */
} OMR_RuntimeConfiguration;

typedef struct OMR_Runtime {
	uintptr_t _initialized;
	OMRPortLibrary *_portLibrary;
	struct OMR_VM *_vmList;
	omrthread_monitor_t _vmListMutex;
	struct OMR_VM *_rootVM;
	struct OMR_RuntimeConfiguration _configuration;
	uintptr_t _vmCount;
} OMR_Runtime;

typedef struct OMR_VMConfiguration {
	uintptr_t _maximum_thread_count;		/* 0 for unlimited */
} OMR_VMConfiguration;

typedef struct movedObjectHashCode {
	uint32_t originalHashCode;
	BOOLEAN hasBeenMoved;
	BOOLEAN hasBeenHashed;
} movedObjectHashCode;

typedef struct OMR_ExclusiveVMAccessStats {
	U_64 startTime;
	U_64 endTime;
	U_64 totalResponseTime;
	struct OMR_VMThread *requester;
	struct OMR_VMThread *lastResponder;
	UDATA haltedThreads;
} OMR_ExclusiveVMAccessStats;
// this is the J9UTF8 struct as defined in genBinaryBlob.hpp (which exists in the guts of OMR,
// but seems to be intended only for use in its DDR interface). 
struct OMRUTF8 {
  uint16_t length;
  uint8_t data[2];
};

typedef struct OMRSharedDataDescriptor {
	U_8* address;
	UDATA length;
	UDATA type;
	UDATA flags;
} OMRSharedDataDescriptor;
  
#define OMRUTF8_LENGTH(j9UTF8Address) (((struct OMRUTF8 *)(j9UTF8Address))->length)
#define OMRUTF8_SET_LENGTH(j9UTF8Address, len) (((struct OMRUTF8 *)(j9UTF8Address))->length = (len))
#define OMRUTF8_DATA(j9UTF8Address) (((struct OMRUTF8 *)(j9UTF8Address))->data)

#define OMRNAMEANDSIG_NAME(base) NNSRP_GET(base->name, struct OMRUTF8*)
#define OMRNAMEANDSIG_SIGNATURE(base) NNSRP_GET(base->signature, struct OMRUTF8*)
  
#define OMRUTF8_DATA_EQUALS(data1, length1, data2, length2) ((((length1) == (length2)) && (memcmp((data1), (data2), (length1)) == 0)))
#define OMRUTF8_EQUALS(utf1, utf2) (((utf1) == (utf2)) || (OMRUTF8_DATA_EQUALS(OMRUTF8_DATA(utf1), OMRUTF8_LENGTH(utf1), OMRUTF8_DATA(utf2), OMRUTF8_LENGTH(utf2))))
#define OMRUTF8_LITERAL_EQUALS(data1, length1, cString) (OMRUTF8_DATA_EQUALS((data1), (length1), (cString), sizeof(cString) - 1))
  
typedef struct OMRSharedCacheDescriptor {
	struct OMRSharedCacheHeader* cacheStartAddress;
	void* romclassStartAddress;
	void* metadataStartAddress;
	UDATA cacheSizeBytes;
	void* deployedROMClassStartAddress;
	struct OMRSharedCacheDescriptor* next;
} OMRSharedCacheDescriptor;

/* @ddr_namespace: map_to_type=OMRSharedInternSRPHashTableEntry */

typedef struct OMRSharedInternSRPHashTableEntry {
	J9SRP utf8SRP;
	U_16 flags;
	U_16 internWeight;
	J9SRP prevNode;
	J9SRP nextNode;
} OMRSharedInternSRPHashTableEntry;

typedef struct MethodNameAndSignature {
	J9SRP name;
	J9SRP signature;
} MethodNameAndSignature;

typedef struct OMRSharedInvariantInternTable {
	UDATA  ( *performNodeAction)(struct OMRSharedInvariantInternTable *sharedInvariantInternTable, struct OMRSharedInternSRPHashTableEntry *node, UDATA action, void* userData) ;
	UDATA flags;
	omrthread_monitor_t tableInternFxMutex;
	struct J9SRPHashTable* sharedInvariantSRPHashtable;
	struct OMRSharedInternSRPHashTableEntry* headNode;
	struct OMRSharedInternSRPHashTableEntry* tailNode;
	J9SRP* sharedTailNodePtr;
	J9SRP* sharedHeadNodePtr;
	U_32* totalSharedNodesPtr;
	U_32* totalSharedWeightPtr;
//	struct J9ClassLoader* systemClassLoader;
} J9SharedInvariantInternTable;

#define STRINGINTERNTABLES_NODE_FLAG_UTF8_IS_SHARED  4
#define STRINGINTERNTABLES_ACTION_VERIFY_BOTH_TABLES  10
#define STRINGINTERNTABLES_ACTION_VERIFY_LOCAL_TABLE_ONLY  13

#define J9SHAREDINTERNSRPHASHTABLEENTRY_UTF8SRP(base) SRP_GET((base)->utf8SRP, struct J9UTF8*)
#define J9SHAREDINTERNSRPHASHTABLEENTRY_PREVNODE(base) SRP_GET((base)->prevNode, struct OMRSharedInternSRPHashTableEntry*)
#define J9SHAREDINTERNSRPHASHTABLEENTRY_NEXTNODE(base) SRP_GET((base)->nextNode, struct OMRSharedInternSRPHashTableEntry*)

#define J9SHAREDCACHEHEADER_UPDATECOUNTPTR(base) WSRP_GET((base)->updateCountPtr, UDATA*)
#define J9SHAREDCACHEHEADER_LOCKEDPTR(base) WSRP_GET((base)->lockedPtr, U_32*)
#define J9SHAREDCACHEHEADER_CORRUPTFLAGPTR(base) WSRP_GET((base)->corruptFlagPtr, U_8*)
#define J9SHAREDCACHEHEADER_SHAREDSTRINGHEAD(base) SRP_GET((base)->sharedStringHead, struct OMRSharedInternSRPHashTableEntry*)
#define J9SHAREDCACHEHEADER_SHAREDSTRINGTAIL(base) SRP_GET((base)->sharedStringTail, struct OMRSharedInternSRPHashTableEntry*)
#define J9SHAREDCACHEHEADER_UNUSED01(base) SRP_GET((base)->unused01, void*)

typedef struct J9ShrCompositeCacheCommonInfo {
	omrthread_tls_key_t writeMutexEntryCount;
	struct OMR_VMThread* hasWriteMutexThread;
	struct OMR_VMThread* hasReadWriteMutexThread;
	struct OMR_VMThread* hasRefreshMutexThread;
	struct OMR_VMThread* hasRWMutexThreadMprotectAll;
	U_16 vmID;
	U_32 writeMutexID;
	U_32 readWriteAreaMutexID;
	UDATA cacheIsCorrupt;
	UDATA stringTableStarted;
	UDATA oldWriterCount;
} J9ShrCompositeCacheCommonInfo;

typedef struct OMRSharedCacheConfig {
 	void* sharedClassCache;
        OMRSharedCacheDescriptor* cacheDescriptorList;
   // all of this being part of the SharedCache.. yeah, I dunno. obviously, they are
   // java specific. we do not need them here.
 //	omrthread_monitor_t jclCacheMutex;
 //	struct J9Pool* jclClasspathCache;
 //	struct J9Pool* jclURLCache;
 //	struct J9Pool* jclTokenCache;
 //	struct J9HashTable* jclURLHashTable;
 //	struct J9HashTable* jclUTF8HashTable;
 //	struct J9Pool* jclJ9ClassPathEntryPool;
 //	struct J9SharedStringFarm* jclStringFarm;
 //	struct J9ClassPathEntry* lastBootstrapCPE;

   // this, I think, is more java-related stuff that ought not to be preserved here.
 //	void* bootstrapCPI;
 	U_64 runtimeFlags;
 	UDATA verboseFlags;
 //	UDATA findClassCntr;
   // we want monitor-based concurrency support, which omr already has. clearly.
 	omrthread_monitor_t configMonitor;
   // I guess because of the comment, we'll keep it?
 	UDATA configLockWord; /* The VM no longer uses this field, but the z/OS JIT doesn't build without it */
 	const struct J9UTF8* modContext;
        void* sharedAPIObject;
 	const char* ctrlDirName;
 	UDATA  ( *getCacheSizeBytes)(struct OMR_VM* vm) ;
 	UDATA  ( *getTotalUsableCacheBytes)(struct OMR_VM* vm);
 	void  ( *getMinMaxBytes)(struct OMR_VM* vm, U_32 *softmx, I_32 *minAOT, I_32 *maxAOT, I_32 *minJIT, I_32 *maxJIT);
 	I_32  ( *setMinMaxBytes)(struct OMR_VM* vm, U_32 softmx, I_32 minAOT, I_32 maxAOT, I_32 minJIT, I_32 maxJIT);
 	void (* increaseUnstoredBytes)(struct OMR_VM *vm, U_32 aotBytes, U_32 jitBytes);
 	void (* getUnstoredBytes)(struct OMR_VM *vm, U_32 *softmxUnstoredBytes, U_32 *maxAOTUnstoredBytes, U_32 *maxJITUnstoredBytes);
 	UDATA  ( *getFreeSpaceBytes)(struct OMR_VM* vm) ;
 	IDATA  ( *findSharedData)(struct OMR_VMThread* currentThread, const char* key, UDATA keylen, UDATA limitDataType, UDATA includePrivateData, OMRSharedDataDescriptor* firstItem, const J9Pool* descriptorPool) ;
 	const U_8*  ( *storeSharedData)(struct OMR_VMThread* vmThread, const char* key, UDATA keylen, const OMRSharedDataDescriptor* data) ;
 	UDATA  ( *storeAttachedData)(struct OMR_VMThread* vmThread, const void* addressInCache, const OMRSharedDataDescriptor* data, UDATA forceReplace) ;
 	const U_8*  ( *findAttachedData)(struct OMR_VMThread* vmThread, const void* addressInCache, OMRSharedDataDescriptor* data, IDATA *dataIsCorrupt) ;
 	UDATA  ( *updateAttachedData)(struct OMR_VMThread* vmThread, const void* addressInCache, I_32 updateAtOffset, const OMRSharedDataDescriptor* data) ;
 	UDATA  ( *updateAttachedUDATA)(struct OMR_VMThread* vmThread, const void* addressInCache, UDATA type, I_32 updateAtOffset, UDATA value) ;
 	void  ( *freeAttachedDataDescriptor)(struct OMR_VMThread* vmThread, OMRSharedDataDescriptor* data) ;
  const U_8*  ( *findCompiledMethodEx1)(struct OMR_VMThread* vmThread, const MethodNameAndSignature* methodNameAndSignature, UDATA* flags);
 	const U_8*  ( *storeCompiledMethod)(struct OMR_VMThread* vmThread, const MethodNameAndSignature* methodNameAndSignature, const U_8* dataStart, UDATA dataSize, const U_8* codeStart, UDATA codeSize, UDATA forceReplace) ;
 	UDATA  ( *existsCachedCodeForROMMethod)(struct OMR_VMThread* vmThread, const MethodNameAndSignature* methodNameAndSignature) ;
 	UDATA  ( *acquirePrivateSharedData)(struct OMR_VMThread* vmThread, const struct OMRSharedDataDescriptor* data) ;
 	UDATA  ( *releasePrivateSharedData)(struct OMR_VMThread* vmThread, const struct OMRSharedDataDescriptor* data) ;
  // 	UDATA  ( *getJavacoreData)(struct OMR_VM *vm, struct J9SharedClassJavacoreDataDescriptor* descriptor) ;
 	UDATA  ( *isBCIEnabled)(struct OMR_VM *vm) ;
  //	void  ( *freeClasspathData)(struct OMR_VM *vm, void *cpData) ;
  //	void  ( *jvmPhaseChange)(struct OMR_VMThread *currentThread, UDATA phase);
 	struct J9MemorySegment* metadataMemorySegment;
 	J9Pool* classnameFilterPool;
 	U_32 softMaxBytes;
 	I_32 minAOT;
 	I_32 maxAOT;
 	I_32 minJIT;
 	I_32 maxJIT;
} OMRSharedCacheConfig;

typedef struct OMRSharedCachePreinitConfig {
	UDATA sharedClassCacheSize;
	IDATA sharedClassInternTableNodeCount;
	IDATA sharedClassMinAOTSize;
	IDATA sharedClassMaxAOTSize;
	IDATA sharedClassMinJITSize;
	IDATA sharedClassMaxJITSize;
	IDATA sharedClassReadWriteBytes;
	IDATA sharedClassDebugAreaBytes;
	IDATA sharedClassSoftMaxBytes;
} OMRSharedCachePreinitConfig;
  
typedef struct OMRSharedCacheAPI
{
    char* ctrlDirName;
    char* cacheName;
    char* modContext;
    char* expireTime;
    U_64 runtimeFlags;
    UDATA verboseFlags;
    UDATA cacheType;
    UDATA parseResult;
    UDATA storageKeyTesting;
    UDATA xShareClassesPresent;
    UDATA cacheDirPerm;

//    IDATA  ( *iterateSharedCaches)(struct OMR_VM *vm, const char *cacheDir, UDATA groupPerm, BOOLEAN useCommandLineValues, IDATA (*callback)(struct OMR_VM *vm, OMRSharedCacheInfo *event_data, void *user_data), void *user_data) ;
//    IDATA  ( *destroySharedCache)(struct OMR_VM *vm, const char *cacheDir, const char *name, U_32 cacheType, BOOLEAN useCommandLineValues) ;
    UDATA printStatsOptions;
    char* methodSpecs;
    U_32 softMaxBytes;
    I_32 minAOT;
    I_32 maxAOT;
    I_32 minJIT;
    I_32 maxJIT;
    U_8 sharedCacheEnabled;
} OMRSharedCacheAPI;

#define OMRSHRDATA_IS_PRIVATE  1
#define OMRSHRDATA_ALLOCATE_ZEROD_MEMORY  2
#define OMRSHRDATA_PRIVATE_TO_DIFFERENT_JVM  4
#define OMRSHRDATA_USE_READWRITE  8
#define OMRSHRDATA_NOT_INDEXED  16
#define OMRSHRDATA_SINGLE_STORE_FOR_KEY_TYPE  32

typedef struct OMRSharedCacheInfo {
	const char* name;
	UDATA isCompatible;
	UDATA cacheType;
	UDATA os_shmid;
	UDATA os_semid;
	UDATA modLevel;
	UDATA addrMode;
	UDATA isCorrupt;
	UDATA cacheSize;
	UDATA freeBytes;
	I_64 lastDetach;
	UDATA softMaxBytes;
} OMRSharedCacheInfo;

typedef struct OMRSharedCacheHeader {
	U_32 totalBytes;
	U_32 readWriteBytes;
	UDATA updateSRP;
	UDATA readWriteSRP;
	UDATA segmentSRP;
	UDATA updateCount;
	J9WSRP updateCountPtr;
	volatile UDATA readerCount;
	UDATA unused2;
	UDATA writeHash;
	UDATA unused3;
	UDATA unused4;
	UDATA crashCntr;
	UDATA aotBytes;
	UDATA jitBytes;
	U_16 vmCntr;
	U_8 corruptFlag;
	U_8 roundedPagesFlag;
	I_32 minAOT;
	I_32 maxAOT;
	U_32 locked;
	J9WSRP lockedPtr;
	J9WSRP corruptFlagPtr;
	J9SRP sharedStringHead;
	J9SRP sharedStringTail;
	J9SRP unused1;
	U_32 totalSharedStringNodes;
	U_32 totalSharedStringWeight;
	U_32 readWriteFlags;
	UDATA readWriteCrashCntr;
	UDATA readWriteRebuildCntr;
	UDATA osPageSize;
	UDATA ccInitComplete;
	UDATA crcValid;
	UDATA crcValue;
	UDATA containsCachelets;
	UDATA cacheFullFlags;
	UDATA readWriteVerifyCntr;
	UDATA extraFlags;
	UDATA debugRegionSize;
	UDATA lineNumberTableNextSRP;
	UDATA localVariableTableNextSRP;
	I_32 minJIT;
	I_32 maxJIT;
	IDATA sharedInternTableBytes;
	IDATA corruptionCode;
	UDATA corruptValue;
	UDATA lastMetadataType;
	UDATA writerCount;
	UDATA unused5;
	UDATA unused6;
	U_32 softMaxBytes;
	UDATA unused8;
	UDATA unused9;
	UDATA unused10;
} OMRSharedCacheHeader;

//typedef struct J9SharedClassTransaction {
//	struct OMR_VMThread* ownerThread;
//  //	struct J9ClassLoader* classloader;
//	I_16 entryIndex;
//	UDATA loadType;
//	U_16 classnameLength;
//	U_8* classnameData;
//	UDATA transactionState;
//	IDATA isOK;
//	struct OMRUTF8* partitionInCache;
//	struct OMRUTF8* modContextInCache;
//	IDATA helperID;
//	void* allocatedMem;
//	U_32 allocatedLineNumberTableSize;
//	U_32 allocatedLocalVariableTableSize;
//	void* allocatedLineNumberTable;
//	void* allocatedLocalVariableTable;
//	void* ClasspathWrapper;
//	void* cacheAreaForAllocate;
//	void* newItemInCache;
//  //	j9object_t updatedItemSize;
//	void* findNextRomClass;
//	void* findNextIterator;
//	void* firstFound;
//	UDATA oldVMState;
//	UDATA isModifiedClassfile;
//	UDATA takeReadWriteLock;
//} J9SharedClassTransaction;
//
typedef struct J9SharedStringTransaction {
	struct OMR_VMThread* ownerThread;
	UDATA transactionState;
	IDATA isOK;
} J9SharedStringTransaction;

/* @ddr_namespace: map_to_type=J9GenericByID */

typedef struct J9GenericByID {
	U_8 magic;
	U_8 type;
	U_16 id;
	struct J9ClassPathEntry* jclData;
	void* cpData;
} J9GenericByID;

typedef struct OMR_VM {
	struct OMR_Runtime *_runtime;
	void *_language_vm;
#if defined(OMR_GC_COMPRESSED_POINTERS) && defined(OMR_GC_FULL_POINTERS)
	uintptr_t _compressObjectReferences;
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) && defined(OMR_GC_FULL_POINTERS) */
	struct OMR_VM *_linkNext;
	struct OMR_VM *_linkPrevious;
	struct OMR_VMThread *_vmThreadList;
	omrthread_monitor_t _vmThreadListMutex;
	omrthread_tls_key_t _vmThreadKey;
	uintptr_t _arrayletLeafSize;
	uintptr_t _arrayletLeafLogSize;
	uintptr_t _compressedPointersShift;
	uintptr_t _objectAlignmentInBytes;
	uintptr_t _objectAlignmentShift;
	void *_gcOmrVMExtensions;
	struct OMR_VMConfiguration _configuration;
	uintptr_t _languageThreadCount;
	uintptr_t _internalThreadCount;
	struct OMR_ExclusiveVMAccessStats exclusiveVMAccessStats;
	uintptr_t gcPolicy;
	struct OMR_SysInfo *sysInfo;
	struct OMR_SizeClasses *_sizeClasses;
#if defined(OMR_THR_FORK_SUPPORT)
	uintptr_t forkGeneration;
	uintptr_t parentPID;
#endif /* defined(OMR_THR_FORK_SUPPORT) */

#if defined(OMR_RAS_TDF_TRACE)
	struct UtInterface *utIntf;
	struct OMR_Agent *_hcAgent;
	omrthread_monitor_t _omrTIAccessMutex;
	struct OMRTraceEngine *_trcEngine;
	void *_methodDictionary;
#endif /* OMR_RAS_TDF_TRACE */
        struct OMRSharedCacheConfig* sharedClassConfig;
        struct OMRSharedCacheAPI* sharedCacheAPI;
    	struct OMRSharedInvariantInternTable* sharedInvariantInternTable;
        struct OMRSharedCachePreinitConfig* sharedCachePreinitConfig;
#if defined(OMR_GC_REALTIME)
	omrthread_monitor_t _gcCycleOnMonitor;
	uintptr_t _gcCycleOn;
#endif /* defined(OMR_GC_REALTIME) */
} OMR_VM;

#if defined(OMR_GC_COMPRESSED_POINTERS)
#if defined(OMR_GC_FULL_POINTERS)
#define OMRVM_COMPRESS_OBJECT_REFERENCES(omrVM) (0 != (omrVM)->_compressObjectReferences)
#else /* OMR_GC_FULL_POINTERS */
#define OMRVM_COMPRESS_OBJECT_REFERENCES(omrVM) TRUE
#endif /* OMR_GC_FULL_POINTERS */
#else /* OMR_GC_COMPRESSED_POINTERS */
#define OMRVM_COMPRESS_OBJECT_REFERENCES(omrVM) FALSE
#endif /* OMR_GC_COMPRESSED_POINTERS */

typedef struct OMR_VMThread {
	struct OMR_VM *_vm;
	uint32_t _sampleStackBackoff;
#if defined(OMR_GC_COMPRESSED_POINTERS) && defined(OMR_GC_FULL_POINTERS)
	uint32_t _compressObjectReferences;
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) && defined(OMR_GC_FULL_POINTERS) */
	void *_language_vmthread;
	omrthread_t _os_thread;
	struct OMR_VMThread *_linkNext;
	struct OMR_VMThread *_linkPrevious;
	uintptr_t _internal;
	void *_gcOmrVMThreadExtensions;

	uintptr_t vmState;
	uintptr_t exclusiveCount;

	uint8_t *threadName;
	BOOLEAN threadNameIsStatic; /**< threadName is managed externally; Don't free it. */
	omrthread_monitor_t threadNameMutex; /**< Hold this mutex to read or modify threadName. */

#if defined(OMR_RAS_TDF_TRACE)
	union {
		struct UtThreadData *uteThread; /* used by JVM */
		struct OMR_TraceThread *omrTraceThread; /* used by OMR */
	} _trace;
#endif /* OMR_RAS_TDF_TRACE */

	/* todo: dagar these are temporarily duplicated and should be removed from J9VMThread */
	void *lowTenureAddress;
	void *highTenureAddress;

	void *heapBaseForBarrierRange0;
	uintptr_t heapSizeForBarrierRange0;

	void *memorySpace;

	struct movedObjectHashCode movedObjectHashCodeCache;

	int32_t _attachCount;

	void *_savedObject1; /**< holds new object allocation until object can be attached to reference graph (see MM_AllocationDescription::save/restoreObjects()) */
	void *_savedObject2; /**< holds new object allocation until object can be attached to reference graph (see MM_AllocationDescription::save/restoreObjects()) */
} OMR_VMThread;

#if defined(OMR_GC_COMPRESSED_POINTERS)
#if defined(OMR_GC_FULL_POINTERS)
#define OMRVMTHREAD_COMPRESS_OBJECT_REFERENCES(omrVMThread) (0 != (omrVMThread)->_compressObjectReferences)
#else /* OMR_GC_FULL_POINTERS */
#define OMRVMTHREAD_COMPRESS_OBJECT_REFERENCES(omrVMThread) TRUE
#endif /* OMR_GC_FULL_POINTERS */
#else /* OMR_GC_COMPRESSED_POINTERS */
#define OMRVMTHREAD_COMPRESS_OBJECT_REFERENCES(omrVMThread) FALSE
#endif /* OMR_GC_COMPRESSED_POINTERS */

/**
 * Perform basic structural initialization of the OMR runtime
 * (allocating monitors, etc).
 *
 * @param[in] *runtime the runtime to initialize
 *
 * @return an OMR error code
 */
omr_error_t omr_initialize_runtime(OMR_Runtime *runtime);

/**
 * Perform final destruction of the OMR runtime.
 * All VMs be detached before calling.
 *
 * @param[in] *runtime the runtime to destroy
 *
 * @return an OMR error code
 */
omr_error_t omr_destroy_runtime(OMR_Runtime *runtime);

/**
 * Attach an OMR VM to the runtime.
 *
 * @param[in] *vm the VM to attach
 *
 * @return an OMR error code
 */
omr_error_t omr_attach_vm_to_runtime(OMR_VM *vm);

/**
 * Detach an OMR VM from the runtime.
 * All language threads must be detached before calling.
 *
 * @param[in] vm the VM to detach
 *
 * @return an OMR error code
 */
omr_error_t omr_detach_vm_from_runtime(OMR_VM *vm);

/**
 * Attach an OMR VMThread to the VM.
 *
 * Used by JVM and OMR tests, but not used by OMR runtimes.
 *
 * @param[in,out] vmthread The vmthread to attach. NOT necessarily the current thread.
 *                         vmthread->_os_thread must be initialized.
 *
 * @return an OMR error code
 */
omr_error_t omr_attach_vmthread_to_vm(OMR_VMThread *vmthread);

/**
 * Detach a OMR VMThread from the VM.
 *
 * Used by JVM and OMR tests, but not used by OMR runtimes.
 *
 * @param[in,out] vmthread The vmthread to detach. NOT necessarily the current thread.
 *
 * @return an OMR error code
 */
omr_error_t omr_detach_vmthread_from_vm(OMR_VMThread *vmthread);

/**
 * Initialize an OMR VMThread.
 * This should be done before attaching the thread to the OMR VM.
 * The caller thread must be attached to omrthread.
 *
 * @param[in,out] vmthread a new vmthread
 * @return an OMR error code
 */
omr_error_t omr_vmthread_init(OMR_VMThread *vmthread);

/**
 * Destroy an OMR VMThread. Free associated data structures.
 * This should be done after detaching the thread from the OMR VM.
 * The caller thread must be attached to omrthread.
 *
 * Does not free the vmthread.
 *
 * @param[in,out] vmthread the vmthread to cleanup
 */
void omr_vmthread_destroy(OMR_VMThread *vmthread);

/**
 * @brief Attach the current thread to an OMR VM.
 *
 * @pre The current thread must not be attached to the OMR VM.
 * @pre The current thread must be omrthread_attach()ed.
 *
 * Allocates and initializes a new OMR_VMThread for the current thread.
 * The thread should be detached using omr_vmthread_lastDetach().
 *
 * Not currently used by JVM.
 *
 * @param[in]  vm The OMR VM.
 * @param[out] vmThread A new OMR_VMThread for the current thread.
 * @return an OMR error code
 */
omr_error_t omr_vmthread_firstAttach(OMR_VM *vm, OMR_VMThread **vmThread);

/**
 * @brief Detach a thread from its OMR VM.
 *
 * @pre The thread being detached must not have any unbalanced re-attaches.
 * @pre The current thread must be omrthread_attach()ed.
 *
 * Detaches and destroys the OMR_VMThread.
 * The thread should have been initialized using omr_vmthread_firstAttach().
 *
 * This function might be used to detach another thread that is already dead.
 *
 * Not currently used by JVM.
 *
 * @param[in,out] vmThread The thread to detach. If the thread is already dead, it won't be the current thread. It will be freed.
 * @return an OMR error code
 */
omr_error_t omr_vmthread_lastDetach(OMR_VMThread *vmThread);

/**
 * @brief Re-attach a thread that is already attached to the OMR VM.
 *
 * This increments an attach count instead of allocating a new OMR_VMThread.
 * Re-attaches must be paired with an equal number of omr_vmthread_redetach()es.
 *
 * @param[in,out] currentThread The current OMR_VMThread.
 * @param[in] threadName A new name for the thread.
 */
void omr_vmthread_reattach(OMR_VMThread *currentThread, const char *threadName);

/**
 * @brief Detach a thread that has been re-attached multiple times.
 *
 * This decrements an attach count.
 * Re-detaches must be paired with an equal number of omr_vmthread_reattach()es.
 *
 * @param[in,out] omrVMThread An OMR_VMThread. It must have been re-attached at least once.
 */
void omr_vmthread_redetach(OMR_VMThread *omrVMThread);


/**
 * Get the current OMR_VMThread, if the current thread is attached.
 *
 * This function doesn't prevent another thread from maliciously freeing OMR_VMThreads
 * that are still in use.
 *
 * @param[in] vm The VM
 * @return A non-NULL OMR_VMThread if the current thread is already attached, NULL otherwise.
 */
OMR_VMThread *omr_vmthread_getCurrent(OMR_VM *vm);

/*
 * C wrappers for OMR_Agent API
 */
/**
 * @see OMR_Agent::createAgent
 */
struct OMR_Agent *omr_agent_create(OMR_VM *vm, char const *arg);

/**
 * @see OMR_Agent::destroyAgent
 */
void omr_agent_destroy(struct OMR_Agent *agent);

/**
 * @see OMR_Agent::openLibrary
 */
omr_error_t omr_agent_openLibrary(struct OMR_Agent *agent);

/**
 * @see OMR_Agent::callOnLoad
 */
omr_error_t omr_agent_callOnLoad(struct OMR_Agent *agent);

/**
 * @see OMR_Agent::callOnUnload
 */
omr_error_t omr_agent_callOnUnload(struct OMR_Agent *agent);

/**
 * Access the TI function table.
 *
 * @return the TI function table
 */
struct OMR_TI const *omr_agent_getTI(void);

#if defined(OMR_THR_FORK_SUPPORT)

/**
 * To be called directly after a fork in the child process, this function will reset the OMR_VM,
 * including cleaning up the _vmThreadList of threads that do not exist after a fork.
 *
 * @param[in] vm The OMR vm.
 */
void omr_vm_postForkChild(OMR_VM *vm);

/**
 * To be called directly after a fork in the parent process, this function releases the
 * _vmThreadListMutex.
 *
 * @param[in] vm The OMR vm.
 */
void omr_vm_postForkParent(OMR_VM *vm);

/**
 * To be called directly before a fork, this function will hold the _vmThreadListMutex to
 * ensure that the _vmThreadList is in a consistent state during a fork.
 *
 * @param[in] vm The OMR vm.
 */
void omr_vm_preFork(OMR_VM *vm);

#endif /* defined(OMR_THR_FORK_SUPPORT) */




/*
 * LANGUAGE VM GLUE
 * The following functions must be implemented by the language VM.
 */
/**
 * @brief Bind the current thread to a language VM.
 *
 * As a side-effect, the current thread will also be bound to the OMR VM.
 * A thread can bind itself multiple times.
 * Binds must be paired with an equal number of Unbinds.
 *
 * @param[in] omrVM the OMR vm
 * @param[in] threadName An optional name for the thread. May be NULL.
 * 	 It is the responsibility of the caller to ensure this string remains valid for the lifetime of the thread.
 * @param[out] omrVMThread the current OMR VMThread
 * @return an OMR error code
 */
omr_error_t OMR_Glue_BindCurrentThread(OMR_VM *omrVM, const char *threadName, OMR_VMThread **omrVMThread);

/**
 * @brief Unbind the current thread from its language VM.
 *
 * As a side-effect, the current thread will also be unbound from the OMR VM.
 * Unbinds must be paired with an equal number of binds.
 * When the bind count of a thread reaches 0, the OMR VMThread
 * is freed, and can no longer be used.
 *
 * @param[in,out] omrVMThread the current OMR VMThread
 * @return an OMR error code
 */
omr_error_t OMR_Glue_UnbindCurrentThread(OMR_VMThread *omrVMThread);

/**
 * @brief Allocate and initialize a new language thread.
 *
 * Allocate a new language thread, perform language-specific initialization,
 * and attach it to the language VM.
 *
 * The new thread is probably not the current thread. Don't perform initialization
 * that requires the new thread to be currently executing, such as setting TLS values.
 *
 * Don't invoke OMR_Thread_Init(), or attempt to allocate an OMR_VMThread.
 *
 * @param[in] languageVM The language VM to attach the new thread to.
 * @param[out] languageThread A new language thread.
 * @return an OMR error code
 */
omr_error_t OMR_Glue_AllocLanguageThread(void *languageVM, void **languageThread);

/**
 * @brief Cleanup and free a language thread.
 *
 * Perform the reverse of OMR_Glue_AllocLanguageThread().
 *
 * The thread being destroyed is probably not the current thread.
 *
 * Don't invoke OMR_Thread_Free().
 *
 * @param[in,out] languageThread The thread to be destroyed.
 * @return an OMR error code
 */
omr_error_t OMR_Glue_FreeLanguageThread(void *languageThread);

/**
 * @brief Link an OMR VMThread to its corresponding language thread.
 *
 * @param[in,out] languageThread The language thread to be modified.
 * @param[in] omrVMThread The OMR_VMThread that corresponds to languageThread.
 * @return an OMR error code
 */
omr_error_t OMR_Glue_LinkLanguageThreadToOMRThread(void *languageThread, OMR_VMThread *omrVMThread);

#if defined(OMR_OS_WINDOWS)
/**
 * @brief Get a platform-dependent token that can be used to locate the VM directory.
 *
 * The token is used by the utility function detectVMDirectory().
 * Although this utility is currently only implemented for Windows, this functionality
 * is also useful for other platforms.
 *
 * For Windows, the token is the name of a module that resides in the VM directory.
 * For AIX/Linux, the token would be the address of a function that resides in a
 * library in the VM directory.
 * For z/OS, no token is necessary because the VM directory is found by searching
 * the LIBPATH.
 *
 * @param[out] token A token.
 * @return an OMR error code
 */
omr_error_t OMR_Glue_GetVMDirectoryToken(void **token);
#endif /* defined(OMR_OS_WINDOWS) */

char *OMR_Glue_GetThreadNameForUnamedThread(OMR_VMThread *vmThread);

/**
 * Get the number of method properties. This is the number of properties per method
 * inserted into the method dictionary. See OMR_Glue_GetMethodDictionaryPropertyNames().
 *
 * @return Number of method properties
 */
int OMR_Glue_GetMethodDictionaryPropertyNum(void);

/**
 * Get the method property names. This should be a constant array of strings identifying
 * the method properties common to all methods to be inserted into the method dictionary
 * for profiling, such as method name, file name, line number, class name, or other properties.
 *
 * @return Method property names
 */
const char * const *OMR_Glue_GetMethodDictionaryPropertyNames(void);

#ifdef __cplusplus
}
#endif

#endif /* omr_h */
