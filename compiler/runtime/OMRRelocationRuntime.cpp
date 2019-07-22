/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#include <algorithm>
//OMRPORT #include "j9cp.h"
#include "omrcfg.h"
#include "omr.h"
//#include "j9consts.h"
#if defined(J9VM_OPT_SHARED_CLASSES)
#include "j9jitnls.h"
#endif

#include <iostream>
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/jittypes.h"
#include "env/CompilerEnv.hpp"
#include "runtime/CodeCache.hpp"
#include "runtime/CodeCacheConfig.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "codegen/FrontEnd.hpp"
#include "infra/Monitor.hpp"
#include "env/PersistentInfo.hpp"
#include "env/VMAccessCriticalSection.hpp"
#include "env/CompilerEnv.hpp"

#include "runtime/RelocationRuntime.hpp"
#include "runtime/RelocationRecord.hpp"
#include "runtime/RelocationRuntimeLogger.hpp"

#include "control/Recompilation.hpp"
#include "runtime/RelocationTarget.hpp"
#include "codegen/CodeGenerator.hpp"
#include "compile/ResolvedMethod.hpp"

OMR::RelocationRuntime::RelocationRuntime(TR::JitConfig*)
{
   //This should be fixed with Options fix
   _options = NULL;
  _reloLogger = new (PERSISTENT_NEW) TR::RelocationRuntimeLogger(self());

   #if defined(TR_HOST_X86)
      #if defined(TR_HOST_64BIT)
      _reloTarget =  (new (PERSISTENT_NEW) TR::RelocationTarget(self()));
      #else
      _reloTarget = new (PERSISTENT_NEW) TR_X86RelocationTarget(this);
      #endif
   #elif defined(TR_HOST_POWER)
      #if defined(TR_HOST_64BIT)
      _reloTarget = new (PERSISTENT_NEW) TR_PPC64RelocationTarget(this);
      #else
      _reloTarget = new (PERSISTENT_NEW) TR_PPC32RelocationTarget(this);
      #endif
   #elif defined(TR_HOST_S390)
      _reloTarget = new (PERSISTENT_NEW) TR_S390RelocationTarget(this);
   #elif defined(TR_HOST_ARM)
      _reloTarget = new (PERSISTENT_NEW) TR_ARMRelocationTarget(this);
   #else
      TR_ASSERT(0, "Unsupported relocation target");
   #endif

   if (_reloTarget == NULL)
      {
      std::cerr<<" Was unable to create a reloTarget, cannot use Relocations"
         << std::endl;
      // TODO: need error condition here
      return;
      }
}

TR::RelocationRuntime * OMR::RelocationRuntime::self()
   {
   return static_cast<TR::RelocationRuntime*>(this);
   }

void
OMR::RelocationRuntime::relocateAOTCodeAndData(U_8 *tempDataStart,
					       U_8 *oldDataStart,
					       U_8 *codeStart,
					       U_8 *oldCodeStart)
{
   UDATA startPC = 0;
   // initializeCacheDeltas();
   // _newMethodCodeStart = codeStart;
   //TODO implement relocate here apply relocations of binaryrelocationGroup should be called here
   reloLogger()->relocationDump();

}

