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
#include <iostream>
#include <string>
#include "omrcfg.h"
#include "codegen/CodeGenerator.hpp"
#include "codegen/FrontEnd.hpp"
#include "env/ConcreteFE.hpp"
#include "codegen/Relocation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "infra/Assert.hpp"
#include "env/jittypes.h"
#include "runtime/RelocationRuntimeLogger.hpp"
#include "env/VMAccessCriticalSection.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "infra/SimpleRegex.hpp"
#include "runtime/CodeCache.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/RelocationRecord.hpp"
#include "runtime/RelocationRuntime.hpp"
#include "runtime/RelocationTarget.hpp"
#include "runtime/SymbolValidationManager.hpp"
#include "runtime/OMRRelocationRecord.hpp"
#include "runtime/OMRRuntimeAssumptions.hpp"
#include "compile/DisplacementSites.hpp"

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

// OMR::RelocationRecordGroup
TR::RelocationRecord *
OMR::RelocationRecord::self()
   {
   return static_cast<TR::RelocationRecord *>(this);
   }

void
OMR::RelocationRecordGroup::setSize(TR::RelocationTarget *reloTarget,uintptr_t size)
   {
   reloTarget->storePointer((uint8_t *)size, (uint8_t *) _dataBuffer);
   }

uintptr_t
OMR::RelocationRecordGroup::size(TR::RelocationTarget *reloTarget)
   {
   return (uintptr_t)reloTarget->loadPointer((uint8_t *) _dataBuffer);
   }

TR::RelocationRecordBinaryTemplate *
OMR::RelocationRecordGroup::firstRecord(TR::RelocationTarget *reloTarget)
   {
   // first word of the group is a pointer size field for the entire group
   return (TR::RelocationRecordBinaryTemplate *) (((uintptr_t *)_dataBuffer)+1);
   }

TR::RelocationRecordBinaryTemplate *
OMR::RelocationRecordGroup::pastLastRecord(TR::RelocationTarget *reloTarget)
   {
   return (TR::RelocationRecordBinaryTemplate *) ((uint8_t *)_dataBuffer + size(reloTarget));
   }

int32_t
OMR::RelocationRecordGroup::applyRelocations(TR::RelocationRuntime *reloRuntime,
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

      recordPointer =  reinterpret_cast<TR::RelocationRecordBinaryTemplate *>(reloRecord->nextBinaryRecord(reloTarget));
      }

   return 0;
   }


int32_t
OMR::RelocationRecordGroup::handleRelocation(TR::RelocationRuntime *reloRuntime,
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
   OMR::RelocationRecord *reloRecord = NULL;
   // based on the type of the relocation record, create an object of a particular variety of OMR::RelocationRecord object
   uint8_t reloType = record->type(reloTarget);
   switch (reloType)
      {
      case TR_MethodCallAddress:
        reloRecord = new (storage) OMR::RelocationRecordMethodCallAddress(reloRuntime, record);
        break;
      case TR_DataAddress:
        reloRecord = new (storage) OMR::RelocationRecordDataAddress(reloRuntime,record);
        break;
      case TR_ArbitrarySizedHeader:
        reloRecord = new (storage) OMR::RelocationRecordArbitrarySizedHeader(reloRuntime,record);
        break;
      case TR_DisplacementSiteRelocation:
	 reloRecord = new (storage) OMR::RelocationRecordDisplacementSite(reloRuntime,record);
        break;
      default:
         // TODO: error condition
         printf("Unexpected relo record: %d\n", reloType);fflush(stdout);
         exit(0);
      }
   
   return static_cast<TR::RelocationRecord*>(reloRecord);
   }

void
OMR::RelocationRecord::print(TR::RelocationRuntime *reloRuntime)
   {
   TR::RelocationTarget *reloTarget = reloRuntime->reloTarget();
   }

void
OMR::RelocationRecord::clean(TR::RelocationTarget *reloTarget)
   {
   setSize(reloTarget, 0);
   reloTarget->storeUnsigned8b(0, (uint8_t *) &_record->_type);
   reloTarget->storeUnsigned8b(0, (uint8_t *) &_record->_flags);
   }

