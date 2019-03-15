/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef OMR_SHARED_CACHE_INCL
#define OMR_SHARED_CACHE_INCL

#ifndef OMR_SHARED_CACHE_CONNECTOR
#define OMR_SHARED_CACHE_CONNECTOR
namespace OMR { class SharedCache; }
namespace OMR { typedef OMR::SharedCache SharedCacheConnector; }
#endif

#include <stdint.h>
#include "sharedconsts.h"
#include "CacheMap.hpp"

namespace OMR
{

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

class OMRSharedCacheAPI
  {
  private:
    OMRSharedCacheAPI() {}

    void initialize();

    static OMRSharedCacheAPI* instance();
    static OMRSharedCacheAPI* _instance;

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

    friend class OMRSharedCache;
};

typedef struct OMRSharedDataDescriptor {
	U_8* address;
	UDATA length;
	UDATA type;
	UDATA flags;
} OMRSharedDataDescriptor;

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

class SharedCache
   {
   public:

   SharedCache();

   void initialize();

   private:

   SH_CacheMap* cm;

   // OMRSharedCacheConfig* config;
   // OMRSharedCacheDescriptor* desc;

   // static OMRSharedCacheAPI* cacheAPI;
   // OMRSharedInvariantInternTable* internTable;
   // OMRSharedCachePreinitConfig* preinitConfig;
   };
}
#endif
