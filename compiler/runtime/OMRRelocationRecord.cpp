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

#include <stdint.h>
#include "omrcfg.h"
#include "codegen/CodeGenerator.hpp"
#include "codegen/FrontEnd.hpp"
#include "codegen/Relocation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/jittypes.h"
#include "env/VMAccessCriticalSection.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "infra/SimpleRegex.hpp"
#include "runtime/CodeCache.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/RelocationRecord.hpp"
#include "runtime/RelocationRuntime.hpp"
#include "runtime/RelocationTarget.hpp"
#include  "runtime/SymbolValidationManager.hpp"

// TODO: move this someplace common for RuntimeAssumptions.cpp and here
#if defined(__IBMCPP__) && !defined(AIXPPC) && !defined(LINUXPPC)
#define ASM_CALL __cdecl
#else
#define ASM_CALL
#endif
#if defined(TR_HOST_S390) || defined(TR_HOST_X86) // gotten from RuntimeAssumptions.cpp, should common these up
extern "C" void _patchVirtualGuard(uint8_t *locationAddr, uint8_t *destinationAddr, int32_t smpFlag);
#else
extern "C" void ASM_CALL _patchVirtualGuard(uint8_t*, uint8_t*, uint32_t);
#endif

uint8_t
OMR::RelocationRecordBinaryTemplate::type(TR::RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned8b(&_type);
   }

// TR::RelocationRecordGroup

void
TR::RelocationRecordGroup::setSize(TR::RelocationTarget *reloTarget,uintptr_t size)
   {
   reloTarget->storePointer((uint8_t *)size, (uint8_t *) &_group);
   }

uintptr_t
TR::RelocationRecordGroup::size(TR::RelocationTarget *reloTarget)
   {
   return (uintptr_t)reloTarget->loadPointer((uint8_t *) _group);
   }

TR::RelocationRecordBinaryTemplate *
TR::RelocationRecordGroup::firstRecord(TR::RelocationTarget *reloTarget)
   {
   // first word of the group is a pointer size field for the entire group
   return (TR::RelocationRecordBinaryTemplate *) (((uintptr_t *)_group)+1);
   }

TR::RelocationRecordBinaryTemplate *
TR::RelocationRecordGroup::pastLastRecord(TR::RelocationTarget *reloTarget)
   {
   return (TR::RelocationRecordBinaryTemplate *) ((uint8_t *)_group + size(reloTarget));
   }

int32_t
TR::RelocationRecordGroup::applyRelocations(TR::RelocationRuntime *reloRuntime,
                                           TR::RelocationTarget *reloTarget,
                                           uint8_t *reloOrigin)
   {
   TR::RelocationRecordBinaryTemplate *recordPointer = firstRecord(reloTarget);
   TR::RelocationRecordBinaryTemplate *endOfRecords = pastLastRecord(reloTarget);

   while (recordPointer < endOfRecords)
      {
      TR::RelocationRecord storage;
      // Create a specific type of relocation record based on the information
      // in the binary record pointed to by `recordPointer`
      TR::RelocationRecord *reloRecord = TR::RelocationRecord::create(&storage, reloRuntime, reloTarget, recordPointer);
      int32_t rc = handleRelocation(reloRuntime, reloTarget, reloRecord, reloOrigin);
      if (rc != 0)
         return rc;

      recordPointer = reloRecord->nextBinaryRecord(reloTarget);
      }

   return 0;
   }


int32_t
TR::RelocationRecordGroup::handleRelocation(TR::RelocationRuntime *reloRuntime,
                                           TR::RelocationTarget *reloTarget,
                                           TR::RelocationRecord *reloRecord,
                                           uint8_t *reloOrigin)
   {

   if (reloRecord->ignore(reloRuntime))
      {
      return 0;
      }

   reloRecord->preparePrivateData(reloRuntime, reloTarget);
   return reloRecord->applyRelocationAtAllOffsets(reloRuntime, reloTarget, reloOrigin);
   }

#define FLAGS_RELOCATION_WIDE_OFFSETS   0x80
#define FLAGS_RELOCATION_EIP_OFFSET     0x40
#define FLAGS_RELOCATION_TYPE_MASK      (TR::ExternalRelocationTargetKindMask)
#define FLAGS_RELOCATION_FLAG_MASK      ((uint8_t) (FLAGS_RELOCATION_WIDE_OFFSETS | FLAGS_RELOCATION_EIP_OFFSET))


TR::RelocationRecord *
TR::RelocationRecord::create(TR::RelocationRecord *storage, TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, TR::RelocationRecordBinaryTemplate *record)
   {
   TR::RelocationRecord *reloRecord = NULL;
   // based on the type of the relocation record, create an object of a particular variety of TR::RelocationRecord object
   uint8_t reloType = record->type(reloTarget);
   switch (reloType)
      {
      case TR::HelperAddress:
         reloRecord = new (storage) TR::RelocationRecordHelperAddress(reloRuntime, record);
         break;
      case TR::ConstantPool:
      case TR::ConstantPoolOrderedPair:
         reloRecord = new (storage) TR::RelocationRecordConstantPool(reloRuntime, record);
         break;
      case TR::BodyInfoAddress:
         reloRecord = new (storage) TR::RelocationRecordBodyInfo(reloRuntime, record);
         break;
      case TR::VerifyRefArrayForAlloc:
         reloRecord = new (storage) TR::RelocationRecordVerifyRefArrayForAlloc(reloRuntime, record);
         break;

      default:
         // TODO: error condition
         printf("Unexpected relo record: %d\n", reloType);fflush(stdout);
         exit(0);
      }
      return reloRecord;
   }

void
OMR::RelocationRecord::print(TR::RelocationRuntime *reloRuntime)
   {
   TR::RelocationTarget *reloTarget = reloRuntime->reloTarget();
   }

void
TR::RelocationRecord::clean(TR::RelocationTarget *reloTarget)
   {
   setSize(reloTarget, 0);
   reloTarget->storeUnsigned8b(0, (uint8_t *) &_record->_type);
   reloTarget->storeUnsigned8b(0, (uint8_t *) &_record->_flags);
   }

int32_t
TR::RelocationRecord::bytesInHeaderAndPayload()
   {
   return sizeof(TR::RelocationRecordBinaryTemplate);
   }

TR::RelocationRecordBinaryTemplate *
TR::RelocationRecord::nextBinaryRecord(TR::RelocationTarget *reloTarget)
   {
   return (TR::RelocationRecordBinaryTemplate*) (((uint8_t*)this->_record) + size(reloTarget));
   }

void
TR::RelocationRecord::setSize(TR::RelocationTarget *reloTarget, uint16_t size)
   {
   reloTarget->storeUnsigned16b(size,(uint8_t *) &_record->_size);
   }

uint16_t
TR::RelocationRecord::size(TR::RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &_record->_size);
   }


void
TR::RelocationRecord::setType(TR::RelocationTarget *reloTarget, TR::RelocationRecordType type)
   {
   reloTarget->storeUnsigned8b(type, (uint8_t *) &_record->_type);
   }

TR::RelocationRecordType
TR::RelocationRecord::type(TR::RelocationTarget *reloTarget)
   {
   return (TR::RelocationRecordType)_record->type(reloTarget);
   }


void
TR::RelocationRecord::setWideOffsets(TR::RelocationTarget *reloTarget)
   {
   setFlag(reloTarget, FLAGS_RELOCATION_WIDE_OFFSETS);
   }

bool
TR::RelocationRecord::wideOffsets(TR::RelocationTarget *reloTarget)
   {
   return (flags(reloTarget) & FLAGS_RELOCATION_WIDE_OFFSETS) != 0;
   }

void
TR::RelocationRecord::setEipRelative(TR::RelocationTarget *reloTarget)
   {
   setFlag(reloTarget, FLAGS_RELOCATION_EIP_OFFSET);
   }

bool
TR::RelocationRecord::eipRelative(TR::RelocationTarget *reloTarget)
   {
   return (flags(reloTarget) & FLAGS_RELOCATION_EIP_OFFSET) != 0;
   }

void
TR::RelocationRecord::setFlag(TR::RelocationTarget *reloTarget, uint8_t flag)
   {
   uint8_t flags = reloTarget->loadUnsigned8b((uint8_t *) &_record->_flags) | (flag & FLAGS_RELOCATION_FLAG_MASK);
   reloTarget->storeUnsigned8b(flags, (uint8_t *) &_record->_flags);
   }

uint8_t
TR::RelocationRecord::flags(TR::RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned8b((uint8_t *) &_record->_flags) & FLAGS_RELOCATION_FLAG_MASK;
   }

void
TR::RelocationRecord::setReloFlags(TR::RelocationTarget *reloTarget, uint8_t reloFlags)
   {
   TR::ASSERT((reloFlags & ~FLAGS_RELOCATION_FLAG_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
   uint8_t crossPlatFlags = flags(reloTarget);
   uint8_t flags = crossPlatFlags | (reloFlags & ~FLAGS_RELOCATION_FLAG_MASK);
   reloTarget->storeUnsigned8b(flags, (uint8_t *) &_record->_flags);
   }

uint8_t
TR::RelocationRecord::reloFlags(TR::RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned8b((uint8_t *) &_record->_flags) & ~FLAGS_RELOCATION_FLAG_MASK;
   }

void
TR::RelocationRecord::preparePrivateData(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget)
   {
   }

// Generic helper address computation for multiple relocation types
uint8_t *
TR::RelocationRecord::computeHelperAddress(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *baseLocation)
   {
   TR::RelocationRecordHelperAddressPrivateData *reloPrivateData = &(privateData()->helperAddress);
   uint8_t *helperAddress = reloPrivateData->_helper;

   if (reloRuntime->options()->getOption(TR::StressTrampolines) || reloTarget->useTrampoline(helperAddress, baseLocation))
      {
      TR::VMAccessCriticalSection computeHelperAddress(reloRuntime->fej9());
      J9JavaVM *javaVM = reloRuntime->jitConfig()->javaVM;
      helperAddress = (uint8_t *)TR::CodeCacheManager::instance()->findHelperTrampoline((void *)baseLocation, reloPrivateData->_helperID);
      }

   return helperAddress;
   }

#undef FLAGS_RELOCATION_WIDE_OFFSETS
#undef FLAGS_RELOCATION_EIP_OFFSET
#undef FLAGS_RELOCATION_ORDERED_PAIR
#undef FLAGS_RELOCATION_TYPE_MASK
#undef FLAGS_RELOCATION_FLAG_MASK


bool
TR::RelocationRecord::ignore(TR::RelocationRuntime *reloRuntime)
   {
   return false;
   }

int32_t
TR::RelocationRecord::applyRelocationAtAllOffsets(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloOrigin)
   {
   if (ignore(reloRuntime))
      {
      RELO_LOG(reloRuntime->reloLogger(), 6, "\tignore!\n");
      return 0;
      }

   if (reloTarget->isOrderedPairRelocation(this, reloTarget))
      {
      if (wideOffsets(reloTarget))
         {
         int32_t *offsetsBase = (int32_t *) (((uint8_t*)_record) + bytesInHeaderAndPayload());
         int32_t *endOfOffsets = (int32_t *) nextBinaryRecord(reloTarget);
         for (int32_t *offsetPtr = offsetsBase;offsetPtr < endOfOffsets; offsetPtr+=2)
            {
            int32_t offsetHigh = *offsetPtr;
            int32_t offsetLow = *(offsetPtr+1);
            uint8_t *reloLocationHigh = reloOrigin + offsetHigh + 2; // Add 2 to skip the first 16 bits of instruction
            uint8_t *reloLocationLow = reloOrigin + offsetLow + 2; // Add 2 to skip the first 16 bits of instruction
            RELO_LOG(reloRuntime->reloLogger(), 6, "\treloLocation: from %p high %p low %p (offsetHigh %x offsetLow %x)\n", offsetPtr, reloLocationHigh, reloLocationLow, offsetHigh, offsetLow);
            int32_t rc = applyRelocation(reloRuntime, reloTarget, reloLocationHigh, reloLocationLow);
            if (rc != 0)
               {
               RELO_LOG(reloRuntime->reloLogger(), 6, "\tapplyRelocationAtAllOffsets: rc = %d\n", rc);
               return rc;
               }
            }
         }
      else
         {
         int16_t *offsetsBase = (int16_t *) (((uint8_t*)_record) + bytesInHeaderAndPayload());
         int16_t *endOfOffsets = (int16_t *) nextBinaryRecord(reloTarget);
         for (int16_t *offsetPtr = offsetsBase;offsetPtr < endOfOffsets; offsetPtr+=2)
            {
            int16_t offsetHigh = *offsetPtr;
            int16_t offsetLow = *(offsetPtr+1);
            uint8_t *reloLocationHigh = reloOrigin + offsetHigh + 2; // Add 2 to skip the first 16 bits of instruction
            uint8_t *reloLocationLow = reloOrigin + offsetLow + 2; // Add 2 to skip the first 16 bits of instruction
            RELO_LOG(reloRuntime->reloLogger(), 6, "\treloLocation: from %p high %p low %p (offsetHigh %x offsetLow %x)\n", offsetPtr, reloLocationHigh, reloLocationLow, offsetHigh, offsetLow);
            int32_t rc = applyRelocation(reloRuntime, reloTarget, reloLocationHigh, reloLocationLow);
            if (rc != 0)
               {
               RELO_LOG(reloRuntime->reloLogger(), 6, "\tapplyRelocationAtAllOffsets: rc = %d\n", rc);
               return rc;
               }
            }
         }
      }
   else
      {
      if (wideOffsets(reloTarget))
         {
         int32_t *offsetsBase = (int32_t *) (((uint8_t*)_record) + bytesInHeaderAndPayload());
         int32_t *endOfOffsets = (int32_t *) nextBinaryRecord(reloTarget);
         for (int32_t *offsetPtr = offsetsBase;offsetPtr < endOfOffsets; offsetPtr++)
            {
            int32_t offset = *offsetPtr;
            uint8_t *reloLocation = reloOrigin + offset;
            RELO_LOG(reloRuntime->reloLogger(), 6, "\treloLocation: from %p at %p (offset %x)\n", offsetPtr, reloLocation, offset);
            int32_t rc = applyRelocation(reloRuntime, reloTarget, reloLocation);
            if (rc != 0)
               {
               RELO_LOG(reloRuntime->reloLogger(), 6, "\tapplyRelocationAtAllOffsets: rc = %d\n", rc);
               return rc;
               }
            }
         }
      else
         {
         int16_t *offsetsBase = (int16_t *) (((uint8_t*)_record) + bytesInHeaderAndPayload());
         int16_t *endOfOffsets = (int16_t *) nextBinaryRecord(reloTarget);
         for (int16_t *offsetPtr = offsetsBase;offsetPtr < endOfOffsets; offsetPtr++)
            {
            int16_t offset = *offsetPtr;
            uint8_t *reloLocation = reloOrigin + offset;
            RELO_LOG(reloRuntime->reloLogger(), 6, "\treloLocation: from %p at %p (offset %x)\n", offsetPtr, reloLocation, offset);
            int32_t rc = applyRelocation(reloRuntime, reloTarget, reloLocation);
            if (rc != 0)
               {
               RELO_LOG(reloRuntime->reloLogger(), 6, "\tapplyRelocationAtAllOffsets: rc = %d\n", rc);
               return rc;
               }
            }
         }
      }
      return 0;
   }

// Handlers for individual relocation record types

// Relocations with address sequences
//

void
TR::RelocationRecordWithOffset::print(TR::RelocationRuntime *reloRuntime)
   {
   TR::RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR::RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR::RelocationRecord::print(reloRuntime);
   reloLogger->printf("\toffset %x\n", offset(reloTarget));
   }

void
TR::RelocationRecordWithOffset::setOffset(TR::RelocationTarget *reloTarget, uintptrj_t offset)
   {
   reloTarget->storeRelocationRecordValue(offset, (uintptrj_t *) &((TR::RelocationRecordWithOffsetBinaryTemplate *)_record)->_offset);
   }

uintptrj_t
TR::RelocationRecordWithOffset::offset(TR::RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR::RelocationRecordWithOffsetBinaryTemplate *)_record)->_offset);
   }

int32_t
TR::RelocationRecordWithOffset::bytesInHeaderAndPayload()
   {
   return sizeof(TR::RelocationRecordWithOffsetBinaryTemplate);
   }

void
TR::RelocationRecordWithOffset::preparePrivateData(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget)
   {
   TR::RelocationRecordWithOffsetPrivateData *reloPrivateData = &(privateData()->offset);
   reloPrivateData->_addressToPatch = offset(reloTarget) ? reloRuntime->newMethodCodeStart() + offset(reloTarget) : 0x0;
   RELO_LOG(reloRuntime->reloLogger(), 6, "\tpreparePrivateData: addressToPatch: %p \n", reloPrivateData->_addressToPatch);
   }

int32_t
TR::RelocationRecordWithOffset::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR::RelocationRecordWithOffsetPrivateData *reloPrivateData = &(privateData()->offset);
   reloTarget->storeAddressSequence(reloPrivateData->_addressToPatch, reloLocation, reloFlags(reloTarget));

   return 0;
   }

int32_t
TR::RelocationRecordWithOffset::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   TR::RelocationRecordWithOffsetPrivateData *reloPrivateData = &(privateData()->offset);
   reloTarget->storeAddress(reloPrivateData->_addressToPatch, reloLocationHigh, reloLocationLow, reloFlags(reloTarget));
   return 0;
   }

// TR::GlobalValue
//
char *
TR::RelocationRecordGlobalValue::name()
   {
   return "TR::GlobalValue";
   }

void
TR::RelocationRecordGlobalValue::preparePrivateData(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget)
   {
   TR::RelocationRecordWithOffsetPrivateData *reloPrivateData = &(privateData()->offset);
   reloPrivateData->_addressToPatch = (uint8_t *)reloRuntime->getGlobalValue((TR::GlobalValueItem) offset(reloTarget));
   RELO_LOG(reloRuntime->reloLogger(), 6, "\tpreparePrivateData: global value %p \n", reloPrivateData->_addressToPatch);
   }

// TR::BodyInfoLoad
char *
TR::RelocationRecordBodyInfoLoad::name()
   {
   return "TR::BodyInfoLoad";
   }

void
TR::RelocationRecordBodyInfoLoad::preparePrivateData(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget)
   {
   TR::RelocationRecordWithOffsetPrivateData *reloPrivateData = &(privateData()->offset);
   reloPrivateData->_addressToPatch = (uint8_t *)reloRuntime->exceptionTable()->bodyInfo;
   RELO_LOG(reloRuntime->reloLogger(), 6, "\tpreparePrivateData: body info %p \n", reloPrivateData->_addressToPatch);
   }

int32_t
TR::RelocationRecordBodyInfoLoad::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR::RelocationRecordWithOffsetPrivateData *reloPrivateData = &(privateData()->offset);
   reloTarget->storeAddressSequence(reloPrivateData->_addressToPatch, reloLocation, reloFlags(reloTarget));
   return 0;
   }

int32_t
TR::RelocationRecordBodyInfoLoad::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   TR::RelocationRecordWithOffsetPrivateData *reloPrivateData = &(privateData()->offset);
   reloTarget->storeAddress(reloPrivateData->_addressToPatch, reloLocationHigh, reloLocationLow, reloFlags(reloTarget));
   return 0;
   }

// TR::ArrayCopyHelper
char *
TR::RelocationRecordArrayCopyHelper::name()
   {
   return "TR::ArrayCopyHelper";
   }

void
TR::RelocationRecordArrayCopyHelper::preparePrivateData(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget)
   {
   TR::RelocationRecordArrayCopyPrivateData *reloPrivateData = &(privateData()->arraycopy);
   J9JITConfig *jitConfig = reloRuntime->jitConfig();
   TR::ASSERT(jitConfig != NULL, "Relocation runtime doesn't have a jitConfig!");
   J9JavaVM *javaVM = jitConfig->javaVM;

   reloPrivateData->_addressToPatch = (uint8_t *)reloTarget->arrayCopyHelperAddress(javaVM);
   RELO_LOG(reloRuntime->reloLogger(), 6, "\tpreparePrivateData: arraycopy helper %p\n", reloPrivateData->_addressToPatch);
   }

int32_t
TR::RelocationRecordArrayCopyHelper::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR::RelocationRecordArrayCopyPrivateData *reloPrivateData = &(privateData()->arraycopy);
   reloTarget->storeAddressSequence(reloPrivateData->_addressToPatch, reloLocation, reloFlags(reloTarget));

   return 0;
   }

int32_t
TR::RelocationRecordArrayCopyHelper::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   TR::RelocationRecordArrayCopyPrivateData *reloPrivateData = &(privateData()->arraycopy);
   reloTarget->storeAddress(reloPrivateData->_addressToPatch, reloLocationHigh, reloLocationLow, reloFlags(reloTarget));
   return 0;
   }

// TR::ArrayCopyToc
char *
TR::RelocationRecordArrayCopyToc::name()
   {
   return "TR::ArrayCopyToc";
   }

void
TR::RelocationRecordArrayCopyToc::preparePrivateData(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget)
   {
   TR::RelocationRecordArrayCopyPrivateData *reloPrivateData = &(privateData()->arraycopy);
   J9JITConfig *jitConfig = reloRuntime->jitConfig();
   TR::ASSERT(jitConfig != NULL, "Relocation runtime doesn't have a jitConfig!");
   J9JavaVM *javaVM = jitConfig->javaVM;
  uintptr_t *funcdescrptr = (uintptr_t *)javaVM->memoryManagerFunctions->referenceArrayCopy;
   reloPrivateData->_addressToPatch = (uint8_t *)funcdescrptr[1];
   RELO_LOG(reloRuntime->reloLogger(), 6, "\tpreparePrivateData: arraycopy toc %p\n", reloPrivateData->_addressToPatch);
   }

// TR::RamMethodSequence
char *
TR::RelocationRecordRamSequence::name()
   {
   return "TR::RamMethodSequence";
   }

void
TR::RelocationRecordRamSequence::preparePrivateData(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget)
   {
   TR::RelocationRecordWithOffsetPrivateData *reloPrivateData = &(privateData()->offset);
   reloPrivateData->_addressToPatch = (uint8_t *)reloRuntime->exceptionTable()->ramMethod;
   RELO_LOG(reloRuntime->reloLogger(), 6, "\tpreparePrivateData: j9method %p\n", reloPrivateData->_addressToPatch);
   }

// WithInlinedSiteIndex
void
TR::RelocationRecordWithInlinedSiteIndex::print(TR::RelocationRuntime *reloRuntime)
   {
   TR::RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR::RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR::RelocationRecord::print(reloRuntime);
   reloLogger->printf("\tinlined site index %p\n", inlinedSiteIndex(reloTarget));
   }

void
TR::RelocationRecordWithInlinedSiteIndex::setInlinedSiteIndex(TR::RelocationTarget *reloTarget, uintptrj_t inlinedSiteIndex)
   {
   reloTarget->storeRelocationRecordValue(inlinedSiteIndex, (uintptrj_t *) &((TR::RelocationRecordWithInlinedSiteIndexBinaryTemplate *)_record)->_inlinedSiteIndex);
   }

uintptrj_t
TR::RelocationRecordWithInlinedSiteIndex::inlinedSiteIndex(TR::RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR::RelocationRecordWithInlinedSiteIndexBinaryTemplate *)_record)->_inlinedSiteIndex);
   }

int32_t
TR::RelocationRecordWithInlinedSiteIndex::bytesInHeaderAndPayload()
   {
   return sizeof(TR::RelocationRecordWithInlinedSiteIndexBinaryTemplate);
   }