int32_t
OMR::RelocationRecord::bytesInHeaderAndPayload()
   {
   return sizeof(OMR::RelocationRecordBinaryTemplate);
   }

OMR::RelocationRecordBinaryTemplate *
OMR::RelocationRecord::nextBinaryRecord(TR::RelocationTarget *reloTarget)
   {
   return (OMR::RelocationRecordBinaryTemplate*) (((uint8_t*)this->_record) + size(reloTarget));
   }

void
OMR::RelocationRecord::setSize(TR::RelocationTarget *reloTarget, uint16_t size)
   {
   reloTarget->storeUnsigned16b(size,(uint8_t *) &_record->_size);
   }

uint16_t
OMR::RelocationRecord::size(TR::RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &_record->_size);
   }


void
OMR::RelocationRecord::setType(TR::RelocationTarget *reloTarget, OMR::RelocationRecordType type)
   {
   reloTarget->storeUnsigned8b(type, (uint8_t *) &_record->_type);
   }

OMR::RelocationRecordType
OMR::RelocationRecord::type(TR::RelocationTarget *reloTarget)
   {
   return (OMR::RelocationRecordType)_record->type(reloTarget);
   }


void
OMR::RelocationRecord::setWideOffsets(TR::RelocationTarget *reloTarget)
   {
   setFlag(reloTarget, FLAGS_RELOCATION_WIDE_OFFSETS);
   }

bool
OMR::RelocationRecord::wideOffsets(TR::RelocationTarget *reloTarget)
   {
   return (flags(reloTarget) & FLAGS_RELOCATION_WIDE_OFFSETS) != 0;
   }

void
OMR::RelocationRecord::setEipRelative(TR::RelocationTarget *reloTarget)
   {
   setFlag(reloTarget, FLAGS_RELOCATION_EIP_OFFSET);
   }

bool
OMR::RelocationRecord::eipRelative(TR::RelocationTarget *reloTarget)
   {
   return (flags(reloTarget) & FLAGS_RELOCATION_EIP_OFFSET) != 0;
   }

void
OMR::RelocationRecord::setFlag(TR::RelocationTarget *reloTarget, uint8_t flag)
   {
   uint8_t flags = reloTarget->loadUnsigned8b((uint8_t *) &_record->_flags) | (flag & FLAGS_RELOCATION_FLAG_MASK);
   reloTarget->storeUnsigned8b(flags, (uint8_t *) &_record->_flags);
   }

uint8_t
OMR::RelocationRecord::flags(TR::RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned8b((uint8_t *) &_record->_flags) & FLAGS_RELOCATION_FLAG_MASK;
   }

