/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#include "compiler/codegen/AheadOfTimeCompile.hpp"
#include "codegen/FrontEnd.hpp"
#include "codegen/Instruction.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "runtime/RelocationRecord.hpp"
#include "compile/VirtualGuard.hpp"
#include "env/CompilerEnv.hpp"
#include "env/SharedCache.hpp"
#include "env/jittypes.h"
#include <iostream>
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/SymbolReference.hpp"
#include "ras/DebugCounter.hpp"
#include "runtime/CodeCacheConfig.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/RelocationRecord.hpp"
#define NON_HELPER   0x00

TR::AheadOfTimeCompile*
OMR::X86::AMD64::AheadOfTimeCompile::self(){
   return static_cast<TR::AheadOfTimeCompile *> (this);
}
void OMR::X86::AMD64::AheadOfTimeCompile::processRelocations()
   {
   TR::Compilation* comp = self()->comp();
   TR_FrontEnd *fej9 = comp->fe();
   TR::CodeGenerator* _cg = comp->cg();
   // calculate the amount of memory needed to hold the relocation data

   if (TR::Compiler->target.is64Bit()
       && TR::CodeCacheManager::instance()->codeCacheConfig().needsMethodTrampolines()
       && _cg->getPicSlotCount())
      {
      _cg->addExternalRelocation(new (_cg->trHeapMemory()) TR::ExternalRelocation(NULL,
                                                                                 (uint8_t *)(uintptr_t)_cg->getPicSlotCount(),
                                                                                 TR_PicTrampolines, _cg),
                            __FILE__,
                            __LINE__,
                            NULL);
      }


   for (auto aotIterator = _cg->getExternalRelocationList().begin(); aotIterator != _cg->getExternalRelocationList().end(); ++aotIterator)
	  (*aotIterator)->addExternalRelocation(_cg);

   TR::IteratedExternalRelocation *r;
   for (r = self()->getAOTRelocationTargets().getFirst();
        r != NULL;
        r = r->getNext())
      {
      self()->addToSizeOfAOTRelocations(r->getSizeOfRelocationData());
      }

   // now allocate the memory  size of all iterated relocations + the header (total length field)

   // Note that when using the SymbolValidationManager, the well-known classes
   // must be checked even if no explicit records were generated, since they
   // might be responsible for the lack of records.
   //TODO port that!
   //bool useSVM = self()->comp()->getOption(TR_UseSymbolValidationManager);
   bool useSVM = false;
   if (self()->getSizeOfAOTRelocations() != 0 || useSVM)
      {
      // It would be more straightforward to put the well-known classes offset
      // in the AOT method header, but that would use space for AOT bodies that
      // don't use the SVM. TODO: Move it once SVM takes over?
      int wellKnownClassesOffsetSize = useSVM ? SIZEPOINTER : 0;
      uintptrj_t reloBufferSize =
         self()->getSizeOfAOTRelocations() + SIZEPOINTER + wellKnownClassesOffsetSize;
      uint8_t *relocationDataCursor = self()->setRelocationData(
         fej9->allocateRelocationData(self()->comp(), reloBufferSize));
      // set up the size for the region
      *(uintptrj_t *)relocationDataCursor = reloBufferSize;
      relocationDataCursor += SIZEPOINTER;

      // if (useSVM)
      //    {
      //    TR::SymbolValidationManager *svm =
      //       self()->comp()->getSymbolValidationManager();
      //    void *offsets = const_cast<void*>(svm->wellKnownClassChainOffsets());
      //    *(uintptrj_t *)relocationDataCursor = reinterpret_cast<uintptrj_t>(
      //       fej9->sharedCache()->offsetInSharedCacheFromPointer(offsets));
      //    relocationDataCursor += SIZEPOINTER;
      //    }

      // set up pointers for each iterated relocation and initialize header

      
   }
    if (self()->getSizeOfAOTRelocations() != 0)
      {
      uint8_t *relocationDataCursor = self()->setRelocationData(fej9->allocateRelocationData(self()->comp(), self()->getSizeOfAOTRelocations() + 4));

      // set up the size for the region
      *(uint32_t *)relocationDataCursor = self()->getSizeOfAOTRelocations() + 4;
      relocationDataCursor += 4;

      // set up pointers for each iterated relocation and initialize header
      TR::IteratedExternalRelocation *s;
      for (s = self()->getAOTRelocationTargets().getFirst();
           s != NULL;
           s = s->getNext())
         {
         s->setRelocationData(relocationDataCursor);
         s->initializeRelocation(_cg);
         relocationDataCursor += s->getSizeOfRelocationData();
         }
      }
}