TR::OpaqueMethodBlock *
TR::RelocationRecordWithInlinedSiteIndex::getInlinedSiteCallerMethod(TR::RelocationRuntime *reloRuntime)
   {
   uintptrj_t siteIndex = inlinedSiteIndex(reloRuntime->reloTarget());
   TR::InlinedCallSite *inlinedCallSite = (TR::InlinedCallSite *)getInlinedCallSiteArrayElement(reloRuntime->exceptionTable(), siteIndex);
   uintptrj_t callerIndex = inlinedCallSite->_byteCodeInfo.getCallerIndex();
   return getInlinedSiteMethod(reloRuntime, callerIndex);
   }

TR::OpaqueMethodBlock *
TR::RelocationRecordWithInlinedSiteIndex::getInlinedSiteMethod(TR::RelocationRuntime *reloRuntime)
   {
   return getInlinedSiteMethod(reloRuntime, inlinedSiteIndex(reloRuntime->reloTarget()));
   }

TR::OpaqueMethodBlock *
TR::RelocationRecordWithInlinedSiteIndex::getInlinedSiteMethod(TR::RelocationRuntime *reloRuntime, uintptrj_t siteIndex)
   {
   TR::OpaqueMethodBlock *method = (TR::OpaqueMethodBlock *) reloRuntime->method();
   if (siteIndex != (uintptrj_t)-1)
      {
      TR::InlinedCallSite *inlinedCallSite = (TR::InlinedCallSite *)getInlinedCallSiteArrayElement(reloRuntime->exceptionTable(), siteIndex);
      method = inlinedCallSite->_methodInfo;
      }
   return method;
   }

bool
TR::RelocationRecordWithInlinedSiteIndex::ignore(TR::RelocationRuntime *reloRuntime)
   {
   J9Method *method = (J9Method *)getInlinedSiteMethod(reloRuntime);

   if (method == reinterpret_cast<J9Method *>(-1))
      {
      if (reloRuntime->comp()->getOption(TR::UseSymbolValidationManager))
         {
         /* With the SVM, it isn't possible for the method to be equal to -1
          * because:
          *
          * 1. The shape of the class of the method is guaranteed by the validation
          *    records, which are processed first.
          * 2. The inlined table will be populated regardless of whether the guard
          *    for the inlined site has to be patched.
          */
         TR::ASSERT(false, "inlined site method should not be -1!\n");
         reloRuntime->comp()->failCompilation<J9::AOTSymbolValidationManagerFailure>("getInlinedSiteMethod returned method == -1");
         }
      else
         {
         // -1 means this inlined method isn't active, so can ignore relocations associated with it
         return true;
         }
      }

   /* It is safe to return true here because if classes were unloaded, then
    * the compilation will be aborted and the potentially unrelocated sections
    * of code will never be executed.
    */
   if (isUnloadedInlinedMethod(method))
      return true;

   return false;
   }



// Constant Pool relocations
char *
TR::RelocationRecordConstantPool::name()
   {
   return "TR::ConstantPool";
   }

void
TR::RelocationRecordConstantPool::print(TR::RelocationRuntime *reloRuntime)
   {
   TR::RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR::RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR::RelocationRecordWithInlinedSiteIndex::print(reloRuntime);
   reloLogger->printf("\tconstant pool %p\n", constantPool(reloTarget));
   }

int32_t
TR::RelocationRecordConstantPool::bytesInHeaderAndPayload()
   {
   return sizeof(TR::RelocationRecordConstantPoolBinaryTemplate);
   }

void
TR::RelocationRecordConstantPool::setConstantPool(TR::RelocationTarget *reloTarget, uintptrj_t constantPool)
   {
   reloTarget->storeRelocationRecordValue(constantPool, (uintptrj_t *) &((TR::RelocationRecordConstantPoolBinaryTemplate *)_record)->_constantPool);
   }

uintptrj_t
TR::RelocationRecordConstantPool::constantPool(TR::RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR::RelocationRecordConstantPoolBinaryTemplate *)_record)->_constantPool);
   }

uintptrj_t
TR::RelocationRecordConstantPool::currentConstantPool(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uintptrj_t oldValue)
   {
   uintptrj_t oldCPBase = constantPool(reloTarget);
   uintptrj_t newCP = oldValue - oldCPBase + (uintptrj_t)reloRuntime->ramCP();

   return newCP;
   }

uintptrj_t
TR::RelocationRecordConstantPool::findConstantPool(TR::RelocationTarget *reloTarget, uintptrj_t oldValue, TR::OpaqueMethodBlock *ramMethod)
   {
   uintptrj_t oldCPBase = constantPool(reloTarget);
   uintptrj_t methodCP = oldValue - oldCPBase + (uintptrj_t)J9_CP_FROM_METHOD((J9Method *)ramMethod);
   return methodCP;
   }


uintptrj_t
TR::RelocationRecordConstantPool::computeNewConstantPool(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uintptrj_t oldConstantPool)
   {
   uintptrj_t newCP;
   UDATA thisInlinedSiteIndex = (UDATA) inlinedSiteIndex(reloTarget);
   if (thisInlinedSiteIndex != (UDATA) -1)
      {
      // Find CP from inlined method
      // Assume that the inlined call site has already been relocated
      // And assumes that the method is resolved already, otherwise, we would not have properly relocated the
      // ramMethod for the inlined callsite and trying to retreive stuff from the bogus pointer will result in error
      TR::InlinedCallSite *inlinedCallSite = (TR::InlinedCallSite *)getInlinedCallSiteArrayElement(reloRuntime->exceptionTable(), thisInlinedSiteIndex);
      J9Method *ramMethod = (J9Method *) inlinedCallSite->_methodInfo;

      if (!isUnloadedInlinedMethod(ramMethod))
         {
         newCP = findConstantPool(reloTarget, oldConstantPool, (TR::OpaqueMethodBlock *) ramMethod);
         }
      else
         {
         RELO_LOG(reloRuntime->reloLogger(), 1, "\t\tcomputeNewConstantPool: method has been unloaded\n");
         return 0;
         }
      }
   else
      {
      newCP = currentConstantPool(reloRuntime, reloTarget, oldConstantPool);
      }

   RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tcomputeNewConstantPool: newCP %p\n", newCP);
   return newCP;
   }

int32_t
TR::RelocationRecordConstantPool::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint8_t *baseLocation = 0;
   if (eipRelative(reloTarget))
      {
      //j9tty_printf(PORTLIB, "\nInternal Error AOT: relocateConstantPool: Relocation type was IP-relative.\n");
      // TODO: better error condition exit(-1);
      return 0;
      }

   uintptrj_t oldValue =  (uintptrj_t) reloTarget->loadAddress(reloLocation);
   uintptrj_t newCP = computeNewConstantPool(reloRuntime, reloTarget, oldValue);
   reloTarget->storeAddress((uint8_t *)newCP, reloLocation);

   return 0;
   }

int32_t
TR::RelocationRecordConstantPool::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   if (eipRelative(reloTarget))
      {
      //j9tty_printf(PORTLIB, "\nInternal Error AOT: relocateConstantPool: Relocation type was IP-relative.\n");
      // TODO: better error condition exit(-1);
      return 0;
      }

   uintptrj_t oldValue = (uintptrj_t) reloTarget->loadAddress(reloLocationHigh, reloLocationLow);
   uintptrj_t newCP = computeNewConstantPool(reloRuntime, reloTarget, oldValue);
   reloTarget->storeAddress((uint8_t *)newCP, reloLocationHigh, reloLocationLow, reloFlags(reloTarget));

   return 0;
   }

// ConstantPoolWithIndex relocation base class
void
TR::RelocationRecordConstantPoolWithIndex::print(TR::RelocationRuntime *reloRuntime)
   {
   TR::RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR::RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR::RelocationRecordConstantPool::print(reloRuntime);
   reloLogger->printf("\tcpIndex %p\n", cpIndex(reloTarget));
   }

void
TR::RelocationRecordConstantPoolWithIndex::setCpIndex(TR::RelocationTarget *reloTarget, uintptrj_t cpIndex)
   {
   reloTarget->storeRelocationRecordValue(cpIndex, (uintptrj_t *) &((TR::RelocationRecordConstantPoolWithIndexBinaryTemplate *)_record)->_index);
   }

uintptrj_t
TR::RelocationRecordConstantPoolWithIndex::cpIndex(TR::RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR::RelocationRecordConstantPoolWithIndexBinaryTemplate *)_record)->_index);
   }

int32_t
TR::RelocationRecordConstantPoolWithIndex::bytesInHeaderAndPayload()
   {
   return sizeof(TR::RelocationRecordConstantPoolWithIndexBinaryTemplate);
   }

TR::OpaqueMethodBlock *
TR::RelocationRecordConstantPoolWithIndex::getSpecialMethodFromCP(TR::RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex)
   {
   TR::VMAccessCriticalSection getSpecialMethodFromCP(reloRuntime->fej9());
   J9ConstantPool *cp = (J9ConstantPool *) void_cp;
   TR::RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();

   J9VMThread *vmThread = reloRuntime->currentThread();
   TR::OpaqueMethodBlock *method = (TR::OpaqueMethodBlock *) jitResolveSpecialMethodRef(vmThread, cp, cpIndex, J9_RESOLVE_FLAG_AOT_LOAD_TIME);
   RELO_LOG(reloLogger, 6, "\tgetMethodFromCP: found special method %p\n", method);
   return method;
   }

TR::OpaqueMethodBlock *
TR::RelocationRecordConstantPoolWithIndex::getVirtualMethodFromCP(TR::RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex)
   {
   J9JavaVM *javaVM = reloRuntime->javaVM();
   J9ConstantPool *cp = (J9ConstantPool *) void_cp;
   TR::RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();

   J9Method *method = NULL;

      {
      TR::VMAccessCriticalSection getVirtualMethodFromCP(reloRuntime->fej9());
      UDATA vTableOffset = javaVM->internalVMFunctions->resolveVirtualMethodRefInto(javaVM->internalVMFunctions->currentVMThread(javaVM),
                                                                                   cp,
                                                                                   cpIndex,
                                                                                   J9_RESOLVE_FLAG_AOT_LOAD_TIME,
                                                                                   &method,
                                                                                   NULL);
      }

   if (method)
       {
       if ((UDATA)method->constantPool & J9_STARTPC_METHOD_IS_OVERRIDDEN)
          {
          RELO_LOG(reloLogger, 6, "\tgetMethodFromCP: inlined method overridden, fail validation\n");
          method = NULL;
          }
       else
          {
          RELO_LOG(reloLogger, 6, "\tgetMethodFromCP: found virtual method %p\n", method);
          }
       }

   return (TR::OpaqueMethodBlock *) method;
   }

TR::OpaqueMethodBlock *
TR::RelocationRecordConstantPoolWithIndex::getStaticMethodFromCP(TR::RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex)
   {
   TR::VMAccessCriticalSection getStaticMethodFromCP(reloRuntime->fej9());
   J9JavaVM *javaVM = reloRuntime->javaVM();
   J9ConstantPool *cp = (J9ConstantPool *) void_cp;
   TR::RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();

   TR::OpaqueMethodBlock *method = (TR::OpaqueMethodBlock *) jitResolveStaticMethodRef(javaVM->internalVMFunctions->currentVMThread(javaVM),
                                                                                     cp,
                                                                                     cpIndex,
                                                                                     J9_RESOLVE_FLAG_AOT_LOAD_TIME);
   RELO_LOG(reloLogger, 6, "\tgetMethodFromCP: found static method %p\n", method);
   return method;
   }

TR::OpaqueMethodBlock *
TR::RelocationRecordConstantPoolWithIndex::getInterfaceMethodFromCP(TR::RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex, TR::OpaqueMethodBlock *callerMethod)
   {
   TR::RelocationRecordInlinedMethodPrivateData *reloPrivateData = &(privateData()->inlinedMethod);

   J9JavaVM *javaVM = reloRuntime->javaVM();
   TR::J9VMBase *fe = reloRuntime->fej9();
   TR::Memory *trMemory = reloRuntime->trMemory();

   J9ConstantPool *cp = (J9ConstantPool *) void_cp;
   J9ROMMethodRef *romMethodRef = (J9ROMMethodRef *)&cp->romConstantPool[cpIndex];

   TR::OpaqueClassBlock *interfaceClass;

      {
      TR::VMAccessCriticalSection getInterfaceMethodFromCP(reloRuntime->fej9());
      interfaceClass = (TR::OpaqueClassBlock *) javaVM->internalVMFunctions->resolveClassRef(reloRuntime->currentThread(),
                                                                                            cp,
                                                                                            romMethodRef->classRefCPIndex,
                                                                                            J9_RESOLVE_FLAG_AOT_LOAD_TIME);
      }

   TR::RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   RELO_LOG(reloLogger, 6, "\tgetMethodFromCP: interface class %p\n", interfaceClass);

   TR::OpaqueMethodBlock *calleeMethod = NULL;
   if (interfaceClass)
      {
      TR::PersistentCHTable * chTable = reloRuntime->getPersistentInfo()->getPersistentCHTable();
      TR::ResolvedMethod *callerResolvedMethod = fe->createResolvedMethod(trMemory, callerMethod, NULL);

      TR::ResolvedMethod *calleeResolvedMethod = chTable->findSingleInterfaceImplementer(interfaceClass, cpIndex, callerResolvedMethod, reloRuntime->comp(), false, false);

      if (calleeResolvedMethod)
         {
         if (!calleeResolvedMethod->virtualMethodIsOverridden())
            calleeMethod = calleeResolvedMethod->getPersistentIdentifier();
         else
            RELO_LOG(reloLogger, 6, "\tgetMethodFromCP: callee method overridden\n");
         }
      }


   reloPrivateData->_receiverClass = interfaceClass;
   return calleeMethod;
   }

TR::OpaqueMethodBlock *
TR::RelocationRecordConstantPoolWithIndex::getAbstractMethodFromCP(TR::RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex, TR::OpaqueMethodBlock *callerMethod)
   {
   TR::RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR::RelocationRecordInlinedMethodPrivateData *reloPrivateData = &(privateData()->inlinedMethod);

   J9JavaVM *javaVM = reloRuntime->javaVM();
   TR::J9VMBase *fe = reloRuntime->fej9();
   TR::Memory *trMemory = reloRuntime->trMemory();

   J9ConstantPool *cp = (J9ConstantPool *) void_cp;
   J9ROMMethodRef *romMethodRef = (J9ROMMethodRef *)&cp->romConstantPool[cpIndex];

   TR::OpaqueMethodBlock *calleeMethod = NULL;
   TR::OpaqueClassBlock *abstractClass = NULL;
   UDATA vTableOffset = (UDATA)-1;
   J9Method *method = NULL;

      {
      TR::VMAccessCriticalSection getAbstractlMethodFromCP(reloRuntime->fej9());
      abstractClass = (TR::OpaqueClassBlock *) javaVM->internalVMFunctions->resolveClassRef(reloRuntime->currentThread(),
                                                                                            cp,
                                                                                            romMethodRef->classRefCPIndex,
                                                                                            J9_RESOLVE_FLAG_AOT_LOAD_TIME);

      vTableOffset = javaVM->internalVMFunctions->resolveVirtualMethodRefInto(reloRuntime->currentThread(),
                                                                              cp,
                                                                              cpIndex,
                                                                              J9_RESOLVE_FLAG_AOT_LOAD_TIME,
                                                                              &method,
                                                                              NULL);
      }

   if (abstractClass && method)
      {
      int32_t vftSlot = (int32_t)(-(vTableOffset - J9JIT_INTERP_VTABLE_OFFSET));
      TR::PersistentCHTable * chTable = reloRuntime->getPersistentInfo()->getPersistentCHTable();
      TR::ResolvedMethod *callerResolvedMethod = fe->createResolvedMethod(trMemory, callerMethod, NULL);

      TR::ResolvedMethod *calleeResolvedMethod = chTable->findSingleAbstractImplementer(abstractClass, vftSlot, callerResolvedMethod, reloRuntime->comp(), false, false);

      if (calleeResolvedMethod)
         {
         if (!calleeResolvedMethod->virtualMethodIsOverridden())
            calleeMethod = calleeResolvedMethod->getPersistentIdentifier();
         else
            RELO_LOG(reloLogger, 6, "\tgetMethodFromCP: callee method overridden\n");
         }
      }

   reloPrivateData->_receiverClass = abstractClass;
   return calleeMethod;
   }

// TR::HelperAddress
char *
TR::RelocationRecordHelperAddress::name()
   {
   return "TR::HelperAddress";
   }

void
TR::RelocationRecordHelperAddress::print(TR::RelocationRuntime *reloRuntime)
   {
   TR::RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR::RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR::RelocationRecord::print(reloRuntime);
   uintptrj_t helper = helperID(reloTarget);
   if (reloRuntime->comp())
      reloLogger->printf("\thelper %d %s\n", helper, reloRuntime->comp()->findOrCreateDebug()->getRuntimeHelperName(helper));
   else
      reloLogger->printf("\thelper %d\n", helper);
   }

int32_t
TR::RelocationRecordHelperAddress::bytesInHeaderAndPayload()
   {
   return sizeof(TR::RelocationRecordHelperAddressBinaryTemplate);
   }

void
TR::RelocationRecordHelperAddress::setHelperID(TR::RelocationTarget *reloTarget, uint32_t helperID)
   {
   reloTarget->storeUnsigned32b(helperID, (uint8_t *) &(((TR::RelocationRecordHelperAddressBinaryTemplate *)_record)->_helperID));
   }

uint32_t
TR::RelocationRecordHelperAddress::helperID(TR::RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned32b((uint8_t *) &(((TR::RelocationRecordHelperAddressBinaryTemplate *)_record)->_helperID));
   }

void
TR::RelocationRecordHelperAddress::preparePrivateData(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget)
   {
   TR::RelocationRecordHelperAddressPrivateData *reloPrivateData = &(privateData()->helperAddress);

   J9JITConfig *jitConfig = reloRuntime->jitConfig();
   TR::ASSERT(jitConfig != NULL, "Relocation runtime doesn't have a jitConfig!");
   J9JavaVM *javaVM = jitConfig->javaVM;
   reloPrivateData->_helperID = helperID(reloTarget);
   reloPrivateData->_helper = (uint8_t *) (jitConfig->aotrt_getRuntimeHelper)(reloPrivateData->_helperID);
   RELO_LOG(reloRuntime->reloLogger(), 6, "\tpreparePrivateData: helperAddress %p\n", reloPrivateData->_helper);
   }

int32_t
TR::RelocationRecordHelperAddress::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint8_t *baseLocation = 0;
   if (eipRelative(reloTarget))
      baseLocation = reloTarget->eipBaseForCallOffset(reloLocation);

   uint8_t *helperAddress = (uint8_t *)computeHelperAddress(reloRuntime, reloTarget, baseLocation);
   uint8_t *helperOffset = helperAddress - (uintptrj_t)baseLocation;
   RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tapplyRelocation: baseLocation %p helperAddress %p helperOffset %x\n", baseLocation, helperAddress, helperOffset);

   if (eipRelative(reloTarget))
      reloTarget->storeRelativeTarget((uintptr_t )helperOffset, reloLocation);
   else
      reloTarget->storeAddress(helperOffset, reloLocation);

   return 0;
   }

int32_t
TR::RelocationRecordHelperAddress::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   TR::ASSERT(0, "TR::RelocationRecordHelperAddress::applyRelocation for ordered pair, we should never call this");
   uint8_t *baseLocation = 0;

   uint8_t *helperOffset = (uint8_t *) ((uintptr_t)computeHelperAddress(reloRuntime, reloTarget, baseLocation) - (uintptr_t)baseLocation);

   reloTarget->storeAddress(helperOffset, reloLocationHigh, reloLocationLow, reloFlags(reloTarget));

   return 0;
   }

char *
TR::RelocationRecordAbsoluteHelperAddress::name()
   {
   return "TR::AbsoluteHelperAddress";
   }

int32_t
TR::RelocationRecordAbsoluteHelperAddress::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR::RelocationRecordHelperAddressPrivateData *reloPrivateData = &(privateData()->helperAddress);
   uint8_t *helperAddress = reloPrivateData->_helper;

   reloTarget->storeAddress(helperAddress, reloLocation);
   return 0;
   }

int32_t
TR::RelocationRecordAbsoluteHelperAddress::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   TR::RelocationRecordHelperAddressPrivateData *reloPrivateData = &(privateData()->helperAddress);
   uint8_t *helperAddress = reloPrivateData->_helper;
   reloTarget->storeAddress(helperAddress, reloLocationHigh, reloLocationLow, reloFlags(reloTarget));
   return 0;
   }

// Method Address Relocations
//
char *
TR::RelocationRecordMethodAddress::name()
   {
   return "TR::MethodAddress";
   }

uint8_t *
TR::RelocationRecordMethodAddress::currentMethodAddress(TR::RelocationRuntime *reloRuntime, uint8_t *oldMethodAddress)
   {
   TR::AOTMethodHeader *methodHdr = reloRuntime->aotMethodHeaderEntry();
   return oldMethodAddress - methodHdr->compileMethodCodeStartPC + (uintptr_t) reloRuntime->newMethodCodeStart();
   }

int32_t
TR::RelocationRecordMethodAddress::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   bool eipRel = eipRelative(reloTarget);

   uint8_t *oldAddress;
   if (eipRel)
      oldAddress = reloTarget->loadCallTarget(reloLocation);
   else
      oldAddress = reloTarget->loadAddress(reloLocation);

   RELO_LOG(reloRuntime->reloLogger(), 5, "\t\tapplyRelocation: old method address %p\n", oldAddress);
   uint8_t *newAddress = currentMethodAddress(reloRuntime, oldAddress);
   RELO_LOG(reloRuntime->reloLogger(), 5, "\t\tapplyRelocation: new method address %p\n", newAddress);

   if (eipRel)
      reloTarget->storeCallTarget((uintptr_t)newAddress, reloLocation);
   else
      reloTarget->storeAddress(newAddress, reloLocation);

   return 0;
   }

int32_t
TR::RelocationRecordMethodAddress::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   TR::RelocationRecordWithOffsetPrivateData *reloPrivateData = &(privateData()->offset);
   uint8_t *oldAddress = reloTarget->loadAddress(reloLocationHigh, reloLocationLow);
   uint8_t *newAddress = currentMethodAddress(reloRuntime, oldAddress);

   RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tapplyRelocation: oldAddress %p newAddress %p\n", oldAddress, newAddress);
   reloTarget->storeAddress(newAddress, reloLocationHigh, reloLocationLow, reloFlags(reloTarget));
   return 0;
   }

// Direct JNI Address Relocations

char *
TR::RelocationRecordDirectJNICall::name()
   {
   return "TR::JNITargetAddress";
   }

TR::OpaqueMethodBlock *
TR::RelocationRecordDirectJNICall::getMethodFromCP(TR::RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex)
   {
   TR::OpaqueMethodBlock *method = NULL;

   return method;
   }

TR::OpaqueMethodBlock *
TR::RelocationRecordDirectJNISpecialMethodCall::getMethodFromCP(TR::RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex)
   {
   return getSpecialMethodFromCP(reloRuntime, void_cp, cpIndex);
   }

TR::OpaqueMethodBlock *
TR::RelocationRecordDirectJNIStaticMethodCall::getMethodFromCP(TR::RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex)
   {
   return getStaticMethodFromCP(reloRuntime, void_cp, cpIndex);
   }

TR::OpaqueMethodBlock *
TR::RelocationRecordDirectJNIVirtualMethodCall::getMethodFromCP(TR::RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex)
   {
   return getVirtualMethodFromCP(reloRuntime, void_cp, cpIndex);
   }

int32_t TR::RelocationRecordRamMethodConst::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR::RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   J9ConstantPool * newConstantPool =(J9ConstantPool *) computeNewConstantPool(reloRuntime, reloTarget, constantPool(reloTarget));
   TR::OpaqueMethodBlock *ramMethod = getMethodFromCP(reloRuntime, newConstantPool, cpIndex(reloTarget));

   if (!ramMethod)
      {
      return compilationAotClassReloFailure;
      }

   reloTarget->storeAddressRAM((uint8_t *)ramMethod, reloLocation);
   return 0;
   }

