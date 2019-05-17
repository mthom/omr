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

//
// PLEASE DO NOT USE ANY J9-SPECIFIC DATA TYPES IN THIS FILE
//

#ifndef OMR_RELOCATION_RECORD_INCL
#define OMR_RELOCATION_RECORD_INCL
/*
 * The following #define(s) and typedef(s) must appear before any #includes in this file
 */
#ifndef OMR_RELOCATION_RECORD_CONNECTOR
#define OMR_RELOCATION_RECORD_CONNECTOR
namespace OMR { class RelocationRecord; }
namespace OMR { typedef OMR::RelocationRecord RelocationRecordConnector; }
#else
   #error OMR::RelocationRecord expected to be a primary connector, but another connector is already defined
#endif


#ifndef OMR_RELOCATION_RECORD_BINARY_TEMPLATE_CONNECTOR
#define OMR_RELOCATION_RECORD_BINARY_TEMPLATE_CONNECTOR
namespace OMR { class RelocationRecordBinaryTemplate; }
namespace OMR { typedef OMR::RelocationRecordBinaryTemplate RelocationRecordBinaryTemplateConnector; }
#else
   #error OMR::RelocationRecord expected to be a primary connector, but another connector is already defined
#endif


#ifndef OMR_RELOCATION_RECORD_GROUP_CONNECTOR
#define OMR_RELOCATION_RECORD_GROUP_CONNECTOR
namespace OMR { class RelocationRecordGroup; }
namespace OMR { typedef OMR::RelocationRecordGroup RelocationRecordGroupConnector; }
#else
   #error OMR::RelocationRecord expected to be a primary connector, but another connector is already defined
#endif



#include <stdint.h>
#include "compile/Compilation.hpp"
#include "env/jittypes.h"
#include "infra/Link.hpp"
#include "infra/Flags.hpp"
#include "runtime/RelocationRuntime.hpp"
#include "runtime/Runtime.hpp"

namespace TR {
class RelocationRuntime;
class RelocationTarget;
class RelocationRuntimeLogger;
union RelocationRecordPrivateData;
   union RelocationRecordPrivateData
   {
//       TR::RelocationRecordHelperAddressPrivateData helperAddress;
//       TR::RelocationRecordInlinedAllocationPrivateData inlinedAllocation;
//       TR::RelocationRecordInlinedMethodPrivateData inlinedMethod;
//       TR::RelocationRecordProfiledInlinedMethodPrivateData profiledInlinedMethod;
//       TR::RelocationRecordMethodTracingCheckPrivateData methodTracingCheck;
//       TR::RelocationRecordWithOffsetPrivateData offset;
//       TR::RelocationRecordArrayCopyPrivateData arraycopy;
//       TR::RelocationRecordPointerPrivateData pointer;
//       TR::RelocationRecordMethodCallPrivateData methodCall;
//       TR::RelocationRecordEmitClassPrivateData emitClass;
//       TR::RelocationRecordDebugCounterPrivateData debugCounter;
//       TR::RelocationSymbolFromManagerPrivateData symbolFromManager;
    int yetToImplement;
   };

}

namespace OMR { typedef TR_ExternalRelocationTargetKind RelocationRecordType;}
namespace TR { typedef TR_ExternalRelocationTargetKind RelocationRecordType;}
namespace TR { class RelocationRecordBinaryTemplate;}
// These *BinaryTemplate structs describe the shape of the binary relocation records.
extern char* AOTcgDiagOn;

// TR::RelocationRecord is the base class for all relocation records.  It is used for all queries on relocation
// records as well as holding all the "wrapper" parts.  These classes are an interface to the *BinaryTemplate
// classes which are simply structs that can be used to directly access the binary representation of the relocation
// records stored in the cache (*BinaryTemplate structs are defined near the end of this file after the
// RelocationRecord* classes.  The RelocationRecord* classes permit virtual function calls access to the
// *BinaryTemplate classes and must access the binary structs via the _record field in the TR::RelocationRecord
// class.  Most consumers should directly manipulate the TR::RelocationRecord* classes since they offer
// the most flexibility.
namespace OMR
{
   class OMR_EXTENSIBLE RelocationRecordBinaryTemplate
   {
      public:
         RelocationRecordBinaryTemplate(){};
         uint8_t type(TR::RelocationTarget *reloTarget);
         uint16_t _size;
         uint8_t _type;
         uint8_t _flags;
         #if defined(TR_HOST_64BIT)
         uint32_t _extra;
         #endif
   };

  struct RelocationRecordMethodCallAddressBinaryTemplate : public RelocationRecordBinaryTemplate
   {
   UDATA _methodAddress;
   };
   
   class  OMR_EXTENSIBLE  RelocationRecord
   {
   
   public:
      RelocationRecord() {}
      RelocationRecord(TR::RelocationRuntime *reloRuntime, TR::RelocationRecordBinaryTemplate *record)
         {
            _reloRuntime= reloRuntime;
             _record =record;
         }