OMRJITExceptionTable *
OMR::RelocationRuntime::prepareRelocateAOTCodeAndData(OMR_VMThread* vmThread,
						      TR_FrontEnd *theFE,
						      TR::CodeCache *aotMCCRuntimeCodeCache,
						      const  OMRSharedCacheHeader *cacheEntry,
						      OMRMethod *theMethod,
						      bool shouldUseCompiledCopy,
						      TR::Options *options,
						      TR::Compilation *compilation,
						      TR_ResolvedMethod *resolvedMethod)
   {
     _currentThread = vmThread;
   _fe = theFE;
   _codeCache = aotMCCRuntimeCodeCache;
   _method = theMethod;
   _useCompiledCopy = shouldUseCompiledCopy;
   _classReloAmount = 0;
   _exceptionTable = NULL;
   _newExceptionTableStart = NULL;
   _relocationStatus = RelocationNoError;
   _haveReservedCodeCache = false; // MCT
   _returnCode = 0;

   _trMemory = comp()->trMemory();
   _currentResolvedMethod = resolvedMethod;


   _options = options;
   TR_ASSERT(_options, "Options were not correctly initialized.");

   uint8_t *tempCodeStart, *tempDataStart;
   uint8_t *oldDataStart, *oldCodeStart, *newCodeStart;
   tempDataStart = (uint8_t *)cacheEntry;

   //Check method header is valid
   _aotMethodHeaderEntry = (TR::AOTMethodHeader*)(cacheEntry + 1); // skip the header J9JITDataCacheHeader
   // if (!aotMethodHeaderVersionsMatch())
   //    return NULL; 

   oldDataStart = NULL;// (U_8 *)_aotMethodHeaderEntry->compileMethodDataStartPC;
   oldCodeStart = NULL;//(U_8 *)_aotMethodHeaderEntry->compileMethodCodeStartPC;

   UDATA dataSize = NULL;// _aotMethodHeaderEntry->compileMethodDataSize;
   UDATA codeSize = NULL;//_aotMethodHeaderEntry->compileMethodCodeSize;

   TR_ASSERT(codeSize > sizeof(OMR::CodeCacheMethodHeader), "codeSize for AOT loads should include the CodeCacheHeader");

     

   _newExceptionTableStart = allocateSpaceInDataCache(10,10);//_exceptionTableCacheEntry->size, _exceptionTableCacheEntry->type);
   tempCodeStart = tempDataStart + dataSize;
   if (_newExceptionTableStart)
     {
       // _exceptionTable = reinterpret_cast<J9JITExceptionTable *>(_newExceptionTableStart + sizeof(J9JITDataCacheHeader)); // Get new exceptionTable location

       // This must be an AOT load because for AOT compilations we relocate in place
       
       // We must prepare the list of assumptions linked to the metadata
       // We could set just a NULL pointer and let the code update that should an
       // assumption be created.
       // Another alternative is to create a sentinel entry right away to avoid
       // having to allocate one at runtime and possibly running out of memory
       // OMR::RuntimeAssumption * raList = new (PERSISTENT_NEW) TR::SentinelRuntimeAssumption();
       // comp->setMetadataAssumptionList(raList); // copy this list to the compilation object as well (same view as for a JIT compilation)
       // _exceptionTable->runtimeAssumptionList = raList;
       // If we cannot allocate the memory, fail the compilation
       //

        
       // newCodeStart points after a OMR::CodeCacheMethodHeader, but tempCodeStart points at a OMR::CodeCacheMethodHeader
       // to keep alignment consistent, back newCodeStart over the OMR::CodeCacheMethodHeader
       //we can still do the code start without the bodyInfo! need check in cleanup!
       newCodeStart = allocateSpaceInCodeCache(codeSize-sizeof(OMR::CodeCacheMethodHeader));
       if (newCodeStart)
	 {
	   TR_ASSERT(_codeCache->isReserved(), "codeCache must be reserved"); // MCT
	   newCodeStart = ((U_8*)newCodeStart) - sizeof(OMR::CodeCacheMethodHeader);
	   // Before copying, memorize the real size of the block returned by the code cache manager
	   // and fix it later
	   U_32 blockSize = ((OMR::CodeCacheMethodHeader*)newCodeStart)->_size;
	   memcpy(newCodeStart, tempCodeStart, codeSize);  // the real size may have been overwritten
	   ((OMR::CodeCacheMethodHeader*)newCodeStart)->_size = blockSize; // fix it
	   // Must fix the pointer to the metadata which is stored in the OMR::CodeCacheMethodHeader
	   //((OMR::CodeCacheMethodHeader*)newCodeStart)->_metaData = NULL;
	 }
       
       if (_relocationStatus == RelocationNoError)
	 {
      initializeAotRuntimeInfo();
      relocateAOTCodeAndData(tempDataStart, oldDataStart, newCodeStart, oldCodeStart);
      }


   if (haveReservedCodeCache())
      codeCache()->unreserve();
   return _exceptionTable;
   }
}
// This whole function can be dealt with more easily by meta-data relocations rather than this specialized function
//   but leave it here for now
void
OMR::RelocationRuntime::relocateMethodMetaData(UDATA codeRelocationAmount, UDATA dataRelocationAmount)
   {


   _exceptionTable->startPC = (UDATA) ( ((U_8 *)_exceptionTable->startPC) + codeRelocationAmount);
   _exceptionTable->endPC = (UDATA) ( ((U_8 *)_exceptionTable->endPC) + codeRelocationAmount);
   _exceptionTable->endWarmPC = (UDATA) ( ((U_8 *)_exceptionTable->endWarmPC) + codeRelocationAmount);
   if (_exceptionTable->startColdPC)
      _exceptionTable->startColdPC = (UDATA) ( ((U_8 *)_exceptionTable->startColdPC) + codeRelocationAmount);

   _exceptionTable->codeCacheAlloc = (UDATA) ( ((U_8 *)_exceptionTable->codeCacheAlloc) + codeRelocationAmount);

   }