int32_t
TR::RelocationRecordDirectJNICall::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uintptrj_t oldAddress = (uintptrj_t) reloTarget->loadAddress(reloLocation);

   TR::RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   J9ConstantPool * newConstantPool =(J9ConstantPool *) computeNewConstantPool(reloRuntime, reloTarget, constantPool(reloTarget));
   TR::OpaqueMethodBlock *ramMethod = getMethodFromCP(reloRuntime, newConstantPool, cpIndex(reloTarget));

   if (!ramMethod) return compilationAotClassReloFailure;


   TR::ResolvedMethod *callerResolvedMethod = reloRuntime->fej9()->createResolvedMethod(reloRuntime->comp()->trMemory(), ramMethod, NULL);
   void * newAddress = NULL;
   if (callerResolvedMethod->isJNINative())
      newAddress = callerResolvedMethod->startAddressForJNIMethod(reloRuntime->comp());


   if (!newAddress) return compilationAotClassReloFailure;

   RELO_LOG(reloLogger, 6, "\tJNI call relocation: found JNI target address %p\n", newAddress);

   createJNICallSite((void *)ramMethod, (void *)reloLocation,getMetadataAssumptionList(reloRuntime->exceptionTable()));
   RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tapplyRelocation: registered JNI Call redefinition site\n");

   reloTarget->storeRelativeAddressSequence((uint8_t *)newAddress, reloLocation, fixedSequence1);
   return 0;

   }


// Data Address Relocations
char *
TR::RelocationRecordDataAddress::name()
   {
   return "TR::DataAddress";
   }

void
TR::RelocationRecordDataAddress::print(TR::RelocationRuntime *reloRuntime)
   {
   TR::RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR::RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR::RelocationRecordConstantPoolWithIndex::print(reloRuntime);
   reloLogger->printf("\toffset %p\n", offset(reloTarget));
   }

void
TR::RelocationRecordDataAddress::setOffset(TR::RelocationTarget *reloTarget, uintptrj_t offset)
   {
   reloTarget->storeRelocationRecordValue(offset, (uintptrj_t *) &((TR::RelocationRecordDataAddressBinaryTemplate *)_record)->_offset);
   }

uintptrj_t
TR::RelocationRecordDataAddress::offset(TR::RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR::RelocationRecordDataAddressBinaryTemplate *)_record)->_offset);
   }

int32_t
TR::RelocationRecordDataAddress::bytesInHeaderAndPayload()
   {
   return sizeof(TR::RelocationRecordDataAddressBinaryTemplate);
   }

uint8_t *
TR::RelocationRecordDataAddress::findDataAddress(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget)
   {
   J9JITConfig *jitConfig = reloRuntime->jitConfig();
   J9JavaVM *javaVM = jitConfig->javaVM;
   J9ROMFieldShape * fieldShape = 0;
   UDATA cpindex = cpIndex(reloTarget);
   J9ConstantPool *cp =  (J9ConstantPool *)computeNewConstantPool(reloRuntime, reloTarget, constantPool(reloTarget));

   UDATA extraOffset = offset(reloTarget);
   uint8_t *address = NULL;

   if (cp)
      {
      TR::VMAccessCriticalSection findDataAddress(reloRuntime->fej9());
      J9VMThread *vmThread = reloRuntime->currentThread();
      J9Method *ramMethod;
      UDATA thisInlinedSiteIndex = (UDATA) inlinedSiteIndex(reloTarget);
      if (thisInlinedSiteIndex != (UDATA) -1) // Inlined method
         {
         TR::InlinedCallSite *inlinedCallSite = (TR::InlinedCallSite *)getInlinedCallSiteArrayElement(reloRuntime->exceptionTable(), thisInlinedSiteIndex);
         ramMethod = (J9Method *) inlinedCallSite->_methodInfo;
         }
      else
         {
         ramMethod = reloRuntime->method();
         }
      if (ramMethod && (ramMethod != reinterpret_cast<J9Method *>(-1)))
         address = (uint8_t *)jitCTResolveStaticFieldRefWithMethod(vmThread, ramMethod, cpindex, false, &fieldShape);
      }

   if (address == NULL)
      {
      RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tfindDataAddress: unresolved\n");
      return 0;
      }

   address = address + extraOffset;
   RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tfindDataAddress: field address %p\n", address);
   return address;
   }

int32_t
TR::RelocationRecordDataAddress::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint8_t *newAddress = findDataAddress(reloRuntime, reloTarget);

   if (!newAddress)
      return compilationAotStaticFieldReloFailure;

   TR::AOTStats *aotStats = reloRuntime->aotStats();
   if (aotStats)
      {
      aotStats->numRuntimeClassAddressReloUnresolvedCP++;
      }

   reloTarget->storeAddressSequence(newAddress, reloLocation, reloFlags(reloTarget));
   return 0;
   }

int32_t
TR::RelocationRecordDataAddress::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   uint8_t *newAddress = findDataAddress(reloRuntime, reloTarget);

   if (!newAddress)
      return compilationAotStaticFieldReloFailure;
   reloTarget->storeAddress(newAddress, reloLocationHigh, reloLocationLow, reloFlags(reloTarget));
   return 0;
   }

// Class Object Relocations
char *
TR::RelocationRecordClassObject::name()
   {
   return "TR::ClassObject";
   }

TR::OpaqueClassBlock *
TR::RelocationRecordClassObject::computeNewClassObject(TR::RelocationRuntime *reloRuntime, uintptrj_t newConstantPool, uintptrj_t inlinedSiteIndex, uintptrj_t cpIndex)
   {
   J9JavaVM *javaVM = reloRuntime->jitConfig()->javaVM;

   TR::AOTStats *aotStats = reloRuntime->aotStats();

   if (!newConstantPool)
      {
      if (aotStats)
         {
         aotStats->numRuntimeClassAddressReloUnresolvedCP++;
         }
      return 0;
      }
   J9VMThread *vmThread = reloRuntime->currentThread();

   J9Class *resolvedClass;

      {
      TR::VMAccessCriticalSection computeNewClassObject(reloRuntime->fej9());
      resolvedClass = javaVM->internalVMFunctions->resolveClassRef(vmThread, (J9ConstantPool *)newConstantPool, cpIndex, J9_RESOLVE_FLAG_AOT_LOAD_TIME);
      }

   RELO_LOG(reloRuntime->reloLogger(), 6,"\tcomputeNewClassObject: resolvedClass %p\n", resolvedClass);

   if (resolvedClass)
      {
      RELO_LOG(reloRuntime->reloLogger(), 6,"\tcomputeNewClassObject: resolvedClassName %.*s\n",
                                              J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(resolvedClass->romClass)),
                                              J9UTF8_DATA(J9ROMCLASS_CLASSNAME(resolvedClass->romClass)));
      }
   else if (aotStats)
      {
      aotStats->numRuntimeClassAddressReloUnresolvedClass++;
      }

   return (TR::OpaqueClassBlock *)resolvedClass;
   }

int32_t
TR::RelocationRecordClassObject::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uintptrj_t oldAddress = (uintptrj_t) reloTarget->loadAddress(reloLocation);

   uintptrj_t newConstantPool = computeNewConstantPool(reloRuntime, reloTarget, constantPool(reloTarget));
   TR::OpaqueClassBlock *newAddress = computeNewClassObject(reloRuntime, newConstantPool, inlinedSiteIndex(reloTarget), cpIndex(reloTarget));

   if (!newAddress) return compilationAotClassReloFailure;

   if (TR::CodeGenerator::wantToPatchClassPointer(reloRuntime->comp(), newAddress, reloLocation))
      {
      createClassRedefinitionPicSite((void *)newAddress, (void *)reloLocation, sizeof(UDATA), 0,
                                     getMetadataAssumptionList(reloRuntime->exceptionTable()));
      RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tapplyRelocation: hcr enabled, registered class redefinition site\n");
      }

   reloTarget->storeAddressSequence((uint8_t *)newAddress, reloLocation, reloFlags(reloTarget));
   return 0;
   }

int32_t
TR::RelocationRecordClassObject::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   uintptrj_t oldValue = (uintptrj_t) reloTarget->loadAddress(reloLocationHigh, reloLocationLow);
   uintptrj_t newConstantPool = computeNewConstantPool(reloRuntime, reloTarget, oldValue);
   TR::OpaqueClassBlock *newAddress = computeNewClassObject(reloRuntime, newConstantPool, inlinedSiteIndex(reloTarget), cpIndex(reloTarget));

   if (!newAddress) return compilationAotClassReloFailure;

   if (TR::CodeGenerator::wantToPatchClassPointer(reloRuntime->comp(), newAddress, reloLocationHigh))
      {
      // This looks wrong
      createClassRedefinitionPicSite((void *)newAddress, (void *)reloLocationHigh, sizeof(UDATA), 0,
         getMetadataAssumptionList(reloRuntime->exceptionTable()));
      RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tapplyRelocation: hcr enabled, registered class redefinition site\n");
      }

   reloTarget->storeAddress((uint8_t *) newAddress, reloLocationHigh, reloLocationLow, reloFlags(reloTarget));
   return 0;
   }

// MethodObject Relocations
char *
TR::RelocationRecordMethodObject::name()
   {
   return "TR::MethodObject";
   }

int32_t
TR::RelocationRecordMethodObject::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uintptrj_t oldAddress = (uintptrj_t) reloTarget->loadAddress(reloLocation);
   uintptrj_t newAddress = currentConstantPool(reloRuntime, reloTarget, oldAddress);
   reloTarget->storeAddress((uint8_t *) newAddress, reloLocation);
   return 0;
   }

int32_t
TR::RelocationRecordMethodObject::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   uintptrj_t oldAddress = (uintptrj_t) reloTarget->loadAddress(reloLocationHigh, reloLocationLow);
   uintptrj_t newAddress = currentConstantPool(reloRuntime, reloTarget, oldAddress);
   reloTarget->storeAddress((uint8_t *) newAddress, reloLocationHigh, reloLocationLow, reloFlags(reloTarget));
   return 0;
   }

// TR::BodyInfoAddress Relocation
char *
TR::RelocationRecordBodyInfo::name()
   {
   return "TR::BodyInfo";
   }

int32_t
TR::RelocationRecordBodyInfo::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   J9JITExceptionTable *exceptionTable = reloRuntime->exceptionTable();
   reloTarget->storeAddress((uint8_t *) exceptionTable->bodyInfo, reloLocation);
   fixPersistentMethodInfo((void *)exceptionTable);
   return 0;
   }

// TR::Thunks Relocation
char *
TR::RelocationRecordThunks::name()
   {
   return "TR::Thunks";
   }

int32_t
TR::RelocationRecordThunks::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint8_t *oldAddress = reloTarget->loadAddress(reloLocation);

   RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tapplyRelocation: oldAddress %p\n", oldAddress);

   uintptrj_t newConstantPool = computeNewConstantPool(reloRuntime, reloTarget, constantPool(reloTarget));
   reloTarget->storeAddress((uint8_t *)newConstantPool, reloLocation);
   return relocateAndRegisterThunk(reloRuntime, reloTarget, newConstantPool, cpIndex, reloLocation);
   }

int32_t
TR::RelocationRecordThunks::relocateAndRegisterThunk(
   TR::RelocationRuntime *reloRuntime,
   TR::RelocationTarget *reloTarget,
   uintptrj_t cp,
  uintptr_t cpIndex,
   uint8_t *reloLocation)
   {
   J9JITConfig *jitConfig = reloRuntime->jitConfig();
   J9JavaVM *javaVM = reloRuntime->jitConfig()->javaVM;
   J9ConstantPool *constantPool = (J9ConstantPool *)cp;

   J9ROMClass * romClass = J9_CLASS_FROM_CP(constantPool)->romClass;
   J9ROMMethodRef *romMethodRef = &J9ROM_CP_BASE(romClass, J9ROMMethodRef)[cpIndex];
   J9ROMNameAndSignature * nameAndSignature = J9ROMMETHODREF_NAMEANDSIGNATURE(romMethodRef);

   bool matchFound = false;

   int32_t signatureLength = J9UTF8_LENGTH(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSignature));
   char *signatureString = (char *) &(J9UTF8_DATA(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSignature)));

   RELO_LOG(reloRuntime->reloLogger(), 6, "\t\trelocateAndRegisterThunk: %.*s%.*s\n", J9UTF8_LENGTH(J9ROMNAMEANDSIGNATURE_NAME(nameAndSignature)), &(J9UTF8_DATA(J9ROMNAMEANDSIGNATURE_NAME(nameAndSignature))),  signatureLength, signatureString);

   // Everything below is run with VM Access in hand
   TR::VMAccessCriticalSection relocateAndRegisterThunkCriticalSection(reloRuntime->fej9());

   void *existingThunk = j9ThunkLookupNameAndSig(jitConfig, nameAndSignature);
   if (existingThunk != NULL)
      {
      /* Matching thunk found */
      RELO_LOG(reloRuntime->reloLogger(), 6, "\t\t\trelocateAndRegisterThunk:found matching thunk %p\n", existingThunk);
      relocateJ2IVirtualThunkPointer(reloTarget, reloLocation, existingThunk);
      return 0; // return successful
      }

   // search shared cache for thunk, copy it over and create thunk entry
   J9SharedDataDescriptor firstDescriptor;
   firstDescriptor.address = NULL;

   javaVM->sharedClassConfig->findSharedData(reloRuntime->currentThread(),
                                             signatureString,
                                             signatureLength,
                                             J9SHR_DATA_TYPE_AOTTHUNK,
                                             false,
                                             &firstDescriptor,
                                             NULL);

   // if found thunk, then need to copy thunk into code cache, create thunk mapping, and register thunk mapping
   //
   if (firstDescriptor.address)
      {
      //Copy thunk from shared cache into local memory and relocate target address
      //
      uint8_t *coldCode;
      TR::CodeCache *codeCache = reloRuntime->codeCache();

      // Changed the code so that we fail this relocation/compilation if we cannot
      // allocate in the current code cache. The reason is that, when a a new code cache is needed
      // the reservation of the old cache is cancelled and further allocation attempts from
      // the old cache (which is not switched) will fail
      U_8 *thunkStart = TR::CodeCacheManager::instance()->allocateCodeMemory(firstDescriptor.length, 0, &codeCache, &coldCode, true);
      U_8 *thunkAddress;
      if (thunkStart)
         {
         // Relocate the thunk
         //
         RELO_LOG(reloRuntime->reloLogger(), 7, "\t\t\trelocateAndRegisterThunk: thunkStart from cache %p\n", thunkStart);
         memcpy(thunkStart, firstDescriptor.address, firstDescriptor.length);

         thunkAddress = thunkStart + 2*sizeof(I_32);

         RELO_LOG(reloRuntime->reloLogger(), 7, "\t\t\trelocateAndRegisterThunk: thunkAddress %p\n", thunkAddress);
         void *vmHelper = j9ThunkVMHelperFromSignature(jitConfig, signatureLength, signatureString);
         RELO_LOG(reloRuntime->reloLogger(), 7, "\t\t\trelocateAndRegisterThunk: vmHelper %p\n", vmHelper);
         reloTarget->performThunkRelocation(thunkAddress, (UDATA)vmHelper);

         j9ThunkNewNameAndSig(jitConfig, nameAndSignature, thunkAddress);

         if (J9_EVENT_IS_HOOKED(javaVM->hookInterface, J9HOOK_VM_DYNAMIC_CODE_LOAD))
            ALWAYS_TRIGGER_J9HOOK_VM_DYNAMIC_CODE_LOAD(javaVM->hookInterface, javaVM->internalVMFunctions->currentVMThread(javaVM), NULL, (void *) thunkAddress, *((uint32_t *)thunkAddress - 2), "JIT virtual thunk", NULL);

         relocateJ2IVirtualThunkPointer(reloTarget, reloLocation, thunkAddress);
         }
      else
         {
         codeCache->unreserve(); // cancel the reservation
         // return error
         return compilationAotCacheFullReloFailure;
         }

      }
    else
      {
      // return error
      return compilationAotThunkReloFailure;
      }

   return 0;
   }

// TR::J2IVirtualThunkPointer Relocation
char *
TR::RelocationRecordJ2IVirtualThunkPointer::name()
   {
   return "TR::J2IVirtualThunkPointer";
   }

int32_t
TR::RelocationRecordJ2IVirtualThunkPointer::bytesInHeaderAndPayload()
   {
   return sizeof(TR::RelocationRecordJ2IVirtualThunkPointerBinaryTemplate);
   }

void
TR::RelocationRecordJ2IVirtualThunkPointer::relocateJ2IVirtualThunkPointer(
   TR::RelocationTarget *reloTarget,
   uint8_t *reloLocation,
   void *thunk)
   {
   TR::ASSERT_FATAL(thunk != NULL, "expected a j2i virtual thunk for relocation\n");

   // For uniformity with TR::Thunks, the reloLocation is not the location of the
   // J2I thunk pointer, but rather the location of the constant pool address.
   // Find the J2I thunk pointer relative to that.
   reloLocation += offsetToJ2IVirtualThunkPointer(reloTarget);
   reloTarget->storeAddress((uint8_t *)thunk, reloLocation);
   }

uintptrj_t
TR::RelocationRecordJ2IVirtualThunkPointer::offsetToJ2IVirtualThunkPointer(
   TR::RelocationTarget *reloTarget)
   {
   auto recordData = (TR::RelocationRecordJ2IVirtualThunkPointerBinaryTemplate *)_record;
   auto offsetEA = (uintptrj_t *) &recordData->_offsetToJ2IVirtualThunkPointer;
   return reloTarget->loadRelocationRecordValue(offsetEA);
   }

// TR::PicTrampolines Relocation
char *
TR::RelocationRecordPicTrampolines::name()
   {
   return "TR::PicTrampolines";
   }

void
TR::RelocationRecordPicTrampolines::print(TR::RelocationRuntime *reloRuntime)
   {
   TR::RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR::RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR::RelocationRecord::print(reloRuntime);
   reloLogger->printf("\tnumTrampolines %d\n", numTrampolines(reloTarget));
   }

int32_t
TR::RelocationRecordPicTrampolines::bytesInHeaderAndPayload()
   {
   return sizeof(TR::RelocationRecordPicTrampolineBinaryTemplate);
   }

void
TR::RelocationRecordPicTrampolines::setNumTrampolines(TR::RelocationTarget *reloTarget, int numTrampolines)
   {
   reloTarget->storeUnsigned8b(numTrampolines, (uint8_t *) &(((TR::RelocationRecordPicTrampolineBinaryTemplate *)_record)->_numTrampolines));
   }

uint8_t
TR::RelocationRecordPicTrampolines::numTrampolines(TR::RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned8b((uint8_t *) &(((TR::RelocationRecordPicTrampolineBinaryTemplate *)_record)->_numTrampolines));
   }

int32_t
TR::RelocationRecordPicTrampolines::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   if (reloRuntime->codeCache()->reserveNTrampolines(numTrampolines(reloTarget)) != OMR::CodeCacheErrorCode::ERRORCODE_SUCCESS)
      {
      RELO_LOG(reloRuntime->reloLogger(), 1,"\t\tapplyRelocation: aborting AOT relocation because pic trampoline was not reserved. Will be retried.\n");
      return compilationAotPicTrampolineReloFailure;
      }

   return 0;
   }

// TR::Trampolines Relocation

char *
TR::RelocationRecordTrampolines::name()
   {
   return "TR::Trampolines";
   }

int32_t
TR::RelocationRecordTrampolines::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint8_t *oldAddress = reloTarget->loadAddress(reloLocation);

   RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tapplyRelocation: oldAddress %p\n", oldAddress);

   uintptrj_t newConstantPool = computeNewConstantPool(reloRuntime, reloTarget, constantPool(reloTarget));
   reloTarget->storeAddress((uint8_t *)newConstantPool, reloLocation); // Store the new CP address (in snippet)
   uint32_t cpIndex = (uint32_t) reloTarget->loadCPIndex(reloLocation);
   if (reloRuntime->codeCache()->reserveUnresolvedTrampoline((void *)newConstantPool, cpIndex) != OMR::CodeCacheErrorCode::ERRORCODE_SUCCESS)
      {
      RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tapplyRelocation: aborting AOT relocation because trampoline was not reserved. Will be retried.\n");
      return compilationAotTrampolineReloFailure;
      }

   return 0;
   }

// TR::InlinedAllocation relocation
void
TR::RelocationRecordInlinedAllocation::print(TR::RelocationRuntime *reloRuntime)
   {
   TR::RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR::RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR::RelocationRecordConstantPoolWithIndex::print(reloRuntime);
   reloLogger->printf("\tbranchOffset %p\n", branchOffset(reloTarget));
   }

int32_t
TR::RelocationRecordInlinedAllocation::bytesInHeaderAndPayload()
   {
   return sizeof(TR::RelocationRecordInlinedAllocationBinaryTemplate);
   }

void
TR::RelocationRecordInlinedAllocation::setBranchOffset(TR::RelocationTarget *reloTarget, uintptrj_t branchOffset)
   {
   reloTarget->storeRelocationRecordValue(branchOffset, (uintptrj_t *)&((TR::RelocationRecordInlinedAllocationBinaryTemplate *)_record)->_branchOffset);
   }

uintptrj_t
TR::RelocationRecordInlinedAllocation::branchOffset(TR::RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *)&((TR::RelocationRecordInlinedAllocationBinaryTemplate *)_record)->_branchOffset);
   }

void
TR::RelocationRecordInlinedAllocation::preparePrivateData(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget)
   {
   TR::RelocationRecordInlinedAllocationPrivateData *reloPrivateData = &(privateData()->inlinedAllocation);

   uintptrj_t oldValue = constantPool(reloTarget);
   J9ConstantPool *newConstantPool = (J9ConstantPool *) computeNewConstantPool(reloRuntime, reloTarget, constantPool(reloTarget));

   J9JavaVM *javaVM = reloRuntime->jitConfig()->javaVM;
   TR::J9VMBase *fe = reloRuntime->fej9();
   J9Class *clazz;

   if (reloRuntime->comp()->getOption(TR::UseSymbolValidationManager))
      {
      uint16_t classID = (uint16_t)cpIndex(reloTarget);
      clazz = (J9Class *)reloRuntime->comp()->getSymbolValidationManager()->getSymbolFromID(classID);
      }
   else
      {
      TR::VMAccessCriticalSection preparePrivateData(fe);
      clazz = javaVM->internalVMFunctions->resolveClassRef(javaVM->internalVMFunctions->currentVMThread(javaVM),
                                                                    newConstantPool,
                                                                    cpIndex(reloTarget),
                                                                    J9_RESOLVE_FLAG_AOT_LOAD_TIME);
      }

   bool inlinedCodeIsOkay = false;
   if (clazz)
      {
      RELO_LOG(reloRuntime->reloLogger(), 6, "\tpreparePrivateData: clazz %p %.*s\n",
                                                 clazz,
                                                 J9ROMCLASS_CLASSNAME(clazz->romClass)->length,
                                                 J9ROMCLASS_CLASSNAME(clazz->romClass)->data);

      if (verifyClass(reloRuntime, reloTarget, (TR::OpaqueClassBlock *)clazz))
         inlinedCodeIsOkay = true;
      }
   else
      RELO_LOG(reloRuntime->reloLogger(), 6, "\tpreparePrivateData: clazz NULL\n");

   RELO_LOG(reloRuntime->reloLogger(), 6, "\tpreparePrivateData: inlinedCodeIsOkay %d\n", inlinedCodeIsOkay);

   reloPrivateData->_inlinedCodeIsOkay = inlinedCodeIsOkay;
   }

bool
TR::RelocationRecordInlinedAllocation::verifyClass(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, TR::OpaqueClassBlock *clazz)
   {
   TR::ASSERT(0, "TR::RelocationRecordInlinedAllocation::verifyClass should never be called");
   return false;
   }