      void * operator new (size_t s, RelocationRecord *p)   { return p; }
      TR::RelocationRecord* self();
      virtual void print(TR::RelocationRuntime *reloRuntime);
      virtual char *name() { return "RelocationRecord"; }

      virtual bool isValidationRecord() { return false; }


      static RelocationRecord *create(TR::RelocationRecord *storage, TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, TR::RelocationRecordBinaryTemplate *recordPointer);

      virtual void clean(TR::RelocationTarget *reloTarget);
      virtual int32_t bytesInHeaderAndPayload();

      virtual void preparePrivateData(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget);

      virtual int32_t applyRelocationAtAllOffsets(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *relocationOrigin);

      virtual int32_t applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation) {return -1;}
      virtual int32_t applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow) {return -1;}

      RelocationRecordBinaryTemplate *nextBinaryRecord(TR::RelocationTarget *reloTarget);
      RelocationRecordBinaryTemplate *binaryRecord();

      void setSize(TR::RelocationTarget *reloTarget, uint16_t size);
      uint16_t size(TR::RelocationTarget *reloTarget);

      void setType(TR::RelocationTarget *reloTarget, TR::RelocationRecordType type);
      RelocationRecordType type(TR::RelocationTarget *reloTarget);

      void setWideOffsets(TR::RelocationTarget *reloTarget);
      bool wideOffsets(TR::RelocationTarget *reloTarget);

      void setEipRelative(TR::RelocationTarget *reloTarget);
      bool eipRelative(TR::RelocationTarget *reloTarget);

      void setFlag(TR::RelocationTarget *reloTarget, uint8_t flag);
      uint8_t flags(TR::RelocationTarget *reloTarget);

      void setReloFlags(TR::RelocationTarget *reloTarget, uint8_t reloFlags);
      uint8_t reloFlags(TR::RelocationTarget *reloTarget);

      TR::RelocationRuntime *_reloRuntime;

      virtual bool ignore(TR::RelocationRuntime *reloRuntime);

      static uint32_t getSizeOfAOTRelocationHeader(TR_ExternalRelocationTargetKind k)
         {
         return _relocationRecordHeaderSizeTable[k];
         }

   protected:
      // OMR::RuntimeAssumption** getMetadataAssumptionList(OMRJITExceptionTable *exceptionTable)
      //    {
      //    return (OMR::RuntimeAssumption**)(&exceptionTable->runtimeAssumptionList);
      //    }
      uint8_t *computeHelperAddress(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *baseLocation);

      TR::RelocationRecordPrivateData *privateData()
         {
         return &_privateData;
         }

      TR::RelocationRecordBinaryTemplate *_record;

      TR::RelocationRecordPrivateData _privateData;

      static uint32_t _relocationRecordHeaderSizeTable[TR_NumExternalRelocationKinds];
   };


class RelocationRecordGroup
   {
   public:
      RelocationRecordGroup(TR::RelocationRecordBinaryTemplate *groupData) : _group(groupData) {};

      void setSize(TR::RelocationTarget *reloTarget, uintptr_t size);
      uintptr_t size(TR::RelocationTarget *reloTarget);

      TR::RelocationRecordBinaryTemplate *firstRecord(TR::RelocationTarget *reloTarget);
      TR::RelocationRecordBinaryTemplate *pastLastRecord(TR::RelocationTarget *reloTarget);

      int32_t applyRelocations(TR::RelocationRuntime *reloRuntime,
                               TR::RelocationTarget *reloTarget,
                               uint8_t *reloOrigin);
   private:
      int32_t handleRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, TR::RelocationRecord *reloRecord, uint8_t *reloOrigin);

      TR::RelocationRecordBinaryTemplate *_group;
   };

class RelocationRecordMethodCallAddress : public RelocationRecord
   {
   public:
      RelocationRecordMethodCallAddress() {}
      RelocationRecordMethodCallAddress(TR::RelocationRuntime *reloRuntime, TR::RelocationRecordBinaryTemplate *record) : RelocationRecord(reloRuntime, record) {}

      virtual char *name() { return "MethodCallAddress"; }
      virtual int32_t bytesInHeaderAndPayload() { return sizeof(RelocationRecordMethodCallAddressBinaryTemplate); }
      virtual int32_t applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation);

      uint8_t* address(TR::RelocationTarget *reloTarget);
      void setAddress(TR::RelocationTarget *reloTarget, uint8_t* callTargetAddress);

   private:
      uint8_t *computeTargetMethodAddress(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *baseLocation);
   };


} //namespace OMR
// No class that derives from TR::RelocationRecord should define any state: all state variables should be declared
//  in TR::RelocationRecord or the constructor/decode() mechanisms will not work properly


// Relocation record classes for "real" relocation record types
// should be defined on the level of consuming project, I guess????
#endif   // RELOCATION_RECORD_INCL