void
OMR::RelocationRecord::setReloFlags(TR::RelocationTarget *reloTarget, uint8_t reloFlags)
   {
   TR_ASSERT((reloFlags & ~FLAGS_RELOCATION_FLAG_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
   uint8_t crossPlatFlags = flags(reloTarget);
   uint8_t flags = crossPlatFlags | (reloFlags & ~FLAGS_RELOCATION_FLAG_MASK);
   reloTarget->storeUnsigned8b(flags, (uint8_t *) &_record->_flags);
   }

uint8_t
OMR::RelocationRecord::reloFlags(TR::RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned8b((uint8_t *) &_record->_flags) & ~FLAGS_RELOCATION_FLAG_MASK;
   }

void
OMR::RelocationRecord::preparePrivateData(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget)
   {

   }
// Generic helper address computation for multiple relocation types
uint8_t *
OMR::RelocationRecord::computeHelperAddress(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *baseLocation)
   {
   uint8_t* helperAddress =  (uint8_t*) malloc(sizeof(uint8_t));
   *(helperAddress) = 0;
   return helperAddress;
   }

#undef FLAGS_RELOCATION_WIDE_OFFSETS
#undef FLAGS_RELOCATION_EIP_OFFSET
#undef FLAGS_RELOCATION_ORDERED_PAIR
#undef FLAGS_RELOCATION_TYPE_MASK
#undef FLAGS_RELOCATION_FLAG_MASK


bool
OMR::RelocationRecord::ignore(TR::RelocationRuntime *reloRuntime)
   {
   return false;
   }

int32_t
OMR::RelocationRecord::applyRelocationAtAllOffsets(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloOrigin)
   {
   if (ignore(reloRuntime))
      {
      RELO_LOG(reloRuntime->reloLogger(), 6, "\tignore!\n");
      return 0;
      }

   if (reloTarget->isOrderedPairRelocation(self(), reloTarget))
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
int32_t
OMR::RelocationRecordArbitrarySizedHeader::applyRelocation(TR::RelocationRuntime 
         *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
{
};
OMR::RelocationRecordArbitrarySizedHeader *
OMR::RelocationRecordArbitrarySizedHeader::self(){
    return static_cast<RelocationRecordArbitrarySizedHeader *>(this);
}

int32_t
OMR::RelocationRecordArbitrarySizedHeader::bytesInHeaderAndPayload(){
   OMR::RelocationRecordASHLBinaryTemplate* theRecord = 
   reinterpret_cast<OMR::RelocationRecordASHLBinaryTemplate *>( self()->_record);
   int32_t sizeOfTheHeader = ((uint8_t*) theRecord)[0];
   return sizeOfTheHeader;
}
void
OMR::RelocationRecordArbitrarySizedHeader::setSizeOfASHLHeader(
                                                TR::RelocationTarget* reloTarget,
                                                uint8_t size){
   RelocationRecordASHLBinaryTemplate* pointer = 
                        reinterpret_cast<RelocationRecordASHLBinaryTemplate*>
                             (_record);
  reloTarget->storeUnsigned8b(size,(uint8_t*) &pointer->sizeOfDataInTheHeader);
}
void
OMR::RelocationRecordArbitrarySizedHeader::fillThePayload(TR::RelocationTarget* reloTarget,
    uint8_t* source){
   RelocationRecordASHLBinaryTemplate* pointer = 
                        reinterpret_cast<RelocationRecordASHLBinaryTemplate*>
                             (_record);
  uint8_t* addressWithOffset = (uint8_t*) pointer
                                 +sizeof(RelocationRecordASHLBinaryTemplate);
  int i  = 0;
  for (i = 0 ; i < pointer->sizeOfDataInTheHeader; i++)
   reloTarget->storeUnsigned8b(source[i], addressWithOffset+i);
}
// Relocations with address sequences
//

uint8_t *
OMR::RelocationRecordMethodCallAddress::computeTargetMethodAddress(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *baseLocation)
   {
   uint8_t *callTargetAddress = address(reloTarget);
   char methodName[8]{};
   memcpy(methodName,&callTargetAddress,8);
   callTargetAddress = reinterpret_cast<uint8_t*>(reinterpret_cast<TR::SharedCacheRelocationRuntime *>(reloRuntime)->symbolAddress(methodName));
   return callTargetAddress;
   }
void 
OMR::RelocationRecordMethodCallAddress::preparePrivateData(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget){
   TR::RelocationRecordMethodCallPrivateData *reloPrivateData = &(privateData()->methodCall)  ;
   uint8_t *baseLocation = 0;
   uint8_t *callTargetAddress = computeTargetMethodAddress(reloRuntime, reloTarget, baseLocation);
   reloPrivateData->callTargetOffset = (callTargetAddress - baseLocation);
}

uint8_t*
OMR::RelocationRecordMethodCallAddress::address(TR::RelocationTarget *reloTarget)
   {
   RelocationRecordMethodCallAddressBinaryTemplate *reloData = (RelocationRecordMethodCallAddressBinaryTemplate *)_record;
   return reloTarget->loadAddress(reinterpret_cast<uint8_t *>(&reloData->_methodAddress));
   }

void
OMR::RelocationRecordMethodCallAddress::setAddress(TR::RelocationTarget *reloTarget, uint8_t *callTargetAddress)
   {
   RelocationRecordMethodCallAddressBinaryTemplate *reloData = (RelocationRecordMethodCallAddressBinaryTemplate *)_record;
   reloTarget->storeAddress(callTargetAddress, reinterpret_cast<uint8_t *>(&reloData->_methodAddress));
   }

int32_t OMR::RelocationRecordMethodCallAddress::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {      
      uint8_t* addressOfTheFunction =(uint8_t*)(reinterpret_cast<TR::RelocationRecordPrivateData*>(privateData())->methodCall.callTargetOffset);
      reloTarget->storeAddress( addressOfTheFunction, reloLocation);
      return 0;
   }

void 
OMR::RelocationRecordDataAddress::setOffset(TR::RelocationTarget *reloTarget, uintptr_t offset)
   {
   RelocationRecordDataAddressBinaryTemplate *reloRecord = reinterpret_cast<RelocationRecordDataAddressBinaryTemplate*>(_record);
   reloTarget->storeRelocationRecordValue(offset, (uintptrj_t *) &(reloRecord)->_offset);
   }

uintptrj_t
OMR::RelocationRecordDataAddress::offset(TR::RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *) &((RelocationRecordDataAddressBinaryTemplate *)_record)->_offset);
   }

int32_t 
OMR::RelocationRecordDataAddress::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR::SharedCacheRelocationRuntime *rr = reinterpret_cast<TR::SharedCacheRelocationRuntime*>(reloRuntime);
   std::string name = "gl_"+std::to_string(reinterpret_cast<TR::RelocationRecordDataAddressBinaryTemplate*>(_record)->_offset);
   reloTarget->storeAddress((uint8_t*)rr->symbolAddress(const_cast<char*>(name.c_str())), reloLocation);
   return 0;
   }

OMR::RelocationRecordDisplacementSite::RelocationRecordDisplacementSite(TR::RelocationRuntime *reloRuntime, TR::RelocationRecordBinaryTemplate *record): RelocationRecord(reloRuntime, record)
   {
   }

void 
OMR::RelocationRecordDisplacementSite::preparePrivateData(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget)
   {
   TR::RelocationRecordDisplacementSitePrivateData *reloPrivateData = &(privateData()->displacementSite);
   reloPrivateData->_assumptionID = reinterpret_cast<uint64_t>(offset(reloTarget));
   reloPrivateData->_displacementSite = new (trPersistentMemory) TR_DisplacementSite(reloPrivateData->_assumptionID);
   OMR::RuntimeAssumption **assumptions = new (trPersistentMemory) OMR::RuntimeAssumption*();
   TR::PatchDisplacementSiteUserTrigger::make(TR::FrontEnd::instance(), trPersistentMemory, reloPrivateData->_displacementSite->getAssumptionID(), 
					      reloPrivateData->_displacementSite->getSites(),   assumptions);
   }

int32_t
OMR::RelocationRecordDisplacementSite::applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
//   if(!_displacementSite)
//     {
     
//     }
   createAssumptions(reloRuntime,reloLocation);
   return 0;
   }

void
OMR::RelocationRecordDisplacementSite::createAssumptions(TR::RelocationRuntime *reloRuntime, uint8_t *reloLocation)
   {
   privateData()->displacementSite._displacementSite->addLocation(reloLocation);
   
   }

void 
OMR::RelocationRecordDisplacementSite::setOffset(TR::RelocationTarget *reloTarget, uintptr_t offset)
   {
   RelocationRecordDisplacementSiteBinaryTemplate *reloRecord = reinterpret_cast<RelocationRecordDisplacementSiteBinaryTemplate*>(_record);
   reloTarget->storeRelocationRecordValue(offset, (uintptrj_t *) &(reloRecord)->_offset);
   }

uintptrj_t
OMR::RelocationRecordDisplacementSite::offset(TR::RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *) &((RelocationRecordDisplacementSiteBinaryTemplate *)_record)->_offset);
   }

uint32_t OMR::RelocationRecord::_relocationRecordHeaderSizeTable[TR_NumExternalRelocationKinds] =
   {
      0 //yetToBeImplemented
     };