int32_t
TR::RelocationRecordInlinedAllocation::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR::RelocationRecordInlinedAllocationPrivateData *reloPrivateData = &(privateData()->inlinedAllocation);
   reloRuntime->incNumInlinedAllocRelos();

   if (!reloPrivateData->_inlinedCodeIsOkay)
      {
      uint8_t *destination = (uint8_t *) (reloLocation + (UDATA) branchOffset(reloTarget));

      RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tapplyRelocation: inlined alloc not OK, patch destination %p\n", destination);
      _patchVirtualGuard(reloLocation, destination, TR::Compiler->target.isSMP());
      reloRuntime->incNumFailedAllocInlinedRelos();
      }
   else
      {
      RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tapplyRelocation: inlined alloc looks OK\n");
      }
   return 0;
   }

// TR::VerifyRefArrayForAlloc Relocation
char *
TR::RelocationRecordVerifyRefArrayForAlloc::name()
   {
   return "TR::VerifyRefArrayForAlloc";
   }

bool
TR::RelocationRecordVerifyRefArrayForAlloc::verifyClass(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, TR::OpaqueClassBlock *clazz)
   {
   return (clazz && ((J9Class *)clazz)->arrayClass);
   }


// TR::VerifyClassObjectForAlloc Relocation
char *
TR::RelocationRecordVerifyClassObjectForAlloc::name()
   {
   return "TR::VerifyClassObjectForAlloc";
   }

void
TR::RelocationRecordVerifyClassObjectForAlloc::print(TR::RelocationRuntime *reloRuntime)
   {
   TR::RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR::RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR::RelocationRecordConstantPoolWithIndex::print(reloRuntime);
   reloLogger->printf("\tallocationSize %p\n", allocationSize(reloTarget));
   }

int32_t
TR::RelocationRecordVerifyClassObjectForAlloc::bytesInHeaderAndPayload()
   {
   return sizeof(TR::RelocationRecordVerifyClassObjectForAllocBinaryTemplate);
   }

void
TR::RelocationRecordVerifyClassObjectForAlloc::setAllocationSize(TR::RelocationTarget *reloTarget, uintptrj_t allocationSize)
   {
   reloTarget->storeRelocationRecordValue(allocationSize, (uintptrj_t *)&((TR::RelocationRecordVerifyClassObjectForAllocBinaryTemplate *)_record)->_allocationSize);
   }

uintptrj_t
TR::RelocationRecordVerifyClassObjectForAlloc::allocationSize(TR::RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *)&((TR::RelocationRecordVerifyClassObjectForAllocBinaryTemplate *)_record)->_allocationSize);
   }

bool
TR::RelocationRecordVerifyClassObjectForAlloc::verifyClass(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, TR::OpaqueClassBlock *clazz)
   {
   bool inlineAllocation = false;
   TR::Compilation* comp = TR::comp();
   TR::J9VMBase *fe = (TR::J9VMBase *)(reloRuntime->fej9());
   if (comp->canAllocateInlineClass(clazz))
      {
      uintptrj_t size = fe->getAllocationSize(NULL, clazz);
      RELO_LOG(reloRuntime->reloLogger(), 6, "\tverifyClass: allocationSize %d\n", size);
      if (size == allocationSize(reloTarget))
         inlineAllocation = true;
      }
   else
      RELO_LOG(reloRuntime->reloLogger(), 6, "\tverifyClass: cannot inline allocate class\n");

   return inlineAllocation;
   }

// TR::InlinedMethod Relocation
void
TR::RelocationRecordInlinedMethod::print(TR::RelocationRuntime *reloRuntime)
   {
   TR::RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR::RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR::RelocationRecordConstantPoolWithIndex::print(reloRuntime);
   J9ROMClass *inlinedCodeRomClass = (J9ROMClass *)reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache((void *) romClassOffsetInSharedCache(reloTarget));
   J9UTF8 *inlinedCodeClassName = J9ROMCLASS_CLASSNAME(inlinedCodeRomClass);
   reloLogger->printf("\tromClassOffsetInSharedCache %x %.*s\n", romClassOffsetInSharedCache(reloTarget), inlinedCodeClassName->length, inlinedCodeClassName->data );
   //reloLogger->printf("\tromClassOffsetInSharedCache %x %.*s\n", romClassOffsetInSharedCache(reloTarget), J9UTF8_LENGTH(inlinedCodeClassname), J9UTF8_DATA(inlinedCodeClassName));
   }

void
TR::RelocationRecordInlinedMethod::setRomClassOffsetInSharedCache(TR::RelocationTarget *reloTarget, uintptrj_t romClassOffsetInSharedCache)
   {
   reloTarget->storeRelocationRecordValue(romClassOffsetInSharedCache, (uintptrj_t *) &((TR::RelocationRecordInlinedMethodBinaryTemplate *)_record)->_romClassOffsetInSharedCache);
   }

uintptrj_t
TR::RelocationRecordInlinedMethod::romClassOffsetInSharedCache(TR::RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR::RelocationRecordInlinedMethodBinaryTemplate *)_record)->_romClassOffsetInSharedCache);
   }

int32_t
TR::RelocationRecordInlinedMethod::bytesInHeaderAndPayload()
   {
   return sizeof(TR::RelocationRecordInlinedMethodBinaryTemplate);
   }

void
TR::RelocationRecordInlinedMethod::fixInlinedSiteInfo(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, TR::OpaqueMethodBlock *inlinedMethod)
   {
   TR::InlinedCallSite *inlinedCallSite = (TR::InlinedCallSite *)getInlinedCallSiteArrayElement(reloRuntime->exceptionTable(), inlinedSiteIndex(reloTarget));
   inlinedCallSite->_methodInfo = inlinedMethod;
   TR::RelocationRecordInlinedMethodPrivateData *reloPrivateData = &(privateData()->inlinedMethod);
   RELO_LOG(reloRuntime->reloLogger(), 5, "\tfixInlinedSiteInfo: [%p] set to %p, virtual guard address %p\n",inlinedCallSite, inlinedMethod, reloPrivateData->_destination);

   /*
    * An inlined site's _methodInfo field is used to resolve inlined methods' J9Classes and mark their respective heap objects as live during gc stackwalking.
    * If we unload a class from which we have inlined one or more methods, we need to invalidate the _methodInfo fields themselves in addition to any other uses,
    * to prevent us from attempting to mark unloaded classes as live.
    *
    * See also: frontend/j9/MetaData.cpp:populateInlineCalls(TR::Compilation* , TR::J9VMBase* , TR::MethodMetaData* , uint8_t* , uint32_t )
    */
   TR::OpaqueClassBlock *classOfInlinedMethod = reloRuntime->fej9()->getClassFromMethodBlock(inlinedMethod);
   if ( reloRuntime->fej9()->isUnloadAssumptionRequired( classOfInlinedMethod, reloRuntime->comp()->getCurrentMethod() ) )
      {
      reloTarget->addPICtoPatchPtrOnClassUnload(classOfInlinedMethod, &(inlinedCallSite->_methodInfo));
      }
   }

void
TR::RelocationRecordInlinedMethod::preparePrivateData(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget)
   {
   TR::OpaqueMethodBlock *ramMethod = NULL;
   bool inlinedSiteIsValid = inlinedSiteValid(reloRuntime, reloTarget, &ramMethod);

   if (reloRuntime->comp()->getOption(TR::UseSymbolValidationManager) && !ramMethod)
      {
      TR::ASSERT(false, "inlinedSiteValid should not return a NULL method when using the SVM!\n");
      reloRuntime->comp()->failCompilation<J9::AOTSymbolValidationManagerFailure>("inlinedSiteValid returned NULL method");
      }

   if (ramMethod)
      {
      // If validate passes, no patching needed since the fall-through path is the inlined code
      fixInlinedSiteInfo(reloRuntime, reloTarget,ramMethod);
      }

   TR::RelocationRecordInlinedMethodPrivateData *reloPrivateData = &(privateData()->inlinedMethod);
   reloPrivateData->_ramMethod = ramMethod;
   reloPrivateData->_failValidation = !inlinedSiteIsValid;
   RELO_LOG(reloRuntime->reloLogger(), 5, "\tpreparePrivateData: ramMethod %p inlinedSiteIsValid %d\n", ramMethod, inlinedSiteIsValid);
   }


bool
TR::RelocationRecordInlinedMethod::inlinedSiteValid(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, TR::OpaqueMethodBlock **theMethod)
   {
   J9Method *currentMethod = NULL;
   bool inlinedSiteIsValid = true;
   J9Method *callerMethod = (J9Method *) getInlinedSiteCallerMethod(reloRuntime);
   if (callerMethod == (J9Method *)-1)
      {
      RELO_LOG(reloRuntime->reloLogger(), 6, "\tinlinedSiteValid: caller failed relocation so cannot validate inlined method\n");
      *theMethod = NULL;
      return false;
      }
   RELO_LOG(reloRuntime->reloLogger(), 6, "\tvalidateSameClasses: caller method %p\n", callerMethod);
   J9UTF8 *callerClassName;
   J9UTF8 *callerMethodName;
   J9UTF8 *callerMethodSignature;
   getClassNameSignatureFromMethod(callerMethod, callerClassName, callerMethodName, callerMethodSignature);
   RELO_LOG(reloRuntime->reloLogger(), 6, "\tinlinedSiteValid: caller method %.*s.%.*s%.*s\n",
                                              callerClassName->length, callerClassName->data,
                                              callerMethodName->length, callerMethodName->data,
                                              callerMethodSignature->length, callerMethodSignature->data);

   J9ConstantPool *cp = NULL;
   if (!isUnloadedInlinedMethod(callerMethod))
      cp = J9_CP_FROM_METHOD(callerMethod);
   RELO_LOG(reloRuntime->reloLogger(), 6, "\tinlinedSiteValid: cp %p\n", cp);

   if (!cp)
      {
      inlinedSiteIsValid = false;
      }
   else
      {
      TR::RelocationRecordInlinedMethodPrivateData *reloPrivateData = &(privateData()->inlinedMethod);

      if (reloRuntime->comp()->getOption(TR::UseSymbolValidationManager))
         {
         TR::RelocationRecordInlinedMethodPrivateData *reloPrivateData = &(privateData()->inlinedMethod);

         uintptrj_t data = (uintptrj_t)cpIndex(reloTarget);
         uint16_t methodID = (uint16_t)(data & 0xFFFF);
         uint16_t receiverClassID = (uint16_t)((data >> 16) & 0xFFFF);

         // currentMethod is guaranteed to not be NULL because of the SVM
         currentMethod = (J9Method *)reloRuntime->comp()->getSymbolValidationManager()->getSymbolFromID(methodID);
         reloPrivateData->_receiverClass = (TR::OpaqueClassBlock *)reloRuntime->comp()->getSymbolValidationManager()->getSymbolFromID(receiverClassID);

         if (reloFlags(reloTarget) != inlinedMethodIsStatic && reloFlags(reloTarget) != inlinedMethodIsSpecial)
            {
            TR::ResolvedMethod *calleeResolvedMethod = reloRuntime->fej9()->createResolvedMethod(reloRuntime->comp()->trMemory(),
                                                                                                (TR::OpaqueMethodBlock *)currentMethod,
                                                                                                NULL);
            if (calleeResolvedMethod->virtualMethodIsOverridden())
               inlinedSiteIsValid = false;
            }
         }
      else
         {
         currentMethod = (J9Method *) getMethodFromCP(reloRuntime, cp, cpIndex(reloTarget), (TR::OpaqueMethodBlock *) callerMethod);
         if (!currentMethod)
            inlinedSiteIsValid = false;
         }

      if (inlinedSiteIsValid &&
          (reloRuntime->fej9()->isAnyMethodTracingEnabled((TR::OpaqueMethodBlock *) currentMethod) ||
           reloRuntime->fej9()->canMethodEnterEventBeHooked() ||
           reloRuntime->fej9()->canMethodExitEventBeHooked()))
         {
         RELO_LOG(reloRuntime->reloLogger(), 6, "\tinlinedSiteValid: target may need enter/exit tracing so disabling inline site\n");
         inlinedSiteIsValid = false;
         }

      if (inlinedSiteIsValid)
         {
         /* Calculate the runtime rom class value from the code cache */
         void *compileRomClass = reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache((void *) romClassOffsetInSharedCache(reloTarget));
         void *currentRomClass = (void *)J9_CLASS_FROM_METHOD(currentMethod)->romClass;

         RELO_LOG(reloRuntime->reloLogger(), 6, "\tinlinedSiteValid: compileRomClass %p currentRomClass %p\n", compileRomClass, currentRomClass);
         if (compileRomClass == currentRomClass)
            {
            J9UTF8 *className;
            J9UTF8 *methodName;
            J9UTF8 *methodSignature;
            getClassNameSignatureFromMethod(currentMethod, className, methodName, methodSignature);
            RELO_LOG(reloRuntime->reloLogger(), 6, "\tinlinedSiteValid: inlined method %.*s.%.*s%.*s\n",
                                                       className->length, className->data,
                                                       methodName->length, methodName->data,
                                                       methodSignature->length, methodSignature->data);
            }
         else
            {
            inlinedSiteIsValid = false;
            if (reloRuntime->comp()->getOption(TR::UseSymbolValidationManager))
               {
               TR::ASSERT(false, "compileRomClass and currentRomClass should not be different!\n");
               reloRuntime->comp()->failCompilation<J9::AOTSymbolValidationManagerFailure>("compileRomClass and currentRomClass are different");
               }
            }
         }
      }

   /* Even if the inlined site is disabled, the inlined site table in the metadata
    * should still be populated
    */
   TR::SimpleRegex * regex = reloRuntime->options()->getDisabledInlineSites();
   if (regex && TR::SimpleRegex::match(regex, inlinedSiteIndex(reloTarget)))
      {
      RELO_LOG(reloRuntime->reloLogger(), 6, "\tinlinedSiteValid: inlined site forcibly disabled by options\n");
      inlinedSiteIsValid = false;
      }

   if (!inlinedSiteIsValid)
      RELO_LOG(reloRuntime->reloLogger(), 6, "\tinlinedSiteValid: not valid\n");

   *theMethod = reinterpret_cast<TR::OpaqueMethodBlock *>(currentMethod);
   return inlinedSiteIsValid;
   }

int32_t
TR::RelocationRecordInlinedMethod::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   reloRuntime->incNumInlinedMethodRelos();

   TR::AOTStats *aotStats = reloRuntime->aotStats();

   TR::RelocationRecordInlinedMethodPrivateData *reloPrivateData = &(privateData()->inlinedMethod);
   if (reloPrivateData->_failValidation)
      {
      RELO_LOG(reloRuntime->reloLogger(), 6,"\t\tapplyRelocation: invalidating guard\n");

      invalidateGuard(reloRuntime, reloTarget, reloLocation);

      reloRuntime->incNumFailedInlinedMethodRelos();
      if (aotStats)
         {
         aotStats->numInlinedMethodValidationFailed++;
         updateFailedStats(aotStats);
         }
      }
   else
      {
      RELO_LOG(reloRuntime->reloLogger(), 6,"\t\tapplyRelocation: activating inlined method\n");

      activateGuard(reloRuntime, reloTarget, reloLocation);

      if (aotStats)
         {
         aotStats->numInlinedMethodRelocated++;
         updateSucceededStats(aotStats);
         }
      }

   return 0;
   }

// TR::RelocationRecordNopGuard
void
TR::RelocationRecordNopGuard::print(TR::RelocationRuntime *reloRuntime)
   {
   TR::RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR::RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR::RelocationRecordInlinedMethod::print(reloRuntime);
   reloLogger->printf("\tdestinationAddress %p\n", destinationAddress(reloTarget));
   }

void
TR::RelocationRecordNopGuard::setDestinationAddress(TR::RelocationTarget *reloTarget, uintptrj_t destinationAddress)
   {
   reloTarget->storeRelocationRecordValue(destinationAddress, (uintptrj_t *) &((TR::RelocationRecordNopGuardBinaryTemplate *)_record)->_destinationAddress);
   }

uintptrj_t
TR::RelocationRecordNopGuard::destinationAddress(TR::RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR::RelocationRecordNopGuardBinaryTemplate *)_record)->_destinationAddress);
   }

void
TR::RelocationRecordNopGuard::invalidateGuard(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR::RelocationRecordInlinedMethodPrivateData *reloPrivateData = &(privateData()->inlinedMethod);
   _patchVirtualGuard(reloLocation, reloPrivateData->_destination, 1);
   }

void
TR::RelocationRecordNopGuard::activateGuard(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   createAssumptions(reloRuntime, reloLocation);
   if (reloRuntime->options()->getOption(TR::EnableHCR))
      {
      TR::RelocationRecordInlinedMethodPrivateData *reloPrivateData = &(privateData()->inlinedMethod);
      TR::PatchNOPedGuardSiteOnClassRedefinition::make(reloRuntime->fej9(), reloRuntime->trMemory()->trPersistentMemory(),
         (TR::OpaqueClassBlock*) J9_CLASS_FROM_METHOD((J9Method *)reloPrivateData->_ramMethod),
         reloLocation, reloPrivateData->_destination,
         getMetadataAssumptionList(reloRuntime->exceptionTable()));
      }
   }

int32_t
TR::RelocationRecordNopGuard::bytesInHeaderAndPayload()
   {
   return sizeof(TR::RelocationRecordNopGuardBinaryTemplate);
   }

void
TR::RelocationRecordNopGuard::preparePrivateData(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget)
   {
   TR::RelocationRecordInlinedMethod::preparePrivateData(reloRuntime, reloTarget);

   TR::RelocationRecordInlinedMethodPrivateData *reloPrivateData = &(privateData()->inlinedMethod);
   reloPrivateData->_destination = (uint8_t *) (destinationAddress(reloTarget) - reloRuntime->aotMethodHeaderEntry()->compileMethodCodeStartPC + (UDATA)reloRuntime->newMethodCodeStart());
   RELO_LOG(reloRuntime->reloLogger(), 6, "\tpreparePrivateData: guard backup destination %p\n", reloPrivateData->_destination);
   }

// TR::InlinedStaticMethodWithNopGuard
char *
TR::RelocationRecordInlinedStaticMethodWithNopGuard::name()
   {
   return "TR::InlinedStaticMethodWithNopGuard";
   }

TR::OpaqueMethodBlock *
TR::RelocationRecordInlinedStaticMethodWithNopGuard::getMethodFromCP(TR::RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex, TR::OpaqueMethodBlock *callerMethod)
   {
   return getStaticMethodFromCP(reloRuntime, void_cp, cpIndex);
   }

void
TR::RelocationRecordInlinedStaticMethodWithNopGuard::updateFailedStats(TR::AOTStats *aotStats)
   {
   aotStats->staticMethods.numFailedValidations++;
   }

void
TR::RelocationRecordInlinedStaticMethodWithNopGuard::updateSucceededStats(TR::AOTStats *aotStats)
   {
   aotStats->staticMethods.numSucceededValidations++;
   }

// TR::InlinedSpecialMethodWithNopGuard
char *
TR::RelocationRecordInlinedSpecialMethodWithNopGuard::name()
   {
   return "TR::InlinedSpecialMethodWithNopGuard";
   }

TR::OpaqueMethodBlock *
TR::RelocationRecordInlinedSpecialMethodWithNopGuard::getMethodFromCP(TR::RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex, TR::OpaqueMethodBlock *callerMethod)
   {
   return getSpecialMethodFromCP(reloRuntime, void_cp, cpIndex);
   }

void
TR::RelocationRecordInlinedSpecialMethodWithNopGuard::updateFailedStats(TR::AOTStats *aotStats)
   {
   aotStats->specialMethods.numFailedValidations++;
   }

void
TR::RelocationRecordInlinedSpecialMethodWithNopGuard::updateSucceededStats(TR::AOTStats *aotStats)
   {
   aotStats->specialMethods.numSucceededValidations++;
   }

// TR::InlinedVirtualMethodWithNopGuard
char *
TR::RelocationRecordInlinedVirtualMethodWithNopGuard::name()
   {
   return "TR::InlinedVirtualMethodWithNopGuard";
   }

TR::OpaqueMethodBlock *
TR::RelocationRecordInlinedVirtualMethodWithNopGuard::getMethodFromCP(TR::RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex, TR::OpaqueMethodBlock *callerMethod)
   {
   return getVirtualMethodFromCP(reloRuntime, void_cp, cpIndex);
   }

void
TR::RelocationRecordInlinedVirtualMethodWithNopGuard::createAssumptions(TR::RelocationRuntime *reloRuntime, uint8_t *reloLocation)
   {
   TR::RelocationRecordInlinedMethodPrivateData *reloPrivateData = &(privateData()->inlinedMethod);
   TR::PatchNOPedGuardSiteOnMethodOverride::make(reloRuntime->fej9(), reloRuntime->trMemory()->trPersistentMemory(),
      (TR::OpaqueMethodBlock*) reloPrivateData->_ramMethod, reloLocation, reloPrivateData->_destination,
      getMetadataAssumptionList(reloRuntime->exceptionTable()));
   }

void
TR::RelocationRecordInlinedVirtualMethodWithNopGuard::updateFailedStats(TR::AOTStats *aotStats)
   {
   aotStats->virtualMethods.numFailedValidations++;
   }

void
TR::RelocationRecordInlinedVirtualMethodWithNopGuard::updateSucceededStats(TR::AOTStats *aotStats)
   {
   aotStats->virtualMethods.numSucceededValidations++;
   }

// TR::RelocationRecordInlinedVirtualMethod
char *
TR::RelocationRecordInlinedVirtualMethod::name()
   {
   return "TR::InlinedVirtualMethod";
   }

void
TR::RelocationRecordInlinedVirtualMethod::print(TR::RelocationRuntime *reloRuntime)
   {
   Base::print(reloRuntime);
   }

void
TR::RelocationRecordInlinedVirtualMethod::preparePrivateData(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget)
   {
   Base::preparePrivateData(reloRuntime, reloTarget);
   }

TR::OpaqueMethodBlock *
TR::RelocationRecordInlinedVirtualMethod::getMethodFromCP(TR::RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex)
   {
   return getVirtualMethodFromCP(reloRuntime, void_cp, cpIndex);
   }

// TR::InlinedInterfaceMethodWithNopGuard
char *
TR::RelocationRecordInlinedInterfaceMethodWithNopGuard::name()
   {
   return "TR::InlinedInterfaceMethodWithNopGuard";
   }

void
TR::RelocationRecordInlinedInterfaceMethodWithNopGuard::createAssumptions(TR::RelocationRuntime *reloRuntime, uint8_t *reloLocation)
   {
   TR::RelocationRecordInlinedMethodPrivateData *reloPrivateData = &(privateData()->inlinedMethod);
   TR::PersistentCHTable *table = reloRuntime->getPersistentInfo()->getPersistentCHTable();
   List<TR::VirtualGuardSite> sites(reloRuntime->comp()->trMemory());
   TR::VirtualGuardSite site;
   site.setLocation(reloLocation);
   site.setDestination(reloPrivateData->_destination);
   sites.add(&site);
   TR::ClassQueries::addAnAssumptionForEachSubClass(table, table->findClassInfo(reloPrivateData->_receiverClass), sites, reloRuntime->comp());
   }

void
TR::RelocationRecordInlinedInterfaceMethodWithNopGuard::updateFailedStats(TR::AOTStats *aotStats)
   {
   aotStats->interfaceMethods.numFailedValidations++;
   TR::RelocationRecordInlinedMethodPrivateData *reloPrivateData = &(privateData()->inlinedMethod);
   TR::OpaqueClassBlock *receiver = reloPrivateData->_receiverClass;
   }

void
TR::RelocationRecordInlinedInterfaceMethodWithNopGuard::updateSucceededStats(TR::AOTStats *aotStats)
   {
   aotStats->interfaceMethods.numSucceededValidations++;
   }

