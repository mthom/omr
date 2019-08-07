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

#ifndef RELOCATION_RUNTIME_INCL
#define RELOCATION_RUNTIME_INCL

#include "omrcfg.h"
#include "omr.h"

//OMRPORT #if defined(J9VM_INTERP_AOT_COMPILE_SUPPORT) && defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390))
//OMRPORT   #define TR_SHARED_CACHE_AOT_SE_PLATFORM
//OMRPORT #endif

#include <assert.h>
#include "codegen/Relocation.hpp"
#include "compile/OMRMethod.hpp"
#include "runtime/Runtime.hpp"
//OMRPORT  #include "runtime/HWProfiler.hpp"
#include "env/OMREnvironment.hpp"
#include "env/OMRCPU.hpp" // for TR_ProcessorFeatureFlags
#include "env/JitConfig.hpp" // for JitConfig, it got moved
#include "runtime/OMRRelocationRuntimeTypes.hpp" 
#include "runtime/RelocationRuntimeLogger.hpp"

#ifndef OMR_RELOCATION_RUNTIME_CONNECTOR
#define OMR_RELOCATION_RUNTIME_CONNECTOR
namespace OMR { class RelocationRuntime; }
namespace OMR { typedef OMR::RelocationRuntime RelocationRuntimeConnector; }
#endif
#ifndef OMR_SHARED_RELOCATION_RUNTIME_CONNECTOR
#define OMR_SHARED_RELOCATION_RUNTIME_CONNECTOR
namespace OMR { class SharedCacheRelocationRuntime; }
namespace OMR { typedef OMR::SharedCacheRelocationRuntime SharedCacheRelocationRuntimeConnector; }
#endif

namespace TR { 
   class CompilationInfo; 
   class RelocationRecord;
   class RelocationTarget;
   class RelocationRuntimeLogger;
   class RelocationRecordBinaryTemplate;
   class CodeCacheManager;
// class Resolved method will probably need to be returned back
// when the generic object model is here, since resolved method is one of 
// classes that could be used for abstraction
//  class ResolvedMethod;
   class CodeCache; 
   class PersistentInfo; 
   }