bool OMR::RelocationRuntime::_globalValuesInitialized=false;

uintptr_t OMR::RelocationRuntime::_globalValueList[TR_NumGlobalValueItems] =
   {
   0,          // not used
   0,          // TR_CountForRecompile
   0,          // TR_HeapBase
   0,          // TR_HeapTop
   0,          // TR_HeapBaseForBarrierRange0
   0,          // TR_ActiveCardTableBase
   0          // TR_HeapSizeForBarrierRange0
   };

char *OMR::RelocationRuntime::_globalValueNames[TR_NumGlobalValueItems] =
   {
   "not used (0)",
   "TR_CountForRecompile (1)",
   "TR_HeapBase (2)",
   "TR_HeapTop (3)",
   "TR_HeapBaseForBarrierRange0 (4)",
   "TR_ActiveCardTableBase (5)",
   "TR_HeapSizeForBarrierRange0 (6)"
   };




//
// TR_SharedCacheRelocationRuntime
//

const char OMR::SharedCacheRelocationRuntime::aotHeaderKey[] = "J9AOTHeader";
// When we write out the header, we don't seem to include the \0 character at the end of the string.
const UDATA OMR::SharedCacheRelocationRuntime::aotHeaderKeyLength = sizeof(OMR::SharedCacheRelocationRuntime::aotHeaderKey) - 1;

U_8 *
OMR::SharedCacheRelocationRuntime::allocateSpaceInCodeCache(UDATA codeSize)
   {
   // TR_FrontEnd *fej9 = _fe;
   // //TR::CodeCacheManager *manager = TR::CodeCacheManager::instance();

   // int32_t compThreadID = fej9->getCompThreadIDForVMThread(_currentThread);
   // if (!codeCache())
   //    {
   //    int32_t numReserved;

   //    _codeCache = manager->reserveCodeCache(false, codeSize, compThreadID, &numReserved);  // Acquire a cold/warm code cache.
   //    if (!codeCache())
   //       {
   //       // TODO: how do we pass back error codes to trigger retrial?
   //       //if (numReserved >= 1) // We could still get some code space in caches that are currently reserved
   //       //    *returnCode = compilationCodeReservationFailure; // this will promp a retrial
   //       return NULL;
   //       }
   //     // The GC may unload classes if code caches have been switched

   //    if (compThreadID >= 0 && fej9->getCompilationShouldBeInterruptedFlag())
   //       {
   //       codeCache()->unreserve(); // cancel the reservation
   //       //*returnCode = compilationInterrupted; // allow retrial //FIXME: how do we pass error codes?
   //       return NULL; // fail this AOT load
   //       }
   //    _haveReservedCodeCache = true;
   //    }

   // uint8_t *coldCode;
   // U_8 *codeStart = manager->allocateCodeMemory(codeSize, 0, &_codeCache, &coldCode, false);
   // // FIXME: the GC may unload classes if code caches have been switched
   // if (compThreadID >= 0 && fej9->getCompilationShouldBeInterruptedFlag())
   //    {
   //    codeCache()->unreserve(); // cancel the reservation
   //    _haveReservedCodeCache = false;
   //    //*returnCode = compilationInterrupted; // allow retrial
   //    return NULL; // fail this AOT load
   //    }
   // return codeStart;
   }