TR::OpaqueMethodBlock *
TR::RelocationRecordInlinedInterfaceMethodWithNopGuard::getMethodFromCP(TR::RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex, TR::OpaqueMethodBlock *callerMethod)
   {
   return getInterfaceMethodFromCP(reloRuntime, void_cp, cpIndex, callerMethod);
   }

// TR::RelocationRecordInlinedInterfaceMethod
char *
TR::RelocationRecordInlinedInterfaceMethod::name()
   {
   return "TR::InlinedInterfaceMethod";
   }

void
TR::RelocationRecordInlinedInterfaceMethod::print(TR::RelocationRuntime *reloRuntime)
   {
   TR::RelocationRecordInlinedMethod::print(reloRuntime);
   }

void
TR::RelocationRecordInlinedInterfaceMethod::preparePrivateData(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget)
   {
   TR::RelocationRecordInlinedMethod::preparePrivateData(reloRuntime, reloTarget);
   }

TR::OpaqueMethodBlock *
TR::RelocationRecordInlinedInterfaceMethod::getMethodFromCP(TR::RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex, TR::OpaqueMethodBlock *callerMethod)
   {
   return getInterfaceMethodFromCP(reloRuntime, void_cp, cpIndex, callerMethod);
   }

// TR::InlinedAbstractMethodWithNopGuard
char *
TR::RelocationRecordInlinedAbstractMethodWithNopGuard::name()
   {
   return "TR::InlinedAbstractMethodWithNopGuard";
   }

TR::OpaqueMethodBlock *
TR::RelocationRecordInlinedAbstractMethodWithNopGuard::getMethodFromCP(TR::RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex, TR::OpaqueMethodBlock *callerMethod)
   {
   return getAbstractMethodFromCP(reloRuntime, void_cp, cpIndex, callerMethod);
   }

void
TR::RelocationRecordInlinedAbstractMethodWithNopGuard::createAssumptions(TR::RelocationRuntime *reloRuntime, uint8_t *reloLocation)
   {
   TR::RelocationRecordInlinedMethodPrivateData *reloPrivateData = &(privateData()->inlinedMethod);
   TR::PersistentCHTable *table = reloRuntime->getPersistentInfo()->getPersistentCHTable();
   List<TR::VirtualGuardSite> sites(reloRuntime->comp()->trMemory());
   TR::VirtualGuardSite site;
   site.setLocation(reloLocation);
   site.setDestination(reloPrivateData->_destination);
   sites.add(&site);
   TR::ClassQueries::addAnAssumptionForEachSubClass(table, table->findClassInfo(reloPrivateData->_receiverClass), sites, reloRuntime->comp());
   }

void
TR::RelocationRecordInlinedAbstractMethodWithNopGuard::updateFailedStats(TR::AOTStats *aotStats)
   {
   aotStats->abstractMethods.numFailedValidations++;
   }

void
TR::RelocationRecordInlinedAbstractMethodWithNopGuard::updateSucceededStats(TR::AOTStats *aotStats)
   {
   aotStats->abstractMethods.numSucceededValidations++;
   }

// TR::ProfiledInlinedMethod
char *
TR::RelocationRecordProfiledInlinedMethod::name()
   {
   return "TR::ProfiledInlinedMethod";
   }

void
TR::RelocationRecordProfiledInlinedMethod::print(TR::RelocationRuntime *reloRuntime)
   {
   TR::RelocationRecordInlinedMethod::print(reloRuntime);

   TR::RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR::RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   reloLogger->printf("\tclassChainIdentifyingLoaderOffsetInSharedCache %x\n", classChainIdentifyingLoaderOffsetInSharedCache(reloTarget));
   reloLogger->printf("\tclassChainForInlinedMethod %x\n", classChainForInlinedMethod(reloTarget));
   reloLogger->printf("\tvTableSlot %x\n", vTableSlot(reloTarget));
   }

int32_t
TR::RelocationRecordProfiledInlinedMethod::bytesInHeaderAndPayload()
   {
   return sizeof(TR::RelocationRecordProfiledInlinedMethodBinaryTemplate);
   }

void
TR::RelocationRecordProfiledInlinedMethod::setClassChainIdentifyingLoaderOffsetInSharedCache(TR::RelocationTarget *reloTarget, uintptrj_t classChainIdentifyingLoaderOffsetInSharedCache)
   {
   reloTarget->storeRelocationRecordValue(classChainIdentifyingLoaderOffsetInSharedCache, (uintptrj_t *) &((TR::RelocationRecordProfiledInlinedMethodBinaryTemplate *)_record)->_classChainIdentifyingLoaderOffsetInSharedCache);
   }

uintptrj_t
TR::RelocationRecordProfiledInlinedMethod::classChainIdentifyingLoaderOffsetInSharedCache(TR::RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR::RelocationRecordProfiledInlinedMethodBinaryTemplate *)_record)->_classChainIdentifyingLoaderOffsetInSharedCache);
   }

void
TR::RelocationRecordProfiledInlinedMethod::setClassChainForInlinedMethod(TR::RelocationTarget *reloTarget, uintptrj_t classChainForInlinedMethod)
   {
   reloTarget->storeRelocationRecordValue(classChainForInlinedMethod, (uintptrj_t *) &((TR::RelocationRecordProfiledInlinedMethodBinaryTemplate *)_record)->_classChainForInlinedMethod);
   }

uintptrj_t
TR::RelocationRecordProfiledInlinedMethod::classChainForInlinedMethod(TR::RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR::RelocationRecordProfiledInlinedMethodBinaryTemplate *)_record)->_classChainForInlinedMethod);
   }

void
TR::RelocationRecordProfiledInlinedMethod::setVTableSlot(TR::RelocationTarget *reloTarget, uintptrj_t vTableSlot)
   {
   reloTarget->storeRelocationRecordValue(vTableSlot, (uintptrj_t *) &((TR::RelocationRecordProfiledInlinedMethodBinaryTemplate *)_record)->_vTableSlot);
   }

uintptrj_t
TR::RelocationRecordProfiledInlinedMethod::vTableSlot(TR::RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR::RelocationRecordProfiledInlinedMethodBinaryTemplate *)_record)->_vTableSlot);
   }


bool
TR::RelocationRecordProfiledInlinedMethod::checkInlinedClassValidity(TR::RelocationRuntime *reloRuntime, TR::OpaqueClassBlock *inlinedClass)
   {
   return true;
   }

void
TR::RelocationRecordProfiledInlinedMethod::setupInlinedMethodData(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget)
   {
   TR::RelocationRecordProfiledInlinedMethodPrivateData *reloPrivateData = &(privateData()->profiledInlinedMethod);
   reloPrivateData->_guardValue = 0;
   }

void
TR::RelocationRecordProfiledInlinedMethod::preparePrivateData(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget)
   {
   TR::RelocationRecordProfiledInlinedMethodPrivateData *reloPrivateData = &(privateData()->profiledInlinedMethod);
   reloPrivateData->_needUnloadAssumption = false;
   reloPrivateData->_guardValue = 0;
   bool failValidation = true;

   J9ROMClass *inlinedCodeRomClass = (J9ROMClass *) reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache((void *) romClassOffsetInSharedCache(reloTarget));
   J9UTF8 *inlinedCodeClassName = J9ROMCLASS_CLASSNAME(inlinedCodeRomClass);
   RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: inlinedCodeRomClass %p %.*s\n", inlinedCodeRomClass, inlinedCodeClassName->length, inlinedCodeClassName->data);

#if defined(PUBLIC_BUILD)
   J9ClassLoader *classLoader = NULL;
#else
   void *classChainIdentifyingLoader = reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache((void *) classChainIdentifyingLoaderOffsetInSharedCache(reloTarget));
   RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: classChainIdentifyingLoader %p\n", classChainIdentifyingLoader);
   J9ClassLoader *classLoader = (J9ClassLoader *) reloRuntime->fej9()->sharedCache()->persistentClassLoaderTable()->lookupClassLoaderAssociatedWithClassChain(classChainIdentifyingLoader);
#endif
   RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: classLoader %p\n", classLoader);

   if (classLoader)
      {
      if (0 && *((uint32_t *)classLoader->classLoaderObject) != 0x99669966)
         {
         RELO_LOG(reloRuntime->reloLogger(), 1,"\tpreparePrivateData: SUSPICIOUS class loader found\n");
         RELO_LOG(reloRuntime->reloLogger(), 1,"\tpreparePrivateData: inlinedCodeRomClass %p %.*s\n", inlinedCodeRomClass, inlinedCodeClassName->length, inlinedCodeClassName->data);
         RELO_LOG(reloRuntime->reloLogger(), 1,"\tpreparePrivateData: classChainIdentifyingLoader %p\n", classChainIdentifyingLoader);
         RELO_LOG(reloRuntime->reloLogger(), 1,"\tpreparePrivateData: classLoader %p\n", classLoader);
         }
      J9JavaVM *javaVM = reloRuntime->jitConfig()->javaVM;
      J9VMThread *vmThread = reloRuntime->currentThread();

      TR::OpaqueClassBlock *inlinedCodeClass;

         {
         TR::VMAccessCriticalSection preparePrivateDataCriticalSection(reloRuntime->fej9());
         inlinedCodeClass = (TR::OpaqueClassBlock *) jitGetClassInClassloaderFromUTF8(vmThread,
                                                                                     classLoader,
                                                                                     J9UTF8_DATA(inlinedCodeClassName),
                                                                                     J9UTF8_LENGTH(inlinedCodeClassName));
         }

      if (inlinedCodeClass && checkInlinedClassValidity(reloRuntime, inlinedCodeClass))
         {
         RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: inlined class valid\n");
         reloPrivateData->_inlinedCodeClass = inlinedCodeClass;
         uintptrj_t *chainData = (uintptrj_t *) reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache((void *) classChainForInlinedMethod(reloTarget));
         if (reloRuntime->fej9()->sharedCache()->classMatchesCachedVersion(inlinedCodeClass, chainData))
            {
            RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: classes match\n");
            TR::OpaqueMethodBlock *inlinedMethod = * (TR::OpaqueMethodBlock **) (((uint8_t *)reloPrivateData->_inlinedCodeClass) + vTableSlot(reloTarget));

            if (reloRuntime->fej9()->isAnyMethodTracingEnabled(inlinedMethod))
               {
               RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: target may need enter/exit tracing so disable inline site\n");
               }
            else
               {
               fixInlinedSiteInfo(reloRuntime, reloTarget, inlinedMethod);

               reloPrivateData->_needUnloadAssumption = !reloRuntime->fej9()->sameClassLoaders(inlinedCodeClass, reloRuntime->comp()->getCurrentMethod()->classOfMethod());
               setupInlinedMethodData(reloRuntime, reloTarget);
               failValidation = false;
               }
            }
         }
      }

   reloPrivateData->_failValidation = failValidation;
   RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: needUnloadAssumption %d\n", reloPrivateData->_needUnloadAssumption);
   RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: guardValue %p\n", reloPrivateData->_guardValue);
   RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: failValidation %d\n", failValidation);
   }

void
TR::RelocationRecordProfiledInlinedMethod::updateSucceededStats(TR::AOTStats *aotStats)
   {
   aotStats->profiledInlinedMethods.numSucceededValidations++;
   }

void
TR::RelocationRecordProfiledInlinedMethod::updateFailedStats(TR::AOTStats *aotStats)
   {
   aotStats->profiledInlinedMethods.numFailedValidations++;
   }


// TR::ProfiledGuard
void
TR::RelocationRecordProfiledGuard::activateGuard(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   }

void
TR::RelocationRecordProfiledGuard::invalidateGuard(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   }

// TR::ProfileClassGuard
char *
TR::RelocationRecordProfiledClassGuard::name()
   {
   return "TR::ProfiledClassGuard";
   }

bool
TR::RelocationRecordProfiledClassGuard::checkInlinedClassValidity(TR::RelocationRuntime *reloRuntime, TR::OpaqueClassBlock *inlinedCodeClass)
   {
   return true;
   return !reloRuntime->fej9()->classHasBeenExtended(inlinedCodeClass) && !reloRuntime->options()->getOption(TR::DisableProfiledInlining);
   }

void
TR::RelocationRecordProfiledClassGuard::setupInlinedMethodData(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget)
   {
   TR::RelocationRecordProfiledInlinedMethodPrivateData *reloPrivateData = &(privateData()->profiledInlinedMethod);
   reloPrivateData->_guardValue = (uintptrj_t) reloPrivateData->_inlinedCodeClass;
   }

void
TR::RelocationRecordProfiledClassGuard::updateSucceededStats(TR::AOTStats *aotStats)
   {
   aotStats->profiledClassGuards.numSucceededValidations++;
   }

void
TR::RelocationRecordProfiledClassGuard::updateFailedStats(TR::AOTStats *aotStats)
   {
   aotStats->profiledClassGuards.numFailedValidations++;
   }


// TR::ProfiledMethodGuard
char *
TR::RelocationRecordProfiledMethodGuard::name()
   {
   return "TR::ProfiledMethodGuard";
   }

bool
TR::RelocationRecordProfiledMethodGuard::checkInlinedClassValidity(TR::RelocationRuntime *reloRuntime, TR::OpaqueClassBlock *inlinedCodeClass)
   {
   return true;
   return !reloRuntime->options()->getOption(TR::DisableProfiledMethodInlining);
   }

void
TR::RelocationRecordProfiledMethodGuard::setupInlinedMethodData(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget)
   {
   TR::RelocationRecordProfiledInlinedMethodPrivateData *reloPrivateData = &(privateData()->profiledInlinedMethod);
   TR::OpaqueMethodBlock *inlinedMethod = * (TR::OpaqueMethodBlock **) (((uint8_t *)reloPrivateData->_inlinedCodeClass) + vTableSlot(reloTarget));
   reloPrivateData->_guardValue = (uintptrj_t) inlinedMethod;
   }

void
TR::RelocationRecordProfiledMethodGuard::updateSucceededStats(TR::AOTStats *aotStats)
   {
   aotStats->profiledMethodGuards.numSucceededValidations++;
   }

void
TR::RelocationRecordProfiledMethodGuard::updateFailedStats(TR::AOTStats *aotStats)
   {
   aotStats->profiledMethodGuards.numFailedValidations++;
   }


// TR::RamMethod Relocation
char *
TR::RelocationRecordRamMethod::name()
   {
   return "TR::RamMethod";
   }

int32_t
TR::RelocationRecordRamMethod::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tapplyRelocation: method pointer %p\n", reloRuntime->exceptionTable()->ramMethod);
   reloTarget->storeAddress((uint8_t *)reloRuntime->exceptionTable()->ramMethod, reloLocation);
   return 0;
   }

int32_t
TR::RelocationRecordRamMethod::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tapplyRelocation: method pointer %p\n", reloRuntime->exceptionTable()->ramMethod);
   reloTarget->storeAddress((uint8_t *)reloRuntime->exceptionTable()->ramMethod, reloLocationHigh, reloLocationLow, reloFlags(reloTarget));
   return 0;
   }

// TR::MethodTracingCheck Relocation
void
TR::RelocationRecordMethodTracingCheck::print(TR::RelocationRuntime *reloRuntime)
   {
   TR::RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR::RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR::RelocationRecord::print(reloRuntime);
   reloLogger->printf("\tdestinationAddress %x\n", destinationAddress(reloTarget));
   }

int32_t
TR::RelocationRecordMethodTracingCheck::bytesInHeaderAndPayload()
   {
   return sizeof(TR::RelocationRecordMethodTracingCheckBinaryTemplate);
   }

void
TR::RelocationRecordMethodTracingCheck::setDestinationAddress(TR::RelocationTarget *reloTarget, uintptrj_t destinationAddress)
   {
   reloTarget->storeRelocationRecordValue(destinationAddress, (uintptrj_t *) &((TR::RelocationRecordMethodTracingCheckBinaryTemplate *)_record)->_destinationAddress);
   }

uintptrj_t
TR::RelocationRecordMethodTracingCheck::destinationAddress(TR::RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR::RelocationRecordMethodTracingCheckBinaryTemplate *)_record)->_destinationAddress);
   }

void
TR::RelocationRecordMethodTracingCheck::preparePrivateData(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget)
   {
   TR::RelocationRecordMethodTracingCheckPrivateData *reloPrivateData = &(privateData()->methodTracingCheck);

   uintptrj_t destination = destinationAddress(reloTarget);
   reloPrivateData->_destinationAddress = (uint8_t *) (destinationAddress(reloTarget) - (UDATA) reloRuntime->aotMethodHeaderEntry()->compileMethodCodeStartPC + (UDATA)(reloRuntime->newMethodCodeStart()));
   RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: check destination %p\n", reloPrivateData->_destinationAddress);
   }

int32_t
TR::RelocationRecordMethodTracingCheck::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR::RelocationRecordMethodTracingCheckPrivateData *reloPrivateData = &(privateData()->methodTracingCheck);
   _patchVirtualGuard(reloLocation, reloPrivateData->_destinationAddress, 1 /* currently assume SMP only */);
   return 0;
   }


// TR::MethodEnterCheck
char *
TR::RelocationRecordMethodEnterCheck::name()
   {
   return "TR::MethodEnterCheck";
   }

bool
TR::RelocationRecordMethodEnterCheck::ignore(TR::RelocationRuntime *reloRuntime)
   {
   bool reportMethodEnter = reloRuntime->fej9()->isMethodEnterTracingEnabled((TR::OpaqueMethodBlock *) reloRuntime->method()) || reloRuntime->fej9()->canMethodEnterEventBeHooked();
   RELO_LOG(reloRuntime->reloLogger(), 6,"\tignore: reportMethodEnter %d\n", reportMethodEnter);
   return !reportMethodEnter;
   }


// TR::MethodExitCheck
char *
TR::RelocationRecordMethodExitCheck::name()
   {
   return "TR::MethodExitCheck";
   }

bool
TR::RelocationRecordMethodExitCheck::ignore(TR::RelocationRuntime *reloRuntime)
   {
   bool reportMethodExit = reloRuntime->fej9()->isMethodExitTracingEnabled((TR::OpaqueMethodBlock *) reloRuntime->method()) || reloRuntime->fej9()->canMethodExitEventBeHooked();
   RELO_LOG(reloRuntime->reloLogger(), 6,"\tignore: reportMethodExit %d\n", reportMethodExit);
   return !reportMethodExit;
   }


// TR::RelocationRecordValidateClass
char *
TR::RelocationRecordValidateClass::name()
   {
   return "TR::ValidateClass";
   }

void
TR::RelocationRecordValidateClass::print(TR::RelocationRuntime *reloRuntime)
   {
   TR::RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR::RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR::RelocationRecordConstantPoolWithIndex::print(reloRuntime);
   reloLogger->printf("\tclassChainOffsetInSharedClass %x\n", classChainOffsetInSharedCache(reloTarget));
   }

int32_t
TR::RelocationRecordValidateClass::bytesInHeaderAndPayload()
   {
   return sizeof(TR::RelocationRecordValidateClassBinaryTemplate);
   }

void
TR::RelocationRecordValidateClass::setClassChainOffsetInSharedCache(TR::RelocationTarget *reloTarget, uintptrj_t classChainOffsetInSharedCache)
   {
   reloTarget->storeRelocationRecordValue(classChainOffsetInSharedCache, (uintptrj_t *) &((TR::RelocationRecordValidateClassBinaryTemplate *)_record)->_classChainOffsetInSharedCache);
   }

uintptrj_t
TR::RelocationRecordValidateClass::classChainOffsetInSharedCache(TR::RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR::RelocationRecordValidateClassBinaryTemplate *)_record)->_classChainOffsetInSharedCache);
   }

void
TR::RelocationRecordValidateClass::preparePrivateData(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget)
   {
   }

bool
TR::RelocationRecordValidateClass::validateClass(TR::RelocationRuntime *reloRuntime, TR::OpaqueClassBlock *clazz, void *classChainOrRomClass)
   {
   // for classes and instance fields, need to compare clazz to entire class chain to make sure they are identical
   // classChainOrRomClass, for classes and instance fields, is a class chain pointer from the relocation record

   void *classChain = classChainOrRomClass;
   return reloRuntime->fej9()->sharedCache()->classMatchesCachedVersion(clazz, (uintptrj_t *) classChain);
   }

int32_t
TR::RelocationRecordValidateClass::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   reloRuntime->incNumValidations();

   J9ConstantPool *cp = (J9ConstantPool *)computeNewConstantPool(reloRuntime, reloTarget, constantPool(reloTarget));
   RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tapplyRelocation: cp %p\n", cp);
   TR::OpaqueClassBlock *definingClass = getClassFromCP(reloRuntime, reloTarget, (void *) cp);
   RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tapplyRelocation: definingClass %p\n", definingClass);

   int32_t returnCode = 0;
   bool verified = false;
   if (definingClass)
      {
      void *classChain = reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache((void *) classChainOffsetInSharedCache(reloTarget));
      verified = validateClass(reloRuntime, definingClass, classChain);
      }

   if (!verified)
      {
      RELO_LOG(reloRuntime->reloLogger(), 1, "\t\tapplyRelocation: could not verify class\n");
      returnCode = failureCode();
      }

   return returnCode;
   }

TR::OpaqueClassBlock *
TR::RelocationRecordValidateClass::getClassFromCP(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, void *void_cp)
   {
   TR::OpaqueClassBlock *definingClass = NULL;
   if (void_cp)
      {
      TR::VMAccessCriticalSection getClassFromCP(reloRuntime->fej9());
      J9JavaVM *javaVM = reloRuntime->javaVM();
      definingClass = (TR::OpaqueClassBlock *) javaVM->internalVMFunctions->resolveClassRef(javaVM->internalVMFunctions->currentVMThread(javaVM),
                                                                                           (J9ConstantPool *) void_cp,
                                                                                           cpIndex(reloTarget),
                                                                                           J9_RESOLVE_FLAG_AOT_LOAD_TIME);
      }

   return definingClass;
   }

int32_t
TR::RelocationRecordValidateClass::failureCode()
   {
   return compilationAotClassReloFailure;
   }


// TR::VerifyInstanceField
char *
TR::RelocationRecordValidateInstanceField::name()
   {
   return "TR::ValidateInstanceField";
   }

TR::OpaqueClassBlock *
TR::RelocationRecordValidateInstanceField::getClassFromCP(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, void *void_cp)
   {
   TR::OpaqueClassBlock *definingClass = NULL;
   if (void_cp)
      {
      J9JavaVM *javaVM = reloRuntime->javaVM();
      definingClass = reloRuntime->getClassFromCP(javaVM->internalVMFunctions->currentVMThread(javaVM), javaVM, (J9ConstantPool *) void_cp, cpIndex(reloTarget), false);
      }

   return definingClass;
   }

int32_t
TR::RelocationRecordValidateInstanceField::failureCode()
   {
   return compilationAotValidateFieldFailure;
   }

// TR::VerifyStaticField
char *
TR::RelocationRecordValidateStaticField::name()
   {
   return "TR::ValidateStaticField";
   }

void
TR::RelocationRecordValidateStaticField::print(TR::RelocationRuntime *reloRuntime)
   {
   TR::RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR::RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR::RelocationRecordConstantPoolWithIndex::print(reloRuntime);
   reloLogger->printf("\tromClassOffsetInSharedClass %x\n", romClassOffsetInSharedCache(reloTarget));
   }

int32_t
TR::RelocationRecordValidateStaticField::bytesInHeaderAndPayload()
   {
   return sizeof(TR::RelocationRecordValidateStaticFieldBinaryTemplate);
   }

void
TR::RelocationRecordValidateStaticField::setRomClassOffsetInSharedCache(TR::RelocationTarget *reloTarget, uintptrj_t romClassOffsetInSharedCache)
   {
   reloTarget->storeRelocationRecordValue(romClassOffsetInSharedCache, (uintptrj_t *) &((TR::RelocationRecordValidateStaticFieldBinaryTemplate *)_record)->_romClassOffsetInSharedCache);
   }