uint8_t* OMR::X86::AMD64::AheadOfTimeCompile::initializeAOTRelocationHeader(TR::IteratedExternalRelocation *relocation)
   {
   TR::Compilation* comp = TR::comp();
   TR::CodeGenerator* cg = comp->cg();
   TR::SharedCache* sharedCache = TR::Compiler->cache;
   //TR::SymbolValidationManager *symValManager = comp->getSymbolValidationManager();

   TR_VirtualGuard *guard;
     uint8_t *cursor = relocation->getRelocationData();
     uint8_t *start = cursor;
   uint8_t flags = 0;
   TR_ResolvedMethod *resolvedMethod;

   TR::RelocationRuntime *reloRuntime =new (cg->trHeapMemory()) TR::RelocationRuntime(NULL);
   TR::RelocationTarget *reloTarget = reloRuntime->reloTarget();
   
   uint8_t * aotMethodCodeStart = (uint8_t *) comp->getRelocatableMethodCodeStart();
   // size of relocation goes first in all types
   *(uint16_t *) cursor = relocation->getSizeOfRelocationData();

   cursor += 2;

   uint8_t modifier = 0;
   uint8_t *relativeBitCursor = cursor;
   TR::LabelSymbol *table;
   uint8_t *codeLocation;

   if (relocation->needsWideOffsets())
      modifier |= RELOCATION_TYPE_WIDE_OFFSET;

   uint8_t targetKind = relocation->getTargetKind();
   *cursor++ = targetKind;
   uint8_t *flagsCursor = cursor++;
   *flagsCursor = modifier;
   *(uint32_t*)cursor = cg->getPrePrologueSize();
   cursor+=4;
   uint32_t *wordAfterHeader = (uint32_t*)cursor;
   uint8_t* end = cursor;
   uint8_t diff = end-start;
   // This has to be created after the kind has been written into the header
   TR::RelocationRecord storage;
   TR::RelocationRecord *reloRecord = TR::RelocationRecord::create(&storage, 
                                       reloRuntime, reloTarget,
                                       reinterpret_cast<TR::RelocationRecordBinaryTemplate *>
                                       (relocation->getRelocationData()));
   
   switch (targetKind)
      {
      case TR_MethodCallAddress:
         {
	   OMR::RelocationRecordMethodCallAddress *mcaRecord = reinterpret_cast<OMR::RelocationRecordMethodCallAddress *>(reloRecord);

	   mcaRecord->setEipRelative(reloTarget);
	   uint64_t methodName = 0;
	   //Should work if method name is like func_1
//	   strcpy(reinterpret_cast<char *>(&methodName),const_cast<const char *>(reinterpret_cast<char *>(relocation->getTargetAddress())));
	   memcpy(&methodName,relocation->getTargetAddress(),8);
//	   mcaRecord->setAddress(reloTarget, relocation->getTargetAddress());
	   mcaRecord->setAddress(reloTarget,reinterpret_cast<uint8_t *>(methodName));
         }

	 sharedCache->setRelocationData(relocation->getRelocationData()-4);
	 cursor = relocation->getRelocationData()+_relocationKindToHeaderSizeMap[targetKind];
         break;
      case TR_ArbitrarySizedHeader:
         {
            OMR::RelocationRecordArbitrarySizedHeader *ar = reinterpret_cast<OMR::RelocationRecordArbitrarySizedHeader *>(reloRecord);
            uint8_t theSize = (uint8_t) *relocation ->getTargetAddress2();
            uint8_t* theData = relocation ->getTargetAddress();
            ar->setSizeOfASHLHeader(reloTarget, theSize);
            ar->fillThePayload(reloTarget, theData);
            sharedCache->setRelocationData(relocation->getRelocationData()-4);
            cursor = relocation->getRelocationData()
                     +_relocationKindToHeaderSizeMap[targetKind]+theSize;
         }
         break;
      case TR_DataAddress:
	{
	OMR::RelocationRecordDataAddress *daRecord = reinterpret_cast<OMR::RelocationRecordDataAddress*>(reloRecord);
	TR::SymbolReference * symRef = reinterpret_cast<TR::SymbolReference*>(relocation->getTargetAddress());
	TR::StaticSymbol * symbol = dynamic_cast<TR::StaticSymbol*>(symRef->getSymbol());
	uint64_t index = symbol->getTOCIndex();
	daRecord->setOffset(reloTarget,index);
	}
	sharedCache->setRelocationData(relocation->getRelocationData()-4);
	cursor = relocation->getRelocationData()+_relocationKindToHeaderSizeMap[targetKind];
	break;
      default:
         // initializeCommonAOTRelocationHeader is currently in the process
         // of becoming the canonical place to initialize the platform agnostic
         // relocation headers; new relocation records' header should be
         // initialized here.
         cursor = self()->initializeCommonAOTRelocationHeader(relocation, reloRecord);

      }
   return cursor;
   }
