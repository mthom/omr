/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#include "runtime/RelocationRuntime.hpp"
#include "runtime/RelocationTarget.hpp"

TR::RelocationRuntime::RelocationRuntime(TR::JitConfig *jitCfg):OMR::RelocationRuntimeConnector(jitCfg)
   {
   _method = NULL;
   _jitConfig = jitCfg;
   _trMemory = NULL;



      _isLoading = false;

#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
      _numValidations = 0;
      _numFailedValidations = 0;
      _numInlinedMethodRelos = 0;
      _numFailedInlinedMethodRelos = 0;
      _numInlinedAllocRelos = 0;
      _numFailedInlinedAllocRelos = 0;
#endif
   }


const char TR::SharedCacheRelocationRuntime::aotHeaderKey[] = "J9AOTHeader";
// When we write out the header, we don't seem to include the \0 character at the end of the string.
const UDATA TR::SharedCacheRelocationRuntime::aotHeaderKeyLength = sizeof(TR::SharedCacheRelocationRuntime::aotHeaderKey) - 1;

U_8 *
TR::SharedCacheRelocationRuntime::allocateSpaceInCodeCache(UDATA codeSize)
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
TR::SharedCacheRelocationRuntime::allocateSpaceInDataCache(uintptr_t metaDataSize,
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
TR::SharedCacheRelocationRuntime::initializeAotRuntimeInfo()
   {
      //TODO implement me
      return;
   }


void
TR::SharedCacheRelocationRuntime::initializeCacheDeltas()
   {
   _dataCacheDelta = 0;
   _codeCacheDelta = 0;
   }




void
TR::SharedCacheRelocationRuntime::incompatibleCache(U_32 module_name, U_32 reason, char *assumeMessage)
   {
      //TODO fill me
   }



void
TR::SharedCacheRelocationRuntime::checkAOTHeaderFlags(TR_FrontEnd *fe, TR::AOTHeader *hdrInCache, intptr_t featureFlags)
   {
      //Checks may follow here
   }



// TR::AOTHeader *
// OMR::SharedCacheRelocationRuntime::createAOTHeader()
//    {
//    PORT_ACCESS_FROM_JAVAVM(javaVM());

//    TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;
//    TR_AOTHeader * aotHeader = (TR_AOTHeader *)j9mem_allocate_memory(sizeof(TR_AOTHeader), J9MEM_CATEGORY_JIT);

//    if (aotHeader)
//       {
//       aotHeader->eyeCatcher = TR_AOTHeaderEyeCatcher;

//       TR_Version *aotHeaderVersion = &aotHeader->version;
//       memset(aotHeaderVersion, 0, sizeof(TR_Version));
//       aotHeaderVersion->structSize = sizeof(TR_Version);
//       aotHeaderVersion->majorVersion = TR_AOTHeaderMajorVersion;
//       aotHeaderVersion->minorVersion = TR_AOTHeaderMinorVersion;
//       strncpy(aotHeaderVersion->vmBuildVersion, EsBuildVersionString, sizeof(EsBuildVersionString));
//       strncpy(aotHeaderVersion->jitBuildVersion, TR_BUILD_NAME, std::min(strlen(TR_BUILD_NAME), sizeof(aotHeaderVersion->jitBuildVersion)));

//       aotHeader->processorSignature = TR::Compiler->target.cpu.id();
//       aotHeader->gcPolicyFlag = javaVM()->memoryManagerFunctions->j9gc_modron_getWriteBarrierType(javaVM());
//       aotHeader->lockwordOptionHashValue = getCurrentLockwordOptionHashValue(pjavaVM);
// #if defined(J9VM_GC_COMPRESSED_POINTERS)
//       aotHeader->compressedPointerShift = javaVM()->memoryManagerFunctions->j9gc_objaccess_compressedPointersShift(javaVM()->internalVMFunctions->currentVMThread(javaVM()));
// #else
//       aotHeader->compressedPointerShift = 0;
// #endif

//       aotHeader->processorFeatureFlags = TR::Compiler->target.cpu.getProcessorFeatureFlags();

//       // Set up other feature flags
//       aotHeader->featureFlags = generateFeatureFlags(fe);

//       // Set ArrayLet Size if supported
//       aotHeader->arrayLetLeafSize = TR::Compiler->om.arrayletLeafSize();
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
TR::SharedCacheRelocationRuntime::generateFeatureFlags(TR_FrontEnd *fe)
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
TR::SharedCacheRelocationRuntime::methodAddress(char *methodName)
   {
   std::string method(const_cast<const char *>(methodName));
   return _symbolLocation[method];
   }

void
TR::SharedCacheRelocationRuntime::registerLoadedMethod(const char *&methodName, void *&methodAddress)
   {
   std::string method(methodName);
   _symbolLocation[methodName] = methodAddress;
   }