uintptrj_t
TR::RelocationRecordValidateStaticField::romClassOffsetInSharedCache(TR::RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR::RelocationRecordValidateStaticFieldBinaryTemplate *)_record)->_romClassOffsetInSharedCache);
   }

TR::OpaqueClassBlock *
TR::RelocationRecordValidateStaticField::getClass(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, void *void_cp)
   {
   TR::OpaqueClassBlock *definingClass = NULL;
   if (void_cp)
      {
      J9JavaVM *javaVM = reloRuntime->javaVM();
      definingClass = reloRuntime->getClassFromCP(javaVM->internalVMFunctions->currentVMThread(javaVM), javaVM, (J9ConstantPool *) void_cp, cpIndex(reloTarget), true);
      }

   return definingClass;
   }

bool
TR::RelocationRecordValidateStaticField::validateClass(TR::RelocationRuntime *reloRuntime, TR::OpaqueClassBlock *clazz, void *classChainOrRomClass)
   {
   // for static fields, all that matters is the romclass of the class matches (statics cannot be inherited)
   // classChainOrRomClass, for static fields, is a romclass pointer from the relocation record

   J9ROMClass *romClass = (J9ROMClass *) classChainOrRomClass;
   J9Class *j9class = (J9Class *)clazz;
   return j9class->romClass == romClass;
   }


// TR::RelocationRecordValidateArbitraryClass
char *
TR::RelocationRecordValidateArbitraryClass::name()
   {
   return "TR::ValidateArbitraryClass";
   }

void
TR::RelocationRecordValidateArbitraryClass::print(TR::RelocationRuntime *reloRuntime)
   {
   TR::RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR::RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR::RelocationRecord::print(reloRuntime);
   reloLogger->printf("\tclassChainIdentifyingLoaderOffset %p\n", classChainIdentifyingLoaderOffset(reloTarget));
   reloLogger->printf("\tclassChainOffsetForClassBeingValidated %p\n", classChainOffsetForClassBeingValidated(reloTarget));
   }


void
TR::RelocationRecordValidateArbitraryClass::setClassChainIdentifyingLoaderOffset(TR::RelocationTarget *reloTarget, uintptrj_t offset)
   {
   reloTarget->storeRelocationRecordValue(offset, (uintptrj_t *) &((TR::RelocationRecordValidateArbitraryClassBinaryTemplate *)_record)->_loaderClassChainOffset);
   }

uintptrj_t
TR::RelocationRecordValidateArbitraryClass::classChainIdentifyingLoaderOffset(TR::RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR::RelocationRecordValidateArbitraryClassBinaryTemplate *)_record)->_loaderClassChainOffset);
   }

void
TR::RelocationRecordValidateArbitraryClass::setClassChainOffsetForClassBeingValidated(TR::RelocationTarget *reloTarget, uintptrj_t offset)
   {
   reloTarget->storeRelocationRecordValue(offset, (uintptrj_t *) &((TR::RelocationRecordValidateArbitraryClassBinaryTemplate *)_record)->_classChainOffsetForClassBeingValidated);
   }

uintptrj_t
TR::RelocationRecordValidateArbitraryClass::classChainOffsetForClassBeingValidated(TR::RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR::RelocationRecordValidateArbitraryClassBinaryTemplate *)_record)->_classChainOffsetForClassBeingValidated);
   }

int32_t
TR::RelocationRecordValidateArbitraryClass::bytesInHeaderAndPayload()
   {
   return sizeof(TR::RelocationRecordValidateArbitraryClassBinaryTemplate);
   }

void
TR::RelocationRecordValidateArbitraryClass::preparePrivateData(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget)
   {
   }

int32_t
TR::RelocationRecordValidateArbitraryClass::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR::AOTStats *aotStats = reloRuntime->aotStats();
   if (aotStats)
      aotStats->numClassValidations++;

   void *classChainIdentifyingLoader = reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache((void*)classChainIdentifyingLoaderOffset(reloTarget));
   RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tpreparePrivateData: classChainIdentifyingLoader %p\n", classChainIdentifyingLoader);

   J9ClassLoader *classLoader = (J9ClassLoader *) reloRuntime->fej9()->sharedCache()->persistentClassLoaderTable()->lookupClassLoaderAssociatedWithClassChain(classChainIdentifyingLoader);
   RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tpreparePrivateData: classLoader %p\n", classLoader);

   if (classLoader)
      {
      uintptrj_t *classChainForClassBeingValidated = (uintptrj_t *) reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache((void*)classChainOffsetForClassBeingValidated(reloTarget));
      TR::OpaqueClassBlock *clazz = reloRuntime->fej9()->sharedCache()->lookupClassFromChainAndLoader(classChainForClassBeingValidated, classLoader);
      RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tpreparePrivateData: clazz %p\n", clazz);

      if (clazz)
         return 0;
      }

   if (aotStats)
      aotStats->numClassValidationsFailed++;

   return compilationAotClassReloFailure;
   }

int32_t
TR::RelocationRecordValidateClassByName::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t classID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateClassByNameBinaryTemplate *)_record)->_classID);
   uint16_t beholderID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateClassByNameBinaryTemplate *)_record)->_beholderID);
   char primitiveType = (char)reloTarget->loadUnsigned8b((uint8_t *) &((TR::RelocationRecordValidateClassByNameBinaryTemplate *)_record)->_primitiveType);
   void *romClassOffset = (void *)reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR::RelocationRecordValidateClassByNameBinaryTemplate *)_record)->_romClassOffsetInSCC);
   void *romClass = reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache(romClassOffset);

   if (reloRuntime->reloLogger()->logEnabled())
      {
      reloRuntime->reloLogger()->printf("%s\n", name());
      reloRuntime->reloLogger()->printf("\tapplyRelocation: classID %d\n", classID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: beholderID %d\n", beholderID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: primitiveType %c\n", primitiveType);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: romClass %p\n", romClass);
      }

   if (reloRuntime->comp()->getSymbolValidationManager()->validateClassByNameRecord(classID, beholderID, static_cast<J9ROMClass *>(romClass), primitiveType))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

int32_t
TR::RelocationRecordValidateProfiledClass::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t classID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateProfiledClassBinaryTemplate *)_record)->_classID);
   char primitiveType = (char)reloTarget->loadUnsigned8b((uint8_t *) &((TR::RelocationRecordValidateProfiledClassBinaryTemplate *)_record)->_primitiveType);

   void *classChainForCLOffset = (void *)reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR::RelocationRecordValidateProfiledClassBinaryTemplate *)_record)->_classChainOffsetForCLInScc);
   void *classChainForCL = reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache(classChainForCLOffset);

   void *classChainOffset = (void *)reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR::RelocationRecordValidateProfiledClassBinaryTemplate *)_record)->_classChainOffsetInSCC);
   void *classChain = reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache(classChainOffset);

   if (reloRuntime->reloLogger()->logEnabled())
      {
      reloRuntime->reloLogger()->printf("%s\n", name());
      reloRuntime->reloLogger()->printf("\tapplyRelocation: classID %d\n", classID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: primitiveType %c\n", primitiveType);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: classChainForCL %p\n", classChainForCL);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: classChain %p\n", classChain);
      }

   if (reloRuntime->comp()->getSymbolValidationManager()->validateProfiledClassRecord(classID, primitiveType, classChainForCL, classChain))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

int32_t
TR::RelocationRecordValidateClassFromCP::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t classID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateClassFromCPBinaryTemplate *)_record)->_classID);
   uint16_t beholderID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateClassFromCPBinaryTemplate *)_record)->_beholderID);
   uint32_t cpIndex = reloTarget->loadUnsigned32b((uint8_t *) &((TR::RelocationRecordValidateClassFromCPBinaryTemplate *)_record)->_cpIndex);

   if (reloRuntime->reloLogger()->logEnabled())
      {
      reloRuntime->reloLogger()->printf("%s\n", name());
      reloRuntime->reloLogger()->printf("\tapplyRelocation: classID %d\n", classID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: beholderID %d\n", beholderID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: cpIndex %d\n", cpIndex);
      }

   if (reloRuntime->comp()->getSymbolValidationManager()->validateClassFromCPRecord(classID, beholderID, cpIndex))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

int32_t
TR::RelocationRecordValidateDefiningClassFromCP::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t classID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateDefiningClassFromCPBinaryTemplate *)_record)->_classID);
   uint16_t beholderID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateDefiningClassFromCPBinaryTemplate *)_record)->_beholderID);
   uint32_t cpIndex = reloTarget->loadUnsigned32b((uint8_t *) &((TR::RelocationRecordValidateDefiningClassFromCPBinaryTemplate *)_record)->_cpIndex);
   bool isStatic = (bool)reloTarget->loadUnsigned8b((uint8_t *) &((TR::RelocationRecordValidateDefiningClassFromCPBinaryTemplate *)_record)->_isStatic);

   if (reloRuntime->reloLogger()->logEnabled())
      {
      reloRuntime->reloLogger()->printf("%s\n", name());
      reloRuntime->reloLogger()->printf("\tapplyRelocation: classID %d\n", classID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: beholderID %d\n", beholderID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: cpIndex %d\n", cpIndex);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: isStatic %s\n", isStatic ? "true" : "false");
      }

   if (reloRuntime->comp()->getSymbolValidationManager()->validateDefiningClassFromCPRecord(classID, beholderID, cpIndex, isStatic))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

int32_t
TR::RelocationRecordValidateStaticClassFromCP::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t classID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateStaticClassFromCPBinaryTemplate *)_record)->_classID);
   uint16_t beholderID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateStaticClassFromCPBinaryTemplate *)_record)->_beholderID);
   uint32_t cpIndex = reloTarget->loadUnsigned32b((uint8_t *) &((TR::RelocationRecordValidateStaticClassFromCPBinaryTemplate *)_record)->_cpIndex);

   if (reloRuntime->reloLogger()->logEnabled())
      {
      reloRuntime->reloLogger()->printf("%s\n", name());
      reloRuntime->reloLogger()->printf("\tapplyRelocation: classID %d\n", classID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: beholderID %d\n", beholderID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: cpIndex %d\n", cpIndex);
      }

   if (reloRuntime->comp()->getSymbolValidationManager()->validateStaticClassFromCPRecord(classID, beholderID, cpIndex))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

int32_t
TR::RelocationRecordValidateClassFromMethod::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t classID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateClassFromMethodBinaryTemplate *)_record)->_classID);
   uint16_t methodID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateClassFromMethodBinaryTemplate *)_record)->_methodID);

   if (reloRuntime->reloLogger()->logEnabled())
      {
      reloRuntime->reloLogger()->printf("%s\n", name());
      reloRuntime->reloLogger()->printf("\tapplyRelocation: classID %d\n", classID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: methodID %d\n", methodID);
      }

   if (reloRuntime->comp()->getSymbolValidationManager()->validateClassFromMethodRecord(classID, methodID))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

int32_t
TR::RelocationRecordValidateComponentClassFromArrayClass::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t componentClassID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateCompFromArrayBinaryTemplate *)_record)->_componentClassID);
   uint16_t arrayClassID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateCompFromArrayBinaryTemplate *)_record)->_arrayClassID);

   if (reloRuntime->reloLogger()->logEnabled())
      {
      reloRuntime->reloLogger()->printf("%s\n", name());
      reloRuntime->reloLogger()->printf("\tapplyRelocation: componentClassID %d\n", componentClassID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: arrayClassID %d\n", arrayClassID);
      }

   if (reloRuntime->comp()->getSymbolValidationManager()->validateComponentClassFromArrayClassRecord(componentClassID, arrayClassID))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

int32_t
TR::RelocationRecordValidateArrayClassFromComponentClass::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t arrayClassID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateArrayFromCompBinaryTemplate *)_record)->_arrayClassID);
   uint16_t componentClassID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateArrayFromCompBinaryTemplate *)_record)->_componentClassID);

   if (reloRuntime->reloLogger()->logEnabled())
      {
      reloRuntime->reloLogger()->printf("%s\n", name());
      reloRuntime->reloLogger()->printf("\tapplyRelocation: arrayClassID %d\n", arrayClassID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: componentClassID %d\n", componentClassID);
      }

   if (reloRuntime->comp()->getSymbolValidationManager()->validateArrayClassFromComponentClassRecord(arrayClassID, componentClassID))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

int32_t
TR::RelocationRecordValidateSuperClassFromClass::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t superClassID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateSuperClassFromClassBinaryTemplate *)_record)->_superClassID);
   uint16_t childClassID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateSuperClassFromClassBinaryTemplate *)_record)->_childClassID);

   if (reloRuntime->reloLogger()->logEnabled())
      {
      reloRuntime->reloLogger()->printf("%s\n", name());
      reloRuntime->reloLogger()->printf("\tapplyRelocation: superClassID %d\n", superClassID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: childClassID %d\n", childClassID);
      }

   if (reloRuntime->comp()->getSymbolValidationManager()->validateSuperClassFromClassRecord(superClassID, childClassID))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

int32_t
TR::RelocationRecordValidateClassInstanceOfClass::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t classOneID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateClassInstanceOfClassBinaryTemplate *)_record)->_classOneID);
   uint16_t classTwoID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateClassInstanceOfClassBinaryTemplate *)_record)->_classTwoID);
   bool objectTypeIsFixed = (bool)reloTarget->loadUnsigned8b((uint8_t *) &((TR::RelocationRecordValidateClassInstanceOfClassBinaryTemplate *)_record)->_objectTypeIsFixed);
   bool castTypeIsFixed = (bool)reloTarget->loadUnsigned8b((uint8_t *) &((TR::RelocationRecordValidateClassInstanceOfClassBinaryTemplate *)_record)->_castTypeIsFixed);
   bool isInstanceOf = (bool)reloTarget->loadUnsigned8b((uint8_t *) &((TR::RelocationRecordValidateClassInstanceOfClassBinaryTemplate *)_record)->_isInstanceOf);

   if (reloRuntime->reloLogger()->logEnabled())
      {
      reloRuntime->reloLogger()->printf("%s\n", name());
      reloRuntime->reloLogger()->printf("\tapplyRelocation: classOneID %d\n", classOneID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: classTwoID %d\n", classTwoID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: objectTypeIsFixed %s\n", objectTypeIsFixed ? "true" : "false'");
      reloRuntime->reloLogger()->printf("\tapplyRelocation: castTypeIsFixed %s\n", castTypeIsFixed ? "true" : "false'");
      reloRuntime->reloLogger()->printf("\tapplyRelocation: isInstanceOf %s\n", isInstanceOf ? "true" : "false'");
      }

   if (reloRuntime->comp()->getSymbolValidationManager()->validateClassInstanceOfClassRecord(classOneID, classTwoID, objectTypeIsFixed, castTypeIsFixed, isInstanceOf))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

int32_t
TR::RelocationRecordValidateSystemClassByName::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t systemClassID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateSystemClassByNameBinaryTemplate *)_record)->_systemClassID);
   void *romClassOffset = (void *)reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR::RelocationRecordValidateSystemClassByNameBinaryTemplate *)_record)->_romClassOffsetInSCC);
   void *romClass = reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache(romClassOffset);

   if (reloRuntime->reloLogger()->logEnabled())
      {
      reloRuntime->reloLogger()->printf("%s\n", name());
      reloRuntime->reloLogger()->printf("\tapplyRelocation: systemClassID %d\n", systemClassID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: romClass %p\n", romClass);
      }

   if (reloRuntime->comp()->getSymbolValidationManager()->validateSystemClassByNameRecord(systemClassID, static_cast<J9ROMClass *>(romClass)))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

int32_t
TR::RelocationRecordValidateClassFromITableIndexCP::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t classID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateClassFromITableIndexCPBinaryTemplate *)_record)->_classID);
   uint16_t beholderID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateClassFromITableIndexCPBinaryTemplate *)_record)->_beholderID);
   uint32_t cpIndex = reloTarget->loadUnsigned32b((uint8_t *) &((TR::RelocationRecordValidateClassFromITableIndexCPBinaryTemplate *)_record)->_cpIndex);

   if (reloRuntime->reloLogger()->logEnabled())
      {
      reloRuntime->reloLogger()->printf("%s\n", name());
      reloRuntime->reloLogger()->printf("\tapplyRelocation: classID %d\n", classID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: beholderID %d\n", beholderID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: cpIndex %d\n", cpIndex);
      }

   if (reloRuntime->comp()->getSymbolValidationManager()->validateClassFromITableIndexCPRecord(classID, beholderID, cpIndex))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

int32_t
TR::RelocationRecordValidateDeclaringClassFromFieldOrStatic::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t classID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateDeclaringClassFromFieldOrStaticBinaryTemplate *)_record)->_classID);
   uint16_t beholderID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateDeclaringClassFromFieldOrStaticBinaryTemplate *)_record)->_beholderID);
   uint32_t cpIndex = reloTarget->loadUnsigned32b((uint8_t *) &((TR::RelocationRecordValidateDeclaringClassFromFieldOrStaticBinaryTemplate *)_record)->_cpIndex);

   if (reloRuntime->reloLogger()->logEnabled())
      {
      reloRuntime->reloLogger()->printf("%s\n", name());
      reloRuntime->reloLogger()->printf("\tapplyRelocation: classID %d\n", classID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: beholderID %d\n", beholderID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: cpIndex %d\n", cpIndex);
      }

   if (reloRuntime->comp()->getSymbolValidationManager()->validateDeclaringClassFromFieldOrStaticRecord(classID, beholderID, cpIndex))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

int32_t
TR::RelocationRecordValidateClassClass::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t classClassID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateClassClassBinaryTemplate *)_record)->_classClassID);
   uint16_t objectClassID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateClassClassBinaryTemplate *)_record)->_objectClassID);

   if (reloRuntime->reloLogger()->logEnabled())
      {
      reloRuntime->reloLogger()->printf("%s\n", name());
      reloRuntime->reloLogger()->printf("\tapplyRelocation: classClassID %d\n", classClassID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: objectClassID %d\n", objectClassID);
      }

   if (reloRuntime->comp()->getSymbolValidationManager()->validateClassClassRecord(classClassID, objectClassID))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

int32_t
TR::RelocationRecordValidateConcreteSubClassFromClass::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t childClassID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateConcreteSubFromClassBinaryTemplate *)_record)->_childClassID);
   uint16_t superClassID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateConcreteSubFromClassBinaryTemplate *)_record)->_superClassID);

   if (reloRuntime->reloLogger()->logEnabled())
      {
      reloRuntime->reloLogger()->printf("%s\n", name());
      reloRuntime->reloLogger()->printf("\tapplyRelocation: childClassID %d\n", childClassID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: superClassID %d\n", superClassID);
      }

   if (reloRuntime->comp()->getSymbolValidationManager()->validateConcreteSubClassFromClassRecord(childClassID, superClassID))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

int32_t
TR::RelocationRecordValidateClassChain::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t classID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateClassChainBinaryTemplate *)_record)->_classID);
   void *classChainOffset = (void *)reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR::RelocationRecordValidateClassChainBinaryTemplate *)_record)->_classChainOffsetInSCC);
   void *classChain = reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache(classChainOffset);

   if (reloRuntime->reloLogger()->logEnabled())
      {
      reloRuntime->reloLogger()->printf("%s\n", name());
      reloRuntime->reloLogger()->printf("\tapplyRelocation: classID %d\n", classID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: classChain %p\n", classChain);
      }

   if (reloRuntime->comp()->getSymbolValidationManager()->validateClassChainRecord(classID, classChain))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

int32_t
TR::RelocationRecordValidateMethodByName::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t methodID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateMethodByNameBinaryTemplate *)_record)->_methodID);
   uint16_t beholderID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateMethodByNameBinaryTemplate *)_record)->_beholderID);

   void *romClassOffset = (void *)reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR::RelocationRecordValidateMethodByNameBinaryTemplate *)_record)->_romClassOffsetInSCC);
   void *romClass = reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache(romClassOffset);

   void *romMethodOffset = (void *)reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR::RelocationRecordValidateMethodByNameBinaryTemplate *)_record)->_romMethodOffsetInSCC);
   void *romMethod = reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache(romMethodOffset);

   if (reloRuntime->reloLogger()->logEnabled())
      {
      reloRuntime->reloLogger()->printf("%s\n", name());
      reloRuntime->reloLogger()->printf("\tapplyRelocation: methodID %d\n", methodID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: beholderID %d\n", beholderID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: romClass %p\n", romClass);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: romMethod %p\n", romMethod);
      }

   if (reloRuntime->comp()->getSymbolValidationManager()->validateMethodByNameRecord(methodID, beholderID, static_cast<J9ROMClass *>(romClass), static_cast<J9ROMMethod *>(romMethod)))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

int32_t
TR::RelocationRecordValidateMethodFromClass::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t methodID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateMethodFromClassBinaryTemplate *)_record)->_methodID);
   uint16_t beholderID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateMethodFromClassBinaryTemplate *)_record)->_beholderID);
   uint32_t index = reloTarget->loadUnsigned32b((uint8_t *) &((TR::RelocationRecordValidateMethodFromClassBinaryTemplate *)_record)->_index);

   if (reloRuntime->reloLogger()->logEnabled())
      {
      reloRuntime->reloLogger()->printf("%s\n", name());
      reloRuntime->reloLogger()->printf("\tapplyRelocation: methodID %d\n", methodID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: beholderID %d\n", beholderID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: index %d\n", index);
      }

   if (reloRuntime->comp()->getSymbolValidationManager()->validateMethodFromClassRecord(methodID, beholderID, index))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

int32_t
TR::RelocationRecordValidateStaticMethodFromCP::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t methodID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateStaticMethodFromCPBinaryTemplate *)_record)->_methodID);
   uint16_t beholderID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateStaticMethodFromCPBinaryTemplate *)_record)->_beholderID);
   uint32_t cpIndex = reloTarget->loadUnsigned32b((uint8_t *) &((TR::RelocationRecordValidateStaticMethodFromCPBinaryTemplate *)_record)->_cpIndex);

   if (reloRuntime->reloLogger()->logEnabled())
      {
      reloRuntime->reloLogger()->printf("%s\n", name());
      reloRuntime->reloLogger()->printf("\tapplyRelocation: methodID %d\n", methodID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: beholderID %d\n", beholderID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: cpIndex %d\n", cpIndex);
      }

   if (reloRuntime->comp()->getSymbolValidationManager()->validateStaticMethodFromCPRecord(methodID, beholderID, cpIndex))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

int32_t
TR::RelocationRecordValidateSpecialMethodFromCP::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t methodID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateSpecialMethodFromCPBinaryTemplate *)_record)->_methodID);
   uint16_t beholderID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateSpecialMethodFromCPBinaryTemplate *)_record)->_beholderID);
   uint32_t cpIndex = reloTarget->loadUnsigned32b((uint8_t *) &((TR::RelocationRecordValidateSpecialMethodFromCPBinaryTemplate *)_record)->_cpIndex);

   if (reloRuntime->reloLogger()->logEnabled())
      {
      reloRuntime->reloLogger()->printf("%s\n", name());
      reloRuntime->reloLogger()->printf("\tapplyRelocation: methodID %d\n", methodID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: beholderID %d\n", beholderID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: cpIndex %d\n", cpIndex);
      }

   if (reloRuntime->comp()->getSymbolValidationManager()->validateSpecialMethodFromCPRecord(methodID, beholderID, cpIndex))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