#ifdef __cplusplus
extern "C" {
#endif

/*  AOTHeader Versions:
 *    1.0    Java6 GA
 *    1.1    Java6 SR1
 *    2.1    Java7 Beta
 *    2.2    Java7
 *    2.3    Java7
 *    3.0    Java7.1
 *    3.1    Java7.1
 *    4.1    Java8 (828)
 *    5.0    Java9 (929 AND 829)
 */
//TODO probably worth changing TR_AOTHeaderMajorVersion
// to AOTHeaderMajorVersion
#define TR_AOTHeaderMajorVersion 5
#define TR_AOTHeaderMinorVersion 1
#define TR_AOTHeaderEyeCatcher   0xA0757A27

namespace TR {
/* AOT Header Flags */
typedef enum AOTFeatureFlags
   {
   FeatureFlag_sanityCheckBegin              = 0x00000001,
   FeatureFlag_IsSMP                         = 0x00000002,
   FeatureFlag_UsesCompressedPointers        = 0x00000004,
   FeatureFlag_UseDFPHardware                = 0x00000008,
   FeatureFlag_DisableTraps                  = 0x00000010,
   FeatureFlag_TLHPrefetch                   = 0x00000020,
   FeatureFlag_MethodTrampolines             = 0x00000040,
   FeatureFlag_MultiTenancy                  = 0x00000080,
   FeatureFlag_HCREnabled                    = 0x00000100,
   FeatureFlag_SIMDEnabled                   = 0x00000200,     //set and tested for s390
   FeatureFlag_AsyncCompilation              = 0x00000400,     //async compilation - switch to interpreter code NOT generated
   FeatureFlag_ConcurrentScavenge            = 0x00000800,
   FeatureFlag_SoftwareReadBarrier           = 0x00001000,
   FeatureFlag_SanityCheckEnd                = 0x80000000
   } AOTFeatureFlags;



typedef struct Version {
   uintptr_t structSize;
   uintptr_t majorVersion;
   uintptr_t minorVersion;
   char vmBuildVersion[64];
   char jitBuildVersion[64];
} Version;
typedef struct AOTHeader {
    uintptr_t eyeCatcher;
    Version version;
    uintptr_t *relativeMethodMetaDataTable;
    uintptr_t architectureAndOs;
    uintptr_t endiannessAndWordSize;
    uintptr_t processorSignature;
    uintptr_t featureFlags;
    uintptr_t vendorId;
    uintptr_t gcPolicyFlag;
    uintptr_t compressedPointerShift;
    uint32_t lockwordOptionHashValue;
    int32_t   arrayLetLeafSize;
    OMR::X86::ProcessorFeatureFlags processorFeatureFlags;
} AOTHeader;

typedef struct AOTRuntimeInfo {
    struct AOTHeader* aotHeader;
    struct OMRMemorySegment* codeCache;
    struct OMRMemorySegment* dataCache;
    void* baseJxeAddress;
    uintptr_t compileFirstClassLocation;
    uintptr_t *fe;
} AOTRuntimeInfo;

}
#ifdef __cplusplus
}
#endif
namespace OMR{
class RelocationRuntime {
   public:
      TR_ALLOC(TR_Memory::Relocation)
      void * operator new(size_t, TR::JitConfig *);
      RelocationRuntime(TR::JitConfig *jitCfg,TR::CodeCacheManager* manager);
      TR::RelocationRuntime* self();
      TR::RelocationTarget *reloTarget()                           { return _reloTarget; }
      TR::AOTStats *aotStats()                                     { return _aotStats; }
      TR::JitConfig *jitConfig()                                    { return _jitConfig; }
      TR_FrontEnd *fe()                                           { return _fe; }
      OMR_VM *omrVM()                                          { return _omrVM; }
      TR::AOTMethodHeader* createMethodHeader(uint8_t *codeLocation,
                            uint32_t codeLength,  uint8_t* reloLocation,uint32_t reloSize);
      TR_Memory *trMemory()                                       { return _trMemory; }
      TR::CompilationInfo *compInfo()                              { return _compInfo; }
      OMR_VMThread *currentThread()                                 { return _currentThread; }
      OMRMethod *method()                                          { return _method; }
      TR::CodeCache *codeCache()                                  { return _codeCache; }
      OMRMemorySegment *dataCache()                                { return _dataCache; }
      bool useCompiledCopy()                                      { return _useCompiledCopy; }
      bool haveReservedCodeCache()                                { return _haveReservedCodeCache; }
      TR::RelocationRuntimeLogger* reloLogger()                    { return _reloLogger;}
      OMRJITExceptionTable * exceptionTable()                      { return _exceptionTable; }
      uint8_t * methodCodeStart()                                 { return _newMethodCodeStart; }
      UDATA metaDataAllocSize()                                   { return _metaDataAllocSize; }
      TR::AOTMethodHeader *aotMethodHeaderEntry()                  { return _aotMethodHeaderEntry; }
      OMRSharedCacheHeader *exceptionTableCacheEntry()            { return _exceptionTableCacheEntry; }
      uint8_t *newMethodCodeStart()                               { return _newMethodCodeStart; }
      UDATA codeCacheDelta()                                      { return _codeCacheDelta; }
      UDATA dataCacheDelta()                                      { return _dataCacheDelta; }
      UDATA classReloAmount()                                     { return _classReloAmount; }
      U_8 *baseAddress()                                          { return _baseAddress; }

      UDATA reloStartTime()                                       { return _reloStartTime; }
      void setReloStartTime(UDATA time)                           { _reloStartTime = time; }

      UDATA reloEndTime()                                         { return _reloEndTime; }
      void setReloEndTime(UDATA time)                             { _reloEndTime = time; }

      int32_t returnCode()                                        { return _returnCode; }
      void setReturnCode(int32_t rc)                              { _returnCode = rc; }

      TR::Options *options()                                       { return _options; }
      TR::Compilation* comp()                                    { return _comp; }
      TR_ResolvedMethod *currentResolvedMethod()                  { return _currentResolvedMethod; }

      // current main entry point
      void *prepareRelocateAOTCodeAndData(
                                                         //  OMR_VMThread* vmThread,
                                                         // TR_FrontEnd *fe,
                                                         // TR::CodeCache *aotMCCRuntimeCodeCache,
                                                         TR::AOTMethodHeader *cacheEntry,
                                                         void* code
                                                         // OMRMethod *theMethod,
                                                         // bool shouldUseCompiledCopy,
                                                         // TR::Options *options,
                                                         // TR::Compilation *compilation,a
                                                         // TR_ResolvedMethod *resolvedMethod
                                                         
                                                         );

      // virtual bool storeAOTHeader(OMR_VM *omrVM, TR_FrontEnd *fe, OMR_VMThread *curThread);
      virtual bool storeAotInformation( uint8_t* codeStart, uint32_t codeSize,uint8_t* dataStart,uint32_t dataSize);
      // virtual TR::AOTHeader *createAOTHeader(OMR_VM *omrVM, TR_FrontEnd *fe);
      // virtual bool validateAOTHeader(OMR_VM *jomrVM, TR_FrontEnd *fe, OMR_VMThread *curThread);

    //  virtual void *isROMClassInSharedCaches(UDATA romClassValue, OMR_VM *omrVm);
     // virtual bool isRomClassForMethodInSharedCache(OMRMethod *method, OMR_VM *omrVm);
   //virtual TR_YesNoMaybe isMethodInSharedCache(OMRMethod *method, OMR_VM *omrVM);
     // virtual TR_OpaqueClassBlock *getClassFromCP(OMR_VMThread *vmThread, OMR_VM *omrVM, J9ConstantPool *constantPool, I_32 cpIndex, bool isStatic);