uint8_t *
OMR::X86::AMD64::AheadOfTimeCompile::initializeCommonAOTRelocationHeader(TR::IteratedExternalRelocation *relocation, TR::RelocationRecord *reloRecord)
   {
   uint8_t *cursor = relocation->getRelocationData();

   TR::Compilation *comp = TR::comp();
   TR::RelocationRuntime *reloRuntime = NULL;
   TR::RelocationTarget *reloTarget = reloRuntime->reloTarget();
   // TR::SymbolValidationManager *symValManager = comp->getSymbolValidationManager();
   // TR_J9VMBase *fej9 = comp->fej9();
   // TR_SharedCache *sharedCache = fej9->sharedCache();

   TR_ExternalRelocationTargetKind kind = relocation->getTargetKind();

   // initializeCommonAOTRelocationHeader is currently in the process
   // of becoming the canonical place to initialize the platform agnostic
   // relocation headers; new relocation records' header should be
   // initialized here.
   switch (kind)
      {
      case TR_ConstantPool:
      
      default:
         return cursor;
      }

   reloRecord->setSize(reloTarget, relocation->getSizeOfRelocationData());
   reloRecord->setType(reloTarget, kind);

   uint8_t wideOffsets = relocation->needsWideOffsets() ? RELOCATION_TYPE_WIDE_OFFSET : 0;
   reloRecord->setFlag(reloTarget, wideOffsets);

   cursor += TR::RelocationRecord::getSizeOfAOTRelocationHeader(kind);

   return cursor;
   }