int32_t
TR::RelocationRecordValidateVirtualMethodFromCP::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t methodID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateVirtualMethodFromCPBinaryTemplate *)_record)->_methodID);
   uint16_t beholderID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateVirtualMethodFromCPBinaryTemplate *)_record)->_beholderID);
   uint32_t cpIndex = reloTarget->loadUnsigned32b((uint8_t *) &((TR::RelocationRecordValidateVirtualMethodFromCPBinaryTemplate *)_record)->_cpIndex);

   if (reloRuntime->reloLogger()->logEnabled())
      {
      reloRuntime->reloLogger()->printf("%s\n", name());
      reloRuntime->reloLogger()->printf("\tapplyRelocation: methodID %d\n", methodID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: beholderID %d\n", beholderID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: cpIndex %d\n", cpIndex);
      }

   if (reloRuntime->comp()->getSymbolValidationManager()->validateVirtualMethodFromCPRecord(methodID, beholderID, cpIndex))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

int32_t
TR::RelocationRecordValidateVirtualMethodFromOffset::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t methodID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateVirtualMethodFromOffsetBinaryTemplate *)_record)->_methodID);
   uint16_t beholderID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateVirtualMethodFromOffsetBinaryTemplate *)_record)->_beholderID);
   uint32_t virtualCallOffset = reloTarget->loadUnsigned32b((uint8_t *) &((TR::RelocationRecordValidateVirtualMethodFromOffsetBinaryTemplate *)_record)->_virtualCallOffset);
   bool ignoreRtResolve = (bool)reloTarget->loadUnsigned8b((uint8_t *) &((TR::RelocationRecordValidateVirtualMethodFromOffsetBinaryTemplate *)_record)->_ignoreRtResolve);

   if (reloRuntime->reloLogger()->logEnabled())
      {
      reloRuntime->reloLogger()->printf("%s\n", name());
      reloRuntime->reloLogger()->printf("\tapplyRelocation: methodID %d\n", methodID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: beholderID %d\n", beholderID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: virtualCallOffset %d\n", virtualCallOffset);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: ignoreRtResolve %s\n", ignoreRtResolve ? "true" : "false");
      }

   if (reloRuntime->comp()->getSymbolValidationManager()->validateVirtualMethodFromOffsetRecord(methodID, beholderID, virtualCallOffset, ignoreRtResolve))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

int32_t
TR::RelocationRecordValidateInterfaceMethodFromCP::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t methodID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateInterfaceMethodFromCPBinaryTemplate *)_record)->_methodID);
   uint16_t beholderID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateInterfaceMethodFromCPBinaryTemplate *)_record)->_beholderID);
   uint16_t lookupID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateInterfaceMethodFromCPBinaryTemplate *)_record)->_lookupID);
   uint32_t cpIndex = reloTarget->loadUnsigned32b((uint8_t *) &((TR::RelocationRecordValidateInterfaceMethodFromCPBinaryTemplate *)_record)->_cpIndex);

   if (reloRuntime->reloLogger()->logEnabled())
      {
      reloRuntime->reloLogger()->printf("%s\n", name());
      reloRuntime->reloLogger()->printf("\tapplyRelocation: methodID %d\n", methodID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: beholderID %d\n", beholderID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: lookupID %d\n", lookupID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: cpIndex %d\n", cpIndex);
      }

   if (reloRuntime->comp()->getSymbolValidationManager()->validateInterfaceMethodFromCPRecord(methodID, beholderID, lookupID, cpIndex))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

int32_t
TR::RelocationRecordValidateImproperInterfaceMethodFromCP::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t methodID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateImproperInterfaceMethodFromCPBinaryTemplate *)_record)->_methodID);
   uint16_t beholderID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateImproperInterfaceMethodFromCPBinaryTemplate *)_record)->_beholderID);
   uint32_t cpIndex = reloTarget->loadUnsigned32b((uint8_t *) &((TR::RelocationRecordValidateImproperInterfaceMethodFromCPBinaryTemplate *)_record)->_cpIndex);

   if (reloRuntime->reloLogger()->logEnabled())
      {
      reloRuntime->reloLogger()->printf("%s\n", name());
      reloRuntime->reloLogger()->printf("\tapplyRelocation: methodID %d\n", methodID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: beholderID %d\n", beholderID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: cpIndex %d\n", cpIndex);
      }

   if (reloRuntime->comp()->getSymbolValidationManager()->validateImproperInterfaceMethodFromCPRecord(methodID, beholderID, cpIndex))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

int32_t
TR::RelocationRecordValidateMethodFromClassAndSig::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t methodID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateMethodFromClassAndSigBinaryTemplate *)_record)->_methodID);
   uint16_t methodClassID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateMethodFromClassAndSigBinaryTemplate *)_record)->_methodClassID);
   uint16_t beholderID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateMethodFromClassAndSigBinaryTemplate *)_record)->_beholderID);

   void *romMethodOffset = (void *)reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR::RelocationRecordValidateMethodFromClassAndSigBinaryTemplate *)_record)->_romMethodOffsetInSCC);
   void *romMethod = reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache(romMethodOffset);

   if (reloRuntime->reloLogger()->logEnabled())
      {
      reloRuntime->reloLogger()->printf("%s\n", name());
      reloRuntime->reloLogger()->printf("\tapplyRelocation: methodID %d\n", methodID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: methodClassID %d\n", methodClassID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: beholderID %d\n", beholderID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: romMethod %p\n", romMethod);
      }

   if (reloRuntime->comp()->getSymbolValidationManager()->validateMethodFromClassAndSignatureRecord(methodID, methodClassID, beholderID, static_cast<J9ROMMethod *>(romMethod)))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

int32_t
TR::RelocationRecordValidateStackWalkerMaySkipFrames::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t methodID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateStackWalkerMaySkipFramesBinaryTemplate *)_record)->_methodID);
   uint16_t methodClassID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateStackWalkerMaySkipFramesBinaryTemplate *)_record)->_methodClassID);
   bool skipFrames = reloTarget->loadUnsigned8b((uint8_t *) &((TR::RelocationRecordValidateStackWalkerMaySkipFramesBinaryTemplate *)_record)->_skipFrames);

   if (reloRuntime->reloLogger()->logEnabled())
      {
      reloRuntime->reloLogger()->printf("%s\n", name());
      reloRuntime->reloLogger()->printf("\tapplyRelocation: methodID %d\n", methodID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: methodClassID %d\n", methodClassID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: skipFrames %s\n", skipFrames ? "true" : "false");
      }

   if (reloRuntime->comp()->getSymbolValidationManager()->validateStackWalkerMaySkipFramesRecord(methodID, methodClassID, skipFrames))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

int32_t
TR::RelocationRecordValidateClassInfoIsInitialized::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t classID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateClassInfoIsInitializedBinaryTemplate *)_record)->_classID);
   bool wasInitialized = (bool)reloTarget->loadUnsigned8b((uint8_t *) &((TR::RelocationRecordValidateClassInfoIsInitializedBinaryTemplate *)_record)->_isInitialized);

   if (reloRuntime->reloLogger()->logEnabled())
      {
      reloRuntime->reloLogger()->printf("%s\n", name());
      reloRuntime->reloLogger()->printf("\tapplyRelocation: classID %d\n", classID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: wasInitialized %s\n", wasInitialized ? "true" : "false");
      }

   if (reloRuntime->comp()->getSymbolValidationManager()->validateClassInfoIsInitializedRecord(classID, wasInitialized))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

int32_t
TR::RelocationRecordValidateMethodFromSingleImpl::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t methodID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateMethodFromSingleImplBinaryTemplate *)_record)->_methodID);
   uint16_t thisClassID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateMethodFromSingleImplBinaryTemplate *)_record)->_thisClassID);
   int32_t cpIndexOrVftSlot = reloTarget->loadSigned32b((uint8_t *) &((TR::RelocationRecordValidateMethodFromSingleImplBinaryTemplate *)_record)->_cpIndexOrVftSlot);
   uint16_t callerMethodID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateMethodFromSingleImplBinaryTemplate *)_record)->_callerMethodID);
   uint16_t useGetResolvedInterfaceMethod = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateMethodFromSingleImplBinaryTemplate *)_record)->_useGetResolvedInterfaceMethod);;

   if (reloRuntime->reloLogger()->logEnabled())
      {
      reloRuntime->reloLogger()->printf("%s\n", name());
      reloRuntime->reloLogger()->printf("\tapplyRelocation: methodID %d\n", methodID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: thisClassID %d\n", thisClassID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: cpIndexOrVftSlot %d\n", cpIndexOrVftSlot);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: callerMethodID %d\n", callerMethodID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: useGetResolvedInterfaceMethod %d\n", useGetResolvedInterfaceMethod);
      }

   if (reloRuntime->comp()->getSymbolValidationManager()->validateMethodFromSingleImplementerRecord(methodID,
                                                                                                    thisClassID,
                                                                                                    cpIndexOrVftSlot,
                                                                                                    callerMethodID,
                                                                                                    (TR::YesNoMaybe)useGetResolvedInterfaceMethod))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

int32_t
TR::RelocationRecordValidateMethodFromSingleInterfaceImpl::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t methodID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateMethodFromSingleInterfaceImplBinaryTemplate *)_record)->_methodID);
   uint16_t thisClassID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateMethodFromSingleInterfaceImplBinaryTemplate *)_record)->_thisClassID);
   int32_t cpIndex = reloTarget->loadSigned32b((uint8_t *) &((TR::RelocationRecordValidateMethodFromSingleInterfaceImplBinaryTemplate *)_record)->_cpIndex);
   uint16_t callerMethodID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateMethodFromSingleInterfaceImplBinaryTemplate *)_record)->_callerMethodID);

   if (reloRuntime->reloLogger()->logEnabled())
      {
      reloRuntime->reloLogger()->printf("%s\n", name());
      reloRuntime->reloLogger()->printf("\tapplyRelocation: methodID %d\n", methodID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: thisClassID %d\n", thisClassID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: cpIndex %d\n", cpIndex);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: callerMethodID %d\n", callerMethodID);
      }

   if (reloRuntime->comp()->getSymbolValidationManager()->validateMethodFromSingleInterfaceImplementerRecord(methodID,
                                                                                                             thisClassID,
                                                                                                             cpIndex,
                                                                                                             callerMethodID))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

int32_t
TR::RelocationRecordValidateMethodFromSingleAbstractImpl::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t methodID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateMethodFromSingleAbstractImplBinaryTemplate *)_record)->_methodID);
   uint16_t thisClassID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateMethodFromSingleAbstractImplBinaryTemplate *)_record)->_thisClassID);
   int32_t vftSlot = reloTarget->loadSigned32b((uint8_t *) &((TR::RelocationRecordValidateMethodFromSingleAbstractImplBinaryTemplate *)_record)->_vftSlot);
   uint16_t callerMethodID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordValidateMethodFromSingleAbstractImplBinaryTemplate *)_record)->_callerMethodID);

   if (reloRuntime->reloLogger()->logEnabled())
      {
      reloRuntime->reloLogger()->printf("%s\n", name());
      reloRuntime->reloLogger()->printf("\tapplyRelocation: methodID %d\n", methodID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: thisClassID %d\n", thisClassID);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: vftSlot %d\n", vftSlot);
      reloRuntime->reloLogger()->printf("\tapplyRelocation: callerMethodID %d\n", callerMethodID);
      }

   if (reloRuntime->comp()->getSymbolValidationManager()->validateMethodFromSingleAbstractImplementerRecord(methodID,
                                                                                                            thisClassID,
                                                                                                            vftSlot,
                                                                                                            callerMethodID))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

void
TR::RelocationRecordSymbolFromManager::preparePrivateData(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget)
   {
   TR::RelocationSymbolFromManagerPrivateData *reloPrivateData = &(privateData()->symbolFromManager);

   uint16_t symbolID = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordSymbolFromManagerBinaryTemplate *)_record)->_symbolID);
   uint16_t symbolType = reloTarget->loadUnsigned16b((uint8_t *) &((TR::RelocationRecordSymbolFromManagerBinaryTemplate *)_record)->_symbolType);

   if (reloRuntime->reloLogger()->logEnabled())
      {
      reloRuntime->reloLogger()->printf("%s\n", name());
      reloRuntime->reloLogger()->printf("\tpreparePrivateData: symbolID %d\n", symbolID);
      reloRuntime->reloLogger()->printf("\tpreparePrivateData: symbolType %d\n", symbolType);
      }

   reloPrivateData->_symbol = reloRuntime->comp()->getSymbolValidationManager()->getSymbolFromID(symbolID);
   reloPrivateData->_symbolType = symbolType;
   }

int32_t
TR::RelocationRecordSymbolFromManager::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR::RelocationSymbolFromManagerPrivateData *reloPrivateData = &(privateData()->symbolFromManager);

   void *symbol = reloPrivateData->_symbol;

   if (reloRuntime->reloLogger()->logEnabled())
      {
      reloRuntime->reloLogger()->printf("%s\n", name());
      reloRuntime->reloLogger()->printf("\tpreparePrivateData: symbol %p\n", symbol);
      }

   if (symbol)
      {
      reloTarget->storeAddressSequence((uint8_t *)symbol, reloLocation, reloFlags(reloTarget));
      activatePointer(reloRuntime, reloTarget, reloLocation);
      }
   else
      {
      return compilationAotClassReloFailure;
      }

   return 0;
   }

bool
TR::RelocationRecordSymbolFromManager::needsUnloadAssumptions(TR::SymbolType symbolType)
   {
   bool needsAssumptions = false;
   switch (symbolType)
      {
      case TR::SymbolType::typeClass:
      case TR::SymbolType::typeMethod:
         needsAssumptions = true;
      default:
         needsAssumptions = false;
      }
   return needsAssumptions;
   }

bool
TR::RelocationRecordSymbolFromManager::needsRedefinitionAssumption(TR::RelocationRuntime *reloRuntime, uint8_t *reloLocation, TR::OpaqueClassBlock *clazz, TR::SymbolType symbolType)
   {
   if (!reloRuntime->options()->getOption(TR::EnableHCR))
      return false;

   bool needsAssumptions = false;
   switch (symbolType)
      {
      case TR::SymbolType::typeClass:
         needsAssumptions =  TR::CodeGenerator::wantToPatchClassPointer(reloRuntime->comp(), clazz, reloLocation);
      case TR::SymbolType::typeMethod:
         needsAssumptions = true;
      default:
         needsAssumptions = false;
      }
   return needsAssumptions;
   }

void
TR::RelocationRecordSymbolFromManager::activatePointer(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR::RelocationSymbolFromManagerPrivateData *reloPrivateData = &(privateData()->symbolFromManager);
   TR::SymbolType symbolType = (TR::SymbolType)reloPrivateData->_symbolType;

   TR::OpaqueClassBlock *clazz = NULL;
   if (symbolType == TR::SymbolType::typeClass)
      {
      clazz = (TR::OpaqueClassBlock *)reloPrivateData->_symbol;
      }
   else if (symbolType == TR::SymbolType::typeMethod)
      {
      clazz = (TR::OpaqueClassBlock *)J9_CLASS_FROM_METHOD((J9Method *)(reloPrivateData->_symbol));
      }

   if (needsUnloadAssumptions(symbolType))
      {
      SVM_ASSERT(clazz != NULL, "clazz must exist to add Unload Assumptions!");
      reloTarget->addPICtoPatchPtrOnClassUnload(clazz, reloLocation);
      }
   if (needsRedefinitionAssumption(reloRuntime, reloLocation, clazz, symbolType))
      {
      SVM_ASSERT(clazz != NULL, "clazz must exist to add Redefinition Assumptions!");
      createClassRedefinitionPicSite((void *)reloPrivateData->_symbol, (void *) reloLocation, sizeof(uintptrj_t), false, reloRuntime->comp()->getMetadataAssumptionList());
      reloRuntime->comp()->setHasClassRedefinitionAssumptions();
      }
   }


// TR::HCR
char *
TR::RelocationRecordHCR::name()
   {
   return "TR::HCR";
   }

bool
TR::RelocationRecordHCR::ignore(TR::RelocationRuntime *reloRuntime)
   {
   bool hcrEnabled = reloRuntime->options()->getOption(TR::EnableHCR);
   RELO_LOG(reloRuntime->reloLogger(), 6,"\tignore: hcrEnabled %d\n", hcrEnabled);
   return !hcrEnabled;
   }

int32_t
TR::RelocationRecordHCR::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   void *methodAddress = (void *)reloRuntime->exceptionTable()->ramMethod;
   if (offset(reloTarget)) // non-NULL means resolved
      createClassRedefinitionPicSite(methodAddress, (void*)reloLocation, sizeof(UDATA), true, getMetadataAssumptionList(reloRuntime->exceptionTable()));
   else
   {
	   uint32_t locationSize = 1; // see OMR::RuntimeAssumption::isForAddressMaterializationSequence
	   if (reloFlags(reloTarget) & needsFullSizeRuntimeAssumption)
		   locationSize = sizeof(uintptrj_t);
      createClassRedefinitionPicSite((void*)-1, (void*)reloLocation, locationSize, true, getMetadataAssumptionList(reloRuntime->exceptionTable()));
   }
   return 0;
   }

// TR::Pointer
void
TR::RelocationRecordPointer::print(TR::RelocationRuntime *reloRuntime)
   {
   TR::RelocationRecordWithInlinedSiteIndex::print(reloRuntime);

   TR::RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR::RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   reloLogger->printf("\tclassChainIdentifyingLoaderOffsetInSharedCache %x\n", classChainIdentifyingLoaderOffsetInSharedCache(reloTarget));
   reloLogger->printf("\tclassChainForInlinedMethod %x\n", classChainForInlinedMethod(reloTarget));
   }

int32_t
TR::RelocationRecordPointer::bytesInHeaderAndPayload()
   {
   return sizeof(TR::RelocationRecordPointerBinaryTemplate);
   }

bool
TR::RelocationRecordPointer::ignore(TR::RelocationRuntime *reloRuntime)
   {
   // pointers must be updated because they tend to be guards controlling entry to inlined code
   // so even if the inlined site is disabled, the guard value needs to be changed to -1
   return false;
   }

void
TR::RelocationRecordPointer::setClassChainIdentifyingLoaderOffsetInSharedCache(TR::RelocationTarget *reloTarget, uintptrj_t classChainIdentifyingLoaderOffsetInSharedCache)
   {
   reloTarget->storeRelocationRecordValue(classChainIdentifyingLoaderOffsetInSharedCache, (uintptrj_t *) &((TR::RelocationRecordPointerBinaryTemplate *)_record)->_classChainIdentifyingLoaderOffsetInSharedCache);
   }

uintptrj_t
TR::RelocationRecordPointer::classChainIdentifyingLoaderOffsetInSharedCache(TR::RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR::RelocationRecordPointerBinaryTemplate *)_record)->_classChainIdentifyingLoaderOffsetInSharedCache);
   }

void
TR::RelocationRecordPointer::setClassChainForInlinedMethod(TR::RelocationTarget *reloTarget, uintptrj_t classChainForInlinedMethod)
   {
   reloTarget->storeRelocationRecordValue(classChainForInlinedMethod, (uintptrj_t *) &((TR::RelocationRecordPointerBinaryTemplate *)_record)->_classChainForInlinedMethod);
   }

uintptrj_t
TR::RelocationRecordPointer::classChainForInlinedMethod(TR::RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR::RelocationRecordPointerBinaryTemplate *)_record)->_classChainForInlinedMethod);
   }

void
TR::RelocationRecordPointer::preparePrivateData(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget)
   {
   TR::RelocationRecordPointerPrivateData *reloPrivateData = &(privateData()->pointer);

   J9Class *classPointer = NULL;
   if (getInlinedSiteMethod(reloRuntime, inlinedSiteIndex(reloTarget)) != (TR::OpaqueMethodBlock *)-1)
      {
      J9ClassLoader *classLoader = NULL;
#if !defined(PUBLIC_BUILD)
      void *classChainIdentifyingLoader = reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache((void *) classChainIdentifyingLoaderOffsetInSharedCache(reloTarget));
      RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: classChainIdentifyingLoader %p\n", classChainIdentifyingLoader);
      classLoader = (J9ClassLoader *) reloRuntime->fej9()->sharedCache()->persistentClassLoaderTable()->lookupClassLoaderAssociatedWithClassChain(classChainIdentifyingLoader);
#endif
      RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: classLoader %p\n", classLoader);

      if (classLoader != NULL)
         {
         uintptrj_t *classChain = (uintptrj_t *) reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache((void *) classChainForInlinedMethod(reloTarget));
         RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: classChain %p\n", classChain);

#if !defined(PUBLIC_BUILD)
         classPointer = (J9Class *) reloRuntime->fej9()->sharedCache()->lookupClassFromChainAndLoader(classChain, (void *) classLoader);
#endif
         RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: classPointer %p\n", classPointer);
         }
      }
   else
      RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: inlined site invalid\n");

   if (classPointer != NULL)
      {
      reloPrivateData->_activatePointer = true;
      reloPrivateData->_clazz = (TR::OpaqueClassBlock *) classPointer;
      reloPrivateData->_pointer = computePointer(reloTarget, reloPrivateData->_clazz);
      reloPrivateData->_needUnloadAssumption = !reloRuntime->fej9()->sameClassLoaders(reloPrivateData->_clazz, reloRuntime->comp()->getCurrentMethod()->classOfMethod());
      RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: pointer %p\n", reloPrivateData->_pointer);
      }
   else
      {
      reloPrivateData->_activatePointer = false;
      reloPrivateData->_clazz = (TR::OpaqueClassBlock *) -1;
      reloPrivateData->_pointer = (uintptrj_t) -1;
      reloPrivateData->_needUnloadAssumption = false;
      RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: class or loader NULL, or invalid site\n");
      }
   }

void
TR::RelocationRecordPointer::activatePointer(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR::RelocationRecordPointerPrivateData *reloPrivateData = &(privateData()->pointer);
   if (reloPrivateData->_needUnloadAssumption)
      {
      reloTarget->addPICtoPatchPtrOnClassUnload(reloPrivateData->_clazz, reloLocation);
      }
   }

void
TR::RelocationRecordPointer::registerHCRAssumption(TR::RelocationRuntime *reloRuntime, uint8_t *reloLocation)
   {
   TR::RelocationRecordPointerPrivateData *reloPrivateData = &(privateData()->pointer);
   createClassRedefinitionPicSite((void *) reloPrivateData->_pointer, (void *) reloLocation, sizeof(uintptrj_t), false, reloRuntime->comp()->getMetadataAssumptionList());
   reloRuntime->comp()->setHasClassRedefinitionAssumptions();
   }

int32_t
TR::RelocationRecordPointer::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR::RelocationRecordPointerPrivateData *reloPrivateData = &(privateData()->pointer);
   reloTarget->storePointer((uint8_t *)reloPrivateData->_pointer, reloLocation);
   if (reloPrivateData->_activatePointer)
      activatePointer(reloRuntime, reloTarget, reloLocation);
   return 0;
   }

// TR::ClassPointer
char *
TR::RelocationRecordClassPointer::name()
   {
   return "TR::ClassPointer";
   }

uintptrj_t
TR::RelocationRecordClassPointer::computePointer(TR::RelocationTarget *reloTarget, TR::OpaqueClassBlock *classPointer)
   {
   return (uintptrj_t) classPointer;
   }

void
TR::RelocationRecordClassPointer::activatePointer(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR::RelocationRecordPointer::activatePointer(reloRuntime, reloTarget, reloLocation);
   TR::RelocationRecordPointerPrivateData *reloPrivateData = &(privateData()->pointer);
   TR::ASSERT((void*)reloPrivateData->_pointer == (void*)reloPrivateData->_clazz, "Pointer differs from class pointer");
   if (TR::CodeGenerator::wantToPatchClassPointer(reloRuntime->comp(), reloPrivateData->_clazz, reloLocation))
      registerHCRAssumption(reloRuntime, reloLocation);
   }

// TR::ArbitraryClassAddress
char *
TR::RelocationRecordArbitraryClassAddress::name()
   {
   return "TR::ArbitraryClassAddress";
   }