      static uintptr_t    getGlobalValue(uint32_t g)
         {
         TR_ASSERT(g >= 0 && g < TR_NumGlobalValueItems, "invalid index for global item");
         return _globalValueList[g];
         }
      static void         setGlobalValue(uint32_t g, uintptr_t v)
         {
         TR_ASSERT(g >= 0 && g < TR_NumGlobalValueItems, "invalid index for global item");
         _globalValueList[g] = v;
         }
      static char *       nameOfGlobal(uint32_t g)
         {
         TR_ASSERT(g >= 0 && g < TR_NumGlobalValueItems, "invalid index for global item");
         return _globalValueNames[g];
         }

      bool isLoading() { return _isLoading; }
      void setIsLoading() { _isLoading = true; }
      void resetIsLoading() { _isLoading = false; }

 //     void initializeHWProfilerRecords(TR::Compilation *comp);
 //??     void addClazzRecord(uint8_t *ia, uint32_t bcIndex, TR_OpaqueMethodBlock *method);

#if 1 // defined(DEBUG) || defined(PROD_WITH_ASSUMES)
      // Detect unexpected scenarios when build has assumes
      void incNumValidations() { _numValidations++; }
      void incNumFailedValidations() { _numFailedValidations++; }
      void incNumInlinedMethodRelos() { _numInlinedMethodRelos++; }
      void incNumFailedInlinedMethodRelos() { _numFailedInlinedMethodRelos++; }
      void incNumInlinedAllocRelos() { _numInlinedAllocRelos++; }
      void incNumFailedAllocInlinedRelos() { _numFailedInlinedAllocRelos++; }

      uint32_t getNumValidations() { return _numValidations; }
      uint32_t getNumFailedValidations() { return _numFailedValidations; }
      uint32_t getNumInlinedMethodRelos() { return _numInlinedMethodRelos; }
      uint32_t getNumFailedInlinedMethodRelos() { return _numFailedInlinedMethodRelos; }
      uint32_t getNumInlinedAllocRelos() { return _numInlinedAllocRelos; }
      uint32_t getNumFailedAllocInlinedRelos() { return _numFailedInlinedAllocRelos; }

#else
      void incNumValidations() { }
      void incNumFailedValidations() { }
      void incNumInlinedMethodRelos() { }
      void incNumFailedInlinedMethodRelos() { }
      void incNumInlinedAllocRelos() { }
      void incNumFailedAllocInlinedRelos() { }

      uint32_t getNumValidations() { return 0; }
      uint32_t getNumFailedValidations() { return 0; }
      uint32_t getNumInlinedMethodRelos() { return 0; }
      uint32_t getNumFailedInlinedMethodRelos() { return 0; }
      uint32_t getNumInlinedAllocRelos() { return 0; }
      uint32_t getNumFailedAllocInlinedRelos() { return 0; }
#endif

   private:
      virtual uint8_t * allocateSpaceInCodeCache(UDATA codeSize)  ;

      virtual uint8_t * allocateSpaceInDataCache(UDATA metaDataSize,
                                                 UDATA type)                               { return NULL; }

      virtual void initializeAotRuntimeInfo()                                              {}

      virtual void initializeCacheDeltas()                                                 {}

      void relocateAOTCodeAndData(
                                 //  U_8 *tempDataStart,
                                 //  U_8 *oldDataStart,
                                 //  U_8 *oldCodeStart,
                                  U_8 *codeStart
                                 );

      void relocateMethodMetaData(UDATA codeRelocationAmount, UDATA dataRelocationAmount);

      bool aotMethodHeaderVersionsMatch();

      typedef enum {
         RelocationNoError = 1,
         RelocationNoClean = -1, //just a general failure, nothing to really clean
         RelocationTableCreateError = -2, //we failed to allocate to data cache (_newExceptionTableEntry)
         RelocationAssumptionCreateError = -3,
         RelocationPersistentCreateError = -4, //we failed to allocate data cache (_newPersistentInfo)
         RelocationCodeCreateError = -5, //could not allocate code cache entry
         RelocationFailure = -6 //error sometime during actual relocation, full cleanup needed (code & data)
      } TR_AotRelocationCleanUp;
      TR_AotRelocationCleanUp _relocationStatus;
      void relocationFailureCleanup();


      static uintptr_t  _globalValueList[TR_NumGlobalValueItems];
      static uint8_t    _globalValueSizeList[TR_NumGlobalValueItems];
      static char      *_globalValueNames[TR_NumGlobalValueItems];