// TODO: why do shared cache and JXE paths manage alignment differently?
//       main reason why there are two implementations here
uint8_t *
OMR::SharedCacheRelocationRuntime::allocateSpaceInDataCache(uintptr_t metaDataSize,
                                                          uintptr_t type)
   {
   // Ensure data cache is aligned
   // _metaDataAllocSize = TR::DataCacheManager::alignToMachineWord(metaDataSize);
   // U_8 *newDataStart = TR::DataCacheManager::getManager()->allocateDataCacheRecord(_metaDataAllocSize, type, 0);
   // if (newDataStart)
   //    newDataStart -= sizeof(J9JITDataCacheHeader);
   return NULL;
   }


void
OMR::SharedCacheRelocationRuntime::initializeAotRuntimeInfo()
   {
      //TODO implement me
      return;
   }


void
OMR::SharedCacheRelocationRuntime::initializeCacheDeltas()
   {
   _dataCacheDelta = 0;
   _codeCacheDelta = 0;
   }




void
OMR::SharedCacheRelocationRuntime::incompatibleCache(U_32 module_name, U_32 reason, char *assumeMessage)
   {
      //TODO fill me
   }



void
OMR::SharedCacheRelocationRuntime::checkAOTHeaderFlags(TR_FrontEnd *fe, TR::AOTHeader *hdrInCache, intptr_t featureFlags)
   {
      //Checks may follow here
   }



// TR::AOTHeader *
// OMR::SharedCacheRelocationRuntime::createAOTHeader(OMR_VM *omrVM, TR_FrontEnd *fee)
//    {
// //    PORT_ACCESS_FROM_JAVAVM(javaVM());

// //    TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;
// //    TR_AOTHeader * aotHeader = (TR_AOTHeader *)j9mem_allocate_memory(sizeof(TR_AOTHeader), J9MEM_CATEGORY_JIT);

// //    if (aotHeader)
// //       {
// //       aotHeader->eyeCatcher = TR_AOTHeaderEyeCatcher;

// //       TR_Version *aotHeaderVersion = &aotHeader->version;
// //       memset(aotHeaderVersion, 0, sizeof(TR_Version));
// //       aotHeaderVersion->structSize = sizeof(TR_Version);
// //       aotHeaderVersion->majorVersion = TR_AOTHeaderMajorVersion;
// //       aotHeaderVersion->minorVersion = TR_AOTHeaderMinorVersion;
// //       strncpy(aotHeaderVersion->vmBuildVersion, EsBuildVersionString, sizeof(EsBuildVersionString));
// //       strncpy(aotHeaderVersion->jitBuildVersion, TR_BUILD_NAME, std::min(strlen(TR_BUILD_NAME), sizeof(aotHeaderVersion->jitBuildVersion)));

// //       aotHeader->processorSignature = TR::Compiler->target.cpu.id();
// //       aotHeader->gcPolicyFlag = javaVM()->memoryManagerFunctions->j9gc_modron_getWriteBarrierType(javaVM());
// //       aotHeader->lockwordOptionHashValue = getCurrentLockwordOptionHashValue(pjavaVM);
// // #if defined(J9VM_GC_COMPRESSED_POINTERS)
// //       aotHeader->compressedPointerShift = javaVM()->memoryManagerFunctions->j9gc_objaccess_compressedPointersShift(javaVM()->internalVMFunctions->currentVMThread(javaVM()));
// // #else
// //       aotHeader->compressedPointerShift = 0;
// // #endif

// //       aotHeader->processorFeatureFlags = TR::Compiler->target.cpu.getProcessorFeatureFlags();

// //       // Set up other feature flags
// //       aotHeader->featureFlags = generateFeatureFlags(fe);

// //       // Set ArrayLet Size if supported
// //       aotHeader->arrayLetLeafSize = TR::Compiler->om.arrayletLeafSize();
// //       }

//    return NULL;
//    }

// bool
// OMR::SharedCacheRelocationRuntime::storeAOTHeader(OMR_VM *omrVM, TR_FrontEnd *fe, OMR_VMThread *curThread)
//    {

//    TR::AOTHeader *aotHeader = createAOTHeader(omrVM,fe);
//    if (!aotHeader)
//       {
//         return false;
//       }