int32_t
TR::RelocationRecordArbitraryClassAddress::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR::RelocationRecordPointerPrivateData *reloPrivateData = &(privateData()->pointer);
   TR::OpaqueClassBlock *clazz = (TR::OpaqueClassBlock*)reloPrivateData->_pointer;
   assertBootstrapLoader(reloRuntime, clazz);
   reloTarget->storeAddressSequence((uint8_t*)clazz, reloLocation, reloFlags(reloTarget));
   // No need to activatePointer(). See its definition below.
   return 0;
   }

int32_t
TR::RelocationRecordArbitraryClassAddress::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   TR::RelocationRecordPointerPrivateData *reloPrivateData = &(privateData()->pointer);
   TR::OpaqueClassBlock *clazz = (TR::OpaqueClassBlock*)reloPrivateData->_pointer;
   assertBootstrapLoader(reloRuntime, clazz);
   reloTarget->storeAddress((uint8_t*)clazz, reloLocationHigh, reloLocationLow, reloFlags(reloTarget));
   // No need to activatePointer(). See its definition below.
   return 0;
   }

void
TR::RelocationRecordArbitraryClassAddress::activatePointer(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   // applyRelocation() for this class doesn't activatePointer(), because it's
   // unnecessary. Class pointers are stable through redefinitions, and this
   // relocation is only valid for classes loaded by the bootstrap loader,
   // which can't be unloaded.
   //
   // This non-implementation is here to ensure that we don't create runtime
   // assumptions that are inappropriate to the code layout, which may be
   // different from the layout expected for other "pointer" relocations.

   TR::ASSERT_FATAL(
      false,
      "TR::RelocationRecordArbitraryClassAddress::activatePointer() is unimplemented\n");
   }

void
TR::RelocationRecordArbitraryClassAddress::assertBootstrapLoader(TR::RelocationRuntime *reloRuntime, TR::OpaqueClassBlock *clazz)
   {
   void *loader = reloRuntime->fej9()->getClassLoader(clazz);
   void *bootstrapLoader = reloRuntime->javaVM()->systemClassLoader;
   TR::ASSERT_FATAL(
      loader == bootstrapLoader,
      "TR::ArbitraryClassAddress relocation must use bootstrap loader\n");
   }

// TR::MethodPointer
char *
TR::RelocationRecordMethodPointer::name()
   {
   return "TR::MethodPointer";
   }

void
TR::RelocationRecordMethodPointer::print(TR::RelocationRuntime *reloRuntime)
   {
   TR::RelocationRecordPointer::print(reloRuntime);

   TR::RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR::RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   reloLogger->printf("\tvTableSlot %x\n", vTableSlot(reloTarget));
   }

void
TR::RelocationRecordMethodPointer::setVTableSlot(TR::RelocationTarget *reloTarget, uintptrj_t vTableSlot)
   {
   reloTarget->storeRelocationRecordValue(vTableSlot, (uintptrj_t *) &((TR::RelocationRecordMethodPointerBinaryTemplate *)_record)->_vTableSlot);
   }

uintptrj_t
TR::RelocationRecordMethodPointer::vTableSlot(TR::RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR::RelocationRecordMethodPointerBinaryTemplate *)_record)->_vTableSlot);
   }

int32_t
TR::RelocationRecordMethodPointer::bytesInHeaderAndPayload()
   {
   return sizeof(TR::RelocationRecordMethodPointerBinaryTemplate);
   }

uintptrj_t
TR::RelocationRecordMethodPointer::computePointer(TR::RelocationTarget *reloTarget, TR::OpaqueClassBlock *classPointer)
   {
   //return (uintptrj_t) getInlinedSiteMethod(reloTarget->reloRuntime(), inlinedSiteIndex(reloTarget));

   J9Method *method = (J9Method *) *(uintptrj_t *)(((uint8_t *)classPointer) + vTableSlot(reloTarget));
   TR::OpaqueClassBlock *clazz = (TR::OpaqueClassBlock *) J9_CLASS_FROM_METHOD(method);
   if (0 && *((uint32_t *)clazz) != 0x99669966)
      {
      TR::RelocationRuntime *reloRuntime = reloTarget->reloRuntime();
      RELO_LOG(reloRuntime->reloLogger(), 7, "\tpreparePrivateData: SUSPICIOUS j9method %p\n", method);
      J9UTF8 *className = J9ROMCLASS_CLASSNAME(((J9Class*)classPointer)->romClass);
      RELO_LOG(reloRuntime->reloLogger(), 7, "\tpreparePrivateData: classPointer %p %.*s\n", classPointer, J9UTF8_LENGTH(className), J9UTF8_DATA(className));
      RELO_LOG(reloRuntime->reloLogger(), 7, "\tpreparePrivateData: method's clazz %p\n", clazz);
      abort();
      }

   return (uintptrj_t) method;
   }

void
TR::RelocationRecordMethodPointer::activatePointer(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR::RelocationRecordPointer::activatePointer(reloRuntime, reloTarget, reloLocation);
   if (reloRuntime->options()->getOption(TR::EnableHCR))
      registerHCRAssumption(reloRuntime, reloLocation);
   }

int32_t
TR::RelocationRecordEmitClass::bytesInHeaderAndPayload()
   {
   return sizeof(TR::RelocationRecordEmitClassBinaryTemplate);
   }

void
TR::RelocationRecordEmitClass::preparePrivateData(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget)
   {
   TR::RelocationRecordEmitClassPrivateData *reloPrivateData = &(privateData()->emitClass);

   reloPrivateData->_bcIndex              = reloTarget->loadSigned32b((uint8_t *) &(((TR::RelocationRecordEmitClassBinaryTemplate *)_record)->_bcIndex));
   reloPrivateData->_method               = getInlinedSiteMethod(reloRuntime);
   }

int32_t
TR::RelocationRecordEmitClass::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR::RelocationRecordEmitClassPrivateData *reloPrivateData = &(privateData()->emitClass);

   reloRuntime->addClazzRecord(reloLocation, reloPrivateData->_bcIndex, reloPrivateData->_method);
   return 0;
   }


char *
TR::RelocationRecordDebugCounter::name()
   {
   return "TR::RelocationRecordDebugCounter";
   }

int32_t
TR::RelocationRecordDebugCounter::bytesInHeaderAndPayload()
   {
   return sizeof(TR::RelocationRecordDebugCounterBinaryTemplate);
   }

void
TR::RelocationRecordDebugCounter::preparePrivateData(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget)
   {
   TR::RelocationRecordDebugCounterPrivateData *reloPrivateData = &(privateData()->debugCounter);

   IDATA callerIndex = (IDATA)inlinedSiteIndex(reloTarget);
   if (callerIndex != -1)
      {
      reloPrivateData->_method = getInlinedSiteMethod(reloRuntime, callerIndex);
      }
   else
      {
      reloPrivateData->_method = NULL;
      }

   reloPrivateData->_bcIndex     = reloTarget->loadSigned32b((uint8_t *) &(((TR::RelocationRecordDebugCounterBinaryTemplate *)_record)->_bcIndex));
   reloPrivateData->_delta       = reloTarget->loadSigned32b((uint8_t *) &(((TR::RelocationRecordDebugCounterBinaryTemplate *)_record)->_delta));
   reloPrivateData->_fidelity    = reloTarget->loadUnsigned8b((uint8_t *) &(((TR::RelocationRecordDebugCounterBinaryTemplate *)_record)->_fidelity));
   reloPrivateData->_staticDelta = reloTarget->loadSigned32b((uint8_t *) &(((TR::RelocationRecordDebugCounterBinaryTemplate *)_record)->_staticDelta));

   UDATA offset                  = (UDATA)reloTarget->loadPointer((uint8_t *) &(((TR::RelocationRecordDebugCounterBinaryTemplate *)_record)->_offsetOfNameString));
   reloPrivateData->_name        =  reloRuntime->fej9()->sharedCache()->getDebugCounterName(offset);
   }

int32_t
TR::RelocationRecordDebugCounter::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR::DebugCounterBase *counter = findOrCreateCounter(reloRuntime);
   if (counter == NULL)
      {
      /*
       * We don't have to return -1 here and fail the relocation. We can always just allocate some memory
       * and patch the update location to that. However, given that it's likely that the developer wishes
       * to have debug counters run, it's probably better to fail the relocation.
       *
       */
      return -1;
      }

   // Update Counter Location
   reloTarget->storeAddressSequence((uint8_t *)counter->getBumpCountAddress(), reloLocation, reloFlags(reloTarget));

   return 0;
   }

int32_t
TR::RelocationRecordDebugCounter::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   TR::DebugCounterBase *counter = findOrCreateCounter(reloRuntime);
   if (counter == NULL)
      {
      /*
       * We don't have to return -1 here and fail the relocation. We can always just allocate some memory
       * and patch the update location to that. However, given that it's likely that the developer wishes
       * to have debug counters run, it's probably better to fail the relocation.
       *
       */
      return -1;
      }

   // Update Counter Location
   reloTarget->storeAddress((uint8_t *)counter->getBumpCountAddress(), reloLocationHigh, reloLocationLow, reloFlags(reloTarget));

   return 0;
   }

TR::DebugCounterBase *
TR::RelocationRecordDebugCounter::findOrCreateCounter(TR::RelocationRuntime *reloRuntime)
   {
   TR::DebugCounterBase *counter = NULL;
   TR::RelocationRecordDebugCounterPrivateData *reloPrivateData = &(privateData()->debugCounter);
   TR::Compilation *comp = reloRuntime->comp();
   bool isAggregateCounter = reloPrivateData->_delta == 0 ? false : true;

   if (reloPrivateData->_name == NULL ||
       (isAggregateCounter && reloPrivateData->_method == (TR::OpaqueMethodBlock *)-1))
      {
      return NULL;
      }

   // Find or Create Debug Counter
   if (isAggregateCounter)
      {
      counter = comp->getPersistentInfo()->getDynamicCounters()->findAggregation(reloPrivateData->_name, strlen(reloPrivateData->_name));
      if (!counter)
         {
         TR::DebugCounterAggregation *aggregatedCounters = comp->getPersistentInfo()->getDynamicCounters()->createAggregation(comp, reloPrivateData->_name);
         if (aggregatedCounters)
            {
            aggregatedCounters->aggregateStandardCounters(comp,
                                                          reloPrivateData->_method,
                                                          reloPrivateData->_bcIndex,
                                                          reloPrivateData->_name,
                                                          reloPrivateData->_delta,
                                                          reloPrivateData->_fidelity,
                                                          reloPrivateData->_staticDelta);
            if (!aggregatedCounters->hasAnyCounters())
               return NULL;
            }
         counter = aggregatedCounters;
         }
      }
   else
      {
      counter = TR::DebugCounter::getDebugCounter(comp,
                                                  reloPrivateData->_name,
                                                  reloPrivateData->_fidelity,
                                                  reloPrivateData->_staticDelta);
      }

   return counter;
   }

// ClassUnloadAssumption
char *
TR::RelocationRecordClassUnloadAssumption::name()
   {
   return "TR::ClassUnloadAssumption";
   }

int32_t
TR::RelocationRecordClassUnloadAssumption::bytesInHeaderAndPayload()
   {
   return sizeof(TR::RelocationRecordClassUnloadAssumptionBinaryTemplate);
   }

int32_t
TR::RelocationRecordClassUnloadAssumption::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   reloTarget->addPICtoPatchPtrOnClassUnload((TR::OpaqueClassBlock *) -1, reloLocation);
   return 0;
   }



uint32_t TR::RelocationRecord::_relocationRecordHeaderSizeTable[TR::NumExternalRelocationKinds] =
   {
   sizeof(TR::RelocationRecordConstantPoolBinaryTemplate),                            // TR::ConstantPool                                 = 0
   sizeof(TR::RelocationRecordHelperAddressBinaryTemplate),                           // TR::HelperAddress                                = 1
   24,                                                                               // TR::RelativeMethodAddress                        = 2
   sizeof(TR::RelocationRecordBinaryTemplate),                                        // TR::AbsoluteMethodAddress                        = 3
   sizeof(TR::RelocationRecordDataAddressBinaryTemplate),                             // TR::DataAddress                                  = 4
   24,                                                                               // TR::ClassObject                                  = 5
   sizeof(TR::RelocationRecordConstantPoolBinaryTemplate),                            // TR::MethodObject                                 = 6
   24,                                                                               // TR::InterfaceObject                              = 7
   sizeof(TR::RelocationRecordHelperAddressBinaryTemplate),                           // TR::AbsoluteHelperAddress                        = 8
   16,                                                                               // TR::FixedSeqAddress                              = 9
   16,                                                                               // TR::FixedSeq2Address                             = 10
   sizeof(TR::RelocationRecordConstantPoolWithIndexBinaryTemplate),                   // TR::JNIVirtualTargetAddress                      = 11
   sizeof(TR::RelocationRecordConstantPoolWithIndexBinaryTemplate),                   // TR::JNIStaticTargetAddress                       = 12
   4,                                                                                // Dummy for TR::ArrayCopyHelper                    = 13
   4,                                                                                // Dummy for TR::ArrayCopyToc                       = 14
   sizeof(TR::RelocationRecordBinaryTemplate),                                        // TR::BodyInfoAddress                              = 15
   sizeof(TR::RelocationRecordConstantPoolBinaryTemplate),                            // TR::Thunks                                       = 16
   sizeof(TR::RelocationRecordConstantPoolWithIndexBinaryTemplate),                   // TR::StaticRamMethodConst                         = 17
   sizeof(TR::RelocationRecordConstantPoolBinaryTemplate),                            // TR::Trampolines                                  = 18
   sizeof(TR::RelocationRecordPicTrampolineBinaryTemplate),                           // TR::PicTrampolines                               = 19
   sizeof(TR::RelocationRecordMethodTracingCheckBinaryTemplate),                      // TR::CheckMethodEnter                             = 20
   sizeof(TR::RelocationRecordBinaryTemplate),                                        // TR::RamMethod                                    = 21
   sizeof(TR::RelocationRecordWithOffsetBinaryTemplate),                              // TR::RamMethodSequence                            = 22
   sizeof(TR::RelocationRecordWithOffsetBinaryTemplate),                              // TR::RamMethodSequenceReg                         = 23
   sizeof(TR::RelocationRecordVerifyClassObjectForAllocBinaryTemplate),               // TR::VerifyClassObjectForAlloc                    = 24
   sizeof(TR::RelocationRecordConstantPoolBinaryTemplate),                            // TR::ConstantPoolOrderedPair                      = 25
   sizeof(TR::RelocationRecordBinaryTemplate),                                        // TR::AbsoluteMethodAddressOrderedPair             = 26
   sizeof(TR::RelocationRecordInlinedAllocationBinaryTemplate),                       // TR::VerifyRefArrayForAlloc                       = 27
   24,                                                                               // TR::J2IThunks                                    = 28
   sizeof(TR::RelocationRecordWithOffsetBinaryTemplate),                              // TR::GlobalValue                                  = 29
   4,                                                                                // dummy for TR::BodyInfoAddress                    = 30
   sizeof(TR::RelocationRecordValidateClassBinaryTemplate),                           // TR::ValidateInstanceField                        = 31
   sizeof(TR::RelocationRecordNopGuardBinaryTemplate),                                // TR::InlinedStaticMethodWithNopGuard              = 32
   sizeof(TR::RelocationRecordNopGuardBinaryTemplate),                                // TR::InlinedSpecialMethodWithNopGuard             = 33
   sizeof(TR::RelocationRecordNopGuardBinaryTemplate),                                // TR::InlinedVirtualMethodWithNopGuard             = 34
   sizeof(TR::RelocationRecordNopGuardBinaryTemplate),                                // TR::InlinedInterfaceMethodWithNopGuard           = 35
   sizeof(TR::RelocationRecordConstantPoolWithIndexBinaryTemplate),                   // TR::SpecialRamMethodConst                        = 36
   48,                                                                               // TR::InlinedHCRMethod                             = 37
   sizeof(TR::RelocationRecordValidateStaticFieldBinaryTemplate),                     // TR::ValidateStaticField                          = 38
   sizeof(TR::RelocationRecordValidateClassBinaryTemplate),                           // TR::ValidateClass                                = 39
   sizeof(TR::RelocationRecordConstantPoolWithIndexBinaryTemplate),                   // TR::ClassAddress                                 = 40
   sizeof(TR::RelocationRecordWithOffsetBinaryTemplate),                              // TR::HCR                                          = 41
   sizeof(TR::RelocationRecordProfiledInlinedMethodBinaryTemplate),                   // TR::ProfiledMethodGuardRelocation                = 42
   sizeof(TR::RelocationRecordProfiledInlinedMethodBinaryTemplate),                   // TR::ProfiledClassGuardRelocation                 = 43
   0,                                                                                // TR::HierarchyGuardRelocation                     = 44
   0,                                                                                // TR::AbstractGuardRelocation                      = 45
   sizeof(TR::RelocationRecordProfiledInlinedMethodBinaryTemplate),                   // TR::ProfiledInlinedMethodRelocation              = 46
   sizeof(TR::RelocationRecordMethodPointerBinaryTemplate),                           // TR::MethodPointer                                = 47
   sizeof(TR::RelocationRecordPointerBinaryTemplate),                                 // TR::ClassPointer                                 = 48
   sizeof(TR::RelocationRecordMethodTracingCheckBinaryTemplate),                      // TR::CheckMethodExit                              = 49
   sizeof(TR::RelocationRecordValidateArbitraryClassBinaryTemplate),                  // TR::ValidateArbitraryClass                       = 50
   0,                                                                                // TR::EmitClass(not used)                          = 51
   sizeof(TR::RelocationRecordConstantPoolWithIndexBinaryTemplate),                   // TR::JNISpecialTargetAddress                      = 52
   sizeof(TR::RelocationRecordConstantPoolWithIndexBinaryTemplate),                   // TR::VirtualRamMethodConst                        = 53
   sizeof(TR::RelocationRecordInlinedMethodBinaryTemplate),                           // TR::InlinedInterfaceMethod                       = 54
   sizeof(TR::RelocationRecordInlinedMethodBinaryTemplate),                           // TR::InlinedVirtualMethod                         = 55
   0,                                                                                // TR::NativeMethodAbsolute                         = 56
   0,                                                                                // TR::NativeMethodRelative                         = 57
   sizeof(TR::RelocationRecordPointerBinaryTemplate),                                 // TR::ArbitraryClassAddress                        = 58
   sizeof(TR::RelocationRecordDebugCounterBinaryTemplate),                            // TR::DebugCounter                                 = 59
   sizeof(TR::RelocationRecordClassUnloadAssumptionBinaryTemplate),                   // TR::ClassUnloadAssumption                        = 60
   sizeof(TR::RelocationRecordJ2IVirtualThunkPointerBinaryTemplate),                  // TR::J2IVirtualThunkPointer                       = 61
   sizeof(TR::RelocationRecordNopGuardBinaryTemplate),                                // TR::InlinedAbstractMethodWithNopGuard            = 62
   0,                                                                                // TR::ValidateRootClass                            = 63
   sizeof(TR::RelocationRecordValidateClassByNameBinaryTemplate),                     // TR::ValidateClassByName                          = 64
   sizeof(TR::RelocationRecordValidateProfiledClassBinaryTemplate),                   // TR::ValidateProfiledClass                        = 65
   sizeof(TR::RelocationRecordValidateClassFromCPBinaryTemplate),                     // TR::ValidateClassFromCP                          = 66
   sizeof(TR::RelocationRecordValidateDefiningClassFromCPBinaryTemplate),             // TR::ValidateDefiningClassFromCP                  = 67
   sizeof(TR::RelocationRecordValidateStaticClassFromCPBinaryTemplate),               // TR::ValidateStaticClassFromCP                    = 68
   sizeof(TR::RelocationRecordValidateClassFromMethodBinaryTemplate),                 // TR::ValidateClassFromMethod                      = 69
   sizeof(TR::RelocationRecordValidateCompFromArrayBinaryTemplate),                   // TR::ValidateComponentClassFromArrayClass         = 70
   sizeof(TR::RelocationRecordValidateArrayFromCompBinaryTemplate),                   // TR::ValidateArrayClassFromComponentClass         = 71
   sizeof(TR::RelocationRecordValidateSuperClassFromClassBinaryTemplate),             // TR::ValidateSuperClassFromClass                  = 72
   sizeof(TR::RelocationRecordValidateClassInstanceOfClassBinaryTemplate),            // TR::ValidateClassInstanceOfClass                 = 73
   sizeof(TR::RelocationRecordValidateSystemClassByNameBinaryTemplate),               // TR::ValidateSystemClassByName                    = 74
   sizeof(TR::RelocationRecordValidateClassFromITableIndexCPBinaryTemplate),          // TR::ValidateClassFromITableIndexCP               = 75
   sizeof(TR::RelocationRecordValidateDeclaringClassFromFieldOrStaticBinaryTemplate), // TR::ValidateDeclaringClassFromFieldOrStatic      = 76
   sizeof(TR::RelocationRecordValidateClassClassBinaryTemplate),                      // TR::ValidateClassClass                           = 77
   sizeof(TR::RelocationRecordValidateConcreteSubFromClassBinaryTemplate),            // TR::ValidateConcreteSubClassFromClass            = 78
   sizeof(TR::RelocationRecordValidateClassChainBinaryTemplate),                      // TR::ValidateClassChain                           = 79
   0,                                                                                // TR::ValidateRomClass                             = 80
   0,                                                                                // TR::ValidatePrimitiveClass                       = 81
   0,                                                                                // TR::ValidateMethodFromInlinedSite                = 82
   sizeof(TR::RelocationRecordValidateMethodByNameBinaryTemplate),                    // TR::ValidatedMethodByName                        = 83
   sizeof(TR::RelocationRecordValidateMethodFromClassBinaryTemplate),                 // TR::ValidatedMethodFromClass                     = 84
   sizeof(TR::RelocationRecordValidateStaticMethodFromCPBinaryTemplate),              // TR::ValidateStaticMethodFromCP                   = 85
   sizeof(TR::RelocationRecordValidateSpecialMethodFromCPBinaryTemplate),             // TR::ValidateSpecialMethodFromCP                  = 86
   sizeof(TR::RelocationRecordValidateVirtualMethodFromCPBinaryTemplate),             // TR::ValidateVirtualMethodFromCP                  = 87
   sizeof(TR::RelocationRecordValidateVirtualMethodFromOffsetBinaryTemplate),         // TR::ValidateVirtualMethodFromOffset              = 88
   sizeof(TR::RelocationRecordValidateInterfaceMethodFromCPBinaryTemplate),           // TR::ValidateInterfaceMethodFromCP                = 89
   sizeof(TR::RelocationRecordValidateMethodFromClassAndSigBinaryTemplate),           // TR::ValidateMethodFromClassAndSig                = 90
   sizeof(TR::RelocationRecordValidateStackWalkerMaySkipFramesBinaryTemplate),        // TR::ValidateStackWalkerMaySkipFramesRecord       = 91
   0,                                                                                // TR::ValidateArrayClassFromJavaVM                 = 92
   sizeof(TR::RelocationRecordValidateClassInfoIsInitializedBinaryTemplate),          // TR::ValidateClassInfoIsInitialized               = 93
   sizeof(TR::RelocationRecordValidateMethodFromSingleImplBinaryTemplate),            // TR::ValidateMethodFromSingleImplementer          = 94
   sizeof(TR::RelocationRecordValidateMethodFromSingleInterfaceImplBinaryTemplate),   // TR::ValidateMethodFromSingleInterfaceImplementer = 95
   sizeof(TR::RelocationRecordValidateMethodFromSingleAbstractImplBinaryTemplate),    // TR::ValidateMethodFromSingleAbstractImplementer  = 96
   sizeof(TR::RelocationRecordValidateImproperInterfaceMethodFromCPBinaryTemplate),   // TR::ValidateImproperInterfaceMethodFromCP        = 97
   sizeof(TR::RelocationRecordSymbolFromManagerBinaryTemplate),                       // TR::SymbolFromManager                            = 98
   };