   protected:
      static bool       _globalValuesInitialized;
      TR::JitConfig *_jitConfig;
      OMR_VM *_omrVM;
      TR_FrontEnd *_fe;
      TR::CodeCacheManager* _manager;
      TR_Memory *_trMemory;
      TR::CompilationInfo * _compInfo;
      TR::RelocationRuntimeLogger *_reloLogger;
      TR::RelocationTarget *_reloTarget;
      TR::AOTStats *_aotStats;
      OMRJITExceptionTable *_exceptionTable;
      uint8_t *_newExceptionTableStart;
      uint8_t *_newPersistentInfo;

      // inlined J9AOTWalkRelocationInfo
      UDATA _dataCacheDelta;
      UDATA _codeCacheDelta;
      // omitted handlers for all the relocation record types

      // inlined TR_AOTRuntimeInfo
      struct TR::AOTHeader* _aotHeader;
      uintptr_t _compileFirstClassLocation;
      U_8 * _baseAddress;
      UDATA _classReloAmount;

      TR::CodeCache *_codeCache;
      struct OMRMemorySegment* _dataCache;

      bool _useCompiledCopy;
      UDATA _metaDataAllocSize;
      TR::AOTMethodHeader *_aotMethodHeaderEntry;
       OMRSharedCacheHeader *_exceptionTableCacheEntry;
      OMR_VMThread *_currentThread;
      OMRMethod *_method;
      uint8_t * _newMethodCodeStart;

      bool _haveReservedCodeCache;

      UDATA _reloStartTime;
      UDATA _reloEndTime;

      int32_t _returnCode;

      TR::Options *_options;
      TR::Compilation *_comp;
      TR_ResolvedMethod *_currentResolvedMethod;

      bool _isLoading;

#if 1 // defined(DEBUG) || defined(PROD_WITH_ASSUMES)
      // Detect unexpected scenarios when build has assumes
      uint32_t _numValidations;
      uint32_t _numFailedValidations;
      uint32_t _numInlinedMethodRelos;
      uint32_t _numFailedInlinedMethodRelos;
      uint32_t _numInlinedAllocRelos;
      uint32_t _numFailedInlinedAllocRelos;
#endif
};
}
namespace OMR
{
class SharedCacheRelocationRuntime : public OMR::RelocationRuntime {
public:
      TR_ALLOC(TR_Memory::Relocation);
      TR::RelocationRuntime* self();
      void * operator new(size_t, TR::JitConfig *);
      SharedCacheRelocationRuntime(TR::JitConfig *jitCfg,TR::CodeCacheManager *ccm) : OMR::RelocationRuntime(jitCfg,ccm) {
         _sharedCacheIsFull=false;
         }
      virtual bool storeAotInformation( uint8_t* codeStart, uint32_t codeSize,uint8_t* dataStart,uint32_t dataSize);
   //  virtual bool storeAOTHeader(OMR_VM *omrVm, TR_FrontEnd *fe, OMR_VMThread *curThread);
   //    virtual TR::AOTHeader *createAOTHeader(OMR_VM *omrVM, TR_FrontEnd *fe);
   //   virtual bool validateAOTHeader(OMR_VM *omrVm, TR_FrontEnd *fe, OMR_VMThread *curThread);
   //      virtual void *isROMClassInSharedCaches(UDATA romClassValue, OMR_VM *omrVm);
   //     virtual bool isRomClassForMethodInSharedCache(OMRMethod *method, OMR_VM *omrVm);
   //    virtual TR_YesNoMaybe isMethodInSharedCache(OMRMethod *method, OMR_VM *omrVm);

      //virtual TR_OpaqueClassBlock *getClassFromCP(OMR_VMThread *vmThread, OMR_VM *omrVm, J9ConstantPool *constantPool, I_32 cpIndex, bool isStatic);
      virtual void *methodAddress(char *methodName);
      virtual void registerLoadedMethod(const char *&methodName,void *&methodAddress);

private:
      uint32_t getCurrentLockwordOptionHashValue(OMR_VM *vm) const;
      virtual uint8_t * allocateSpaceInDataCache(UDATA metaDataSize, UDATA type);
      virtual void initializeAotRuntimeInfo();
      virtual void initializeCacheDeltas();

      virtual void incompatibleCache(U_32 module, U_32 reason, char *assumeMessage);

      void checkAOTHeaderFlags(TR_FrontEnd *fe, TR::AOTHeader * hdrInCache, intptr_t featureFlags);
      bool generateError(char *assumeMessage);

      bool _sharedCacheIsFull;

      static bool useDFPHardware(TR_FrontEnd *fe);
      static uintptr_t generateFeatureFlags(TR_FrontEnd *fe);

      static const char aotHeaderKey[];
      static const UDATA aotHeaderKeyLength;
      std::map<std::string,void *> _symbolLocation;
};
} // end namespace OMR
#endif   // RELOCATION_RUNTIME_INCL