//    // J9SharedDataDescriptor dataDescriptor;
//    // UDATA aotHeaderLen = sizeof(TR_AOTHeader);

//    // dataDescriptor.address = (U_8*)aotHeader;
//    // dataDescriptor.length = aotHeaderLen;
//    // dataDescriptor.type =  J9SHR_DATA_TYPE_AOTHEADER;
//    // dataDescriptor.flags = J9SHRDATA_SINGLE_STORE_FOR_KEY_TYPE;
//    bool store = false;
//    // const void* store = javaVM()->sharedClassConfig->storeSharedData(curThread,
//    //                                                                aotHeaderKey,
//    //                                                                aotHeaderKeyLength,
//    //                                                                &dataDescriptor);
//    if (store)
//       {
//       // If a header already exists, the old one is returned
//       // Thus, we must check the validity of the header
//       // return validateAOTHeader();
//       return true;
//       }
//    else
//       {
//       // The store failed for some odd reason; maybe the cache is full
//       // Let's prevent any further store operations to avoid overhead
//       // TR::Options::getAOTCmdLineOptions()->setOption(TR_NoStoreAOT);

//       // TR_J9SharedCache::setSharedCacheDisabledReason(TR_J9SharedCache::AOT_HEADER_STORE_FAILED);
//       // TR_J9SharedCache::setStoreSharedDataFailedLength(aotHeaderLen);
//       return false;
//       }
//    }




// TR_YesNoMaybe
// OMR::SharedCacheRelocationRuntime::isMethodInSharedCache(J9Method *method, J9JavaVM *pjavaVM)
//    {
//    if (isRomClassForMethodInSharedCache(method, javaVM()))
//       return TR_maybe;
//    else
//       return TR_no;
//    }

uintptr_t
OMR::SharedCacheRelocationRuntime::generateFeatureFlags(TR_FrontEnd *fe)
   {
//    uintptr_t featureFlags = 0;
//    TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;

//    featureFlags |= TR_FeatureFlag_sanityCheckBegin;

//    if (TR::Compiler->target.isSMP())
//       featureFlags |= TR_FeatureFlag_IsSMP;

//    if (TR::Options::useCompressedPointers())
//       featureFlags |= TR_FeatureFlag_UsesCompressedPointers;

//    if (useDFPHardware(fe))
//       featureFlags |= TR_FeatureFlag_UseDFPHardware;

//    if (TR::Options::getCmdLineOptions()->getOption(TR_DisableTraps))
//       featureFlags |= TR_FeatureFlag_DisableTraps;

//    if (TR::Options::getCmdLineOptions()->getOption(TR_TLHPrefetch))
//       featureFlags |= TR_FeatureFlag_TLHPrefetch;

//    if (TR::CodeCacheManager::instance()->codeCacheConfig().needsMethodTrampolines())
//       featureFlags |= TR_FeatureFlag_MethodTrampolines;

//    if (TR::Options::getCmdLineOptions()->getOption(TR_EnableHCR))
//       featureFlags |= TR_FeatureFlag_HCREnabled;

// #ifdef TR_TARGET_S390
//    if (TR::Compiler->target.cpu.getS390SupportsVectorFacility())
//       featureFlags |= TR_FeatureFlag_SIMDEnabled;
// #endif

//    if (TR::Compiler->om.shouldGenerateReadBarriersForFieldLoads())
//       featureFlags |= TR_FeatureFlag_ConcurrentScavenge;

//    if (TR::Compiler->om.shouldReplaceGuardedLoadWithSoftwareReadBarrier())
//       featureFlags |= TR_FeatureFlag_SoftwareReadBarrier;

//    if (fej9->isAsyncCompilation())
//       featureFlags |= TR_FeatureFlag_AsyncCompilation;

//    return featureFlags;
   }
void *
OMR::SharedCacheRelocationRuntime::methodAddress(char *&methodName)
   {
   std::string method(const_cast<const char *>(methodName));
   return _symbolLocation[method];
   }

void
OMR::SharedCacheRelocationRuntime::registerLoadedMethod(const char *&methodName, void *&methodAddress)
   {
   std::string method(methodName);
   _symbolLocation[methodName] = methodAddress;
   }
  