uint32_t OMR::X86::AMD64::AheadOfTimeCompile::_relocationKindToHeaderSizeMap[TR_NumExternalRelocationKinds] =
   {
// FIXME this code needs to be cleaned up by having here access to the platform pointer size
//       or by defining in the runtime.hpp the sizes of the relocation items
#if defined (TR_HOST_64BIT)
   24,                                              // TR_ConstantPool                        = 0
   8,                                               // TR_HelperAddress                       = 1
   24,                                              // TR_RelativeMethodAddress               = 2
   8,                                               // TR_AbsoluteMethodAddress               = 3
   sizeof(TR::RelocationRecordDataAddressBinaryTemplate),                                              // TR_DataAddress                         = 4
   24,                                              // TR_ClassObject                         = 5
   24,                                              // TR_MethodObject                        = 6
   24,                                              // TR_InterfaceObject                     = 7
   8,                                               // TR_AbsoluteHelperAddress               = 8
   16,                                              // TR_FixedSeqAddress                     = 9
   16,                                              // TR_FixedSeq2Address                    = 10
   32,                                              // TR_JNIVirtualTargetAddress	      = 11
   32,                                              // TR_JNIStaticTargetAddress              = 12
   4,                                               // Dummy for TR_ArrayCopyHelper           = 13
   4,                                               // Dummy for TR_ArrayCopyToc              = 14
   8,                                               // TR_BodyInfoAddress                     = 15
   24,                                              // TR_Thunks                              = 16
   32,                                              // TR_StaticRamMethodConst                = 17
   24,                                              // TR_Trampolines                         = 18
   8,                                               // TR_PicTrampolines                      = 19
   16,                                              // TR_CheckMethodEnter                    = 20
   8,                                               // TR_RamMethod                           = 21
   16,                                              // TR_RamMethodSequence                   = 22
   16,                                              // TR_RamMethodSequenceReg                = 23
   48,                                              // TR_VerifyClassObjectForAlloc           = 24
   24,                                              // TR_ConstantPoolOrderedPair             = 25
   8,                                               // TR_AbsoluteMethodAddressOrderedPair    = 26
   40,                                              // TR_VerifyRefArrayForAlloc              = 27
   24,                                              // TR_J2IThunks                           = 28
   16,                                              // TR_GlobalValue                         = 29
   4,                                               // dummy for TR_BodyInfoAddress           = 30
   40,                                              // TR_ValidateInstanceField               = 31
   48,                                              // TR_InlinedStaticMethodWithNopGuard     = 32
   48,                                              // TR_InlinedSpecialMethodWithNopGuard    = 33
   48,                                              // TR_InlinedVirtualMethodWithNopGuard    = 34
   48,                                              // TR_InlinedInterfaceMethodWithNopGuard  = 35
   32,                                              // TR_SpecialRamMethodConst               = 36
   48,                                              // TR_InlinedHCRMethod                    = 37
   40,                                              // TR_ValidateStaticField                 = 38
   40,                                              // TR_ValidateClass                       = 39
   32,                                              // TR_ClassAddress                        = 40
   16,                                              // TR_HCR                                 = 41
   64,                                              // TR_ProfiledMethodGuardRelocation       = 42
   64,                                              // TR_ProfiledClassGuardRelocation        = 43
   0,                                               // TR_HierarchyGuardRelocation            = 44
   0,                                               // TR_AbstractGuardRelocation             = 45
   64,                                              // TR_ProfiledInlinedMethodRelocation     = 46
   40,                                              // TR_MethodPointer                       = 47
   32,                                              // TR_ClassPointer                        = 48
   16,                                              // TR_CheckMethodExit                     = 49
   24,                                              // TR_ValidateArbitraryClass              = 50
   0,                                               // TR_EmitClass(not used)                 = 51
   32,                                              // TR_JNISpecialTargetAddress             = 52
   32,                                              // TR_VirtualRamMethodConst               = 53
   40,                                              // TR_InlinedInterfaceMethod              = 54
   40,                                              // TR_InlinedVirtualMethod                = 55
   0,                                               // TR_NativeMethodAbsolute                = 56,
   0,                                               // TR_NativeMethodRelative                = 57,
   32,                                              // TR_ArbitraryClassAddress               = 58,
   56,                                              // TR_DebugCounter                        = 59
   8,                                               // TR_ClassUnloadAssumption               = 60
   32,                                              // TR_J2IVirtualThunkPointer              = 61,
   48,                                              // TR_InlinedAbstractMethodWithNopGuard   = 62,
   0,                                                                  // TR_ValidateRootClass                   = 63,
0, //  sizeof(TR_RelocationRecordValidateClassByNameBinaryTemplate),       // TR_ValidateClassByName                 = 64,
0,  // sizeof(TR_RelocationRecordValidateProfiledClassBinaryTemplate),     // TR_ValidateProfiledClass               = 65,
0,  // sizeof(TR_RelocationRecordValidateClassFromCPBinaryTemplate),       // TR_ValidateClassFromCP                 = 66,
0,   // sizeof(TR_RelocationRecordValidateDefiningClassFromCPBinaryTemplate),//TR_ValidateDefiningClassFromCP         = 67,
0,   // sizeof(TR_RelocationRecordValidateStaticClassFromCPBinaryTemplate), // TR_ValidateStaticClassFromCP           = 68,
   0,                                                                  // TR_ValidateClassFromMethod             = 69,
   0,                                                                  // TR_ValidateComponentClassFromArrayClass= 70,
0,   // sizeof(TR_RelocationRecordValidateArrayFromCompBinaryTemplate),     // TR_ValidateArrayClassFromComponentClass= 71,
0,   // sizeof(TR_RelocationRecordValidateSuperClassFromClassBinaryTemplate),//TR_ValidateSuperClassFromClass         = 72,
0,   // sizeof(TR_RelocationRecordValidateClassInstanceOfClassBinaryTemplate),//TR_ValidateClassInstanceOfClass       = 73,
0,   // sizeof(TR_RelocationRecordValidateSystemClassByNameBinaryTemplate), //TR_ValidateSystemClassByName            = 74,
0,   // sizeof(TR_RelocationRecordValidateClassFromITableIndexCPBinaryTemplate),//TR_ValidateClassFromITableIndexCP   = 75,
0,   // sizeof(TR_RelocationRecordValidateDeclaringClassFromFieldOrStaticBinaryTemplate),//TR_ValidateDeclaringClassFromFieldOrStatic=76,
   0,                                                                  // TR_ValidateClassClass                  = 77,
0,   // sizeof(TR_RelocationRecordValidateConcreteSubFromClassBinaryTemplate),//TR_ValidateConcreteSubClassFromClass  = 78,
0,   // sizeof(TR_RelocationRecordValidateClassChainBinaryTemplate),        // TR_ValidateClassChain                  = 79,
   0,                                                                  // TR_ValidateRomClass                    = 80,
   0,                                                                  // TR_ValidatePrimitiveClass              = 81,
   0,                                                                  // TR_ValidateMethodFromInlinedSite       = 82,
   0,                                                                  // TR_ValidatedMethodByName               = 83,
0,   // sizeof(TR_RelocationRecordValidateMethodFromClassBinaryTemplate),   // TR_ValidatedMethodFromClass            = 84,
0,   // sizeof(TR_RelocationRecordValidateStaticMethodFromCPBinaryTemplate),// TR_ValidateStaticMethodFromCP          = 85,
0,   // sizeof(TR_RelocationRecordValidateSpecialMethodFromCPBinaryTemplate),//TR_ValidateSpecialMethodFromCP         = 86,
0,   // sizeof(TR_RelocationRecordValidateVirtualMethodFromCPBinaryTemplate),//TR_ValidateVirtualMethodFromCP         = 87,
0,   // sizeof(TR_RelocationRecordValidateVirtualMethodFromOffsetBinaryTemplate),//TR_ValidateVirtualMethodFromOffset = 88,
0,   // sizeof(TR_RelocationRecordValidateInterfaceMethodFromCPBinaryTemplate),//TR_ValidateInterfaceMethodFromCP     = 89,
0,   // sizeof(TR_RelocationRecordValidateMethodFromClassAndSigBinaryTemplate),//TR_ValidateMethodFromClassAndSig     = 90,
0,   // sizeof(TR_RelocationRecordValidateStackWalkerMaySkipFramesBinaryTemplate),//TR_ValidateStackWalkerMaySkipFramesRecord= 91,
   0,                                                                  // TR_ValidateArrayClassFromJavaVM        = 92,
0,   // sizeof(TR_RelocationRecordValidateClassInfoIsInitializedBinaryTemplate),//TR_ValidateClassInfoIsInitialized   = 93,
0,   // sizeof(TR_RelocationRecordValidateMethodFromSingleImplBinaryTemplate),//TR_ValidateMethodFromSingleImplementer= 94,
0,   // sizeof(TR_RelocationRecordValidateMethodFromSingleInterfaceImplBinaryTemplate),//TR_ValidateMethodFromSingleInterfaceImplementer= 95,
0,   // sizeof(TR_RelocationRecordValidateMethodFromSingleAbstractImplBinaryTemplate),//TR_ValidateMethodFromSingleAbstractImplementer= 96,
0,   // sizeof(TR_RelocationRecordValidateImproperInterfaceMethodFromCPBinaryTemplate),//TR_ValidateImproperInterfaceMethodFromCP= 97,
0,   // sizeof(TR_RelocationRecordSymbolFromManagerBinaryTemplate),         // TR_SymbolFromManager = 98,
sizeof(TR::RelocationRecordMethodCallAddressBinaryTemplate),         // TR_MethodCallAddress                   = 99,
0, 
sizeof(OMR::RelocationRecordASHLBinaryTemplate) // 
#else

   12,                                              // TR_ConstantPool                        = 0
   8,                                               // TR_HelperAddress                       = 1
   12,                                              // TR_RelativeMethodAddress               = 2
   4,                                               // TR_AbsoluteMethodAddress               = 3
   20,                                              // TR_DataAddress                         = 4
   12,                                              // TR_ClassObject                         = 5
   12,                                              // TR_MethodObject                        = 6
   12,                                              // TR_InterfaceObject                     = 7
   8,                                               // TR_AbsoluteHelperAddress               = 8
   8,                                               // TR_FixedSeqAddress                     = 9
   8,                                               // TR_FixedSeq2Address                    = 10
   16,                                              // TR_JNIVirtualTargetAddress             = 11
   16,                                              // TR_JNIStaticTargetAddress              = 12
   4,                                               // Dummy for TR_ArrayCopyHelper           = 13
   4,                                               // Dummy for TR_ArrayCopyToc              = 14
   4,                                               // TR_BodyInfoAddress                     = 15
   12,                                              // TR_Thunks                              = 16
   16,                                              // TR_StaticRamMethodConst                = 17
   12,                                              // TR_Trampolines                         = 18
   8,                                               // TR_PicTrampolines                      = 19
   8,                                               // TR_CheckMethodEnter                    = 20
   4,                                               // TR_RamMethod                           = 21
   8,                                               // TR_RamMethodSequence                   = 22
   8,                                               // TR_RamMethodSequenceReg                = 23
   24,                                              // TR_VerifyClassObjectForAlloc           = 24
   12,                                              // TR_ConstantPoolOrderedPair             = 25
   8,                                               // TR_AbsoluteMethodAddressOrderedPair    = 26
   20,                                              // TR_VerifyRefArrayForAlloc              = 27
   12,                                              // TR_J2IThunks                           = 28
   8,                                               // TR_GlobalValue                         = 29
   4,                                               // TR_BodyInfoAddressLoad                 = 30
   20,                                              // TR_ValidateInstanceField               = 31
   24,                                              // TR_InlinedStaticMethodWithNopGuard     = 32
   24,                                              // TR_InlinedSpecialMethodWithNopGuard    = 33
   24,                                              // TR_InlinedVirtualMethodWithNopGuard    = 34
   24,                                              // TR_InlinedInterfaceMethodWithNopGuard  = 35
   16,                                              // TR_SpecialRamMethodConst               = 36
   24,                                              // TR_InlinedHCRMethod                    = 37
   20,                                              // TR_ValidateStaticField                 = 38
   20,                                              // TR_ValidateClass                       = 39
   16,                                              // TR_ClassAddress                        = 40
   8,                                               // TR_HCR                                 = 41
   32,                                              // TR_ProfiledMethodGuardRelocation       = 42
   32,                                              // TR_ProfiledClassGuardRelocation        = 43
   0,                                               // TR_HierarchyGuardRelocation            = 44
   0,                                               // TR_AbstractGuardRelocation             = 45
   32,                                              // TR_ProfiledInlinedMethodRelocation     = 46
   20,                                              // TR_MethodPointer                       = 47
   16,                                              // TR_ClassPointer                        = 48
   8,                                               // TR_CheckMethodExit                     = 49
   12,                                              // TR_ValidateArbitraryClass              = 50
   0,                                               // TR_EmitClass(not used)                 = 51
   16,                                              // TR_JNISpecialTargetAddress             = 52
   16,                                              // TR_VirtualRamMethodConst               = 53
   20,                                              // TR_InlinedInterfaceMethod              = 54
   20,                                              // TR_InlinedVirtualMethod                = 55
   0,                                               // TR_NativeMethodAbsolute                = 56,
   0,                                               // TR_NativeMethodRelative                = 57,
   16,                                              // TR_ArbitraryClassAddress               = 58,
   28,                                               // TR_DebugCounter                        = 59
   4,                                               // TR_ClassUnloadAssumption               = 60
   16,                                              // TR_J2IVirtualThunkPointer              = 61,
   24,                                              // TR_InlinedAbstractMethodWithNopGuard   = 62,
#endif
   };
