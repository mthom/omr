/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#ifndef OMR_X86_RELOCATION_TARGET_INCL
#define OMR_X86_RELOCATION_TARGET_INCL

#ifndef OMR_RELOCATION_TARGET_CONNECTOR
#define OMR_RELOCATION_TARGET_CONNECTOR
namespace OMR { namespace X86 { class RelocationTarget; }}
namespace OMR { typedef OMR::X86::RelocationTarget RelocationTargetConnector; }
#endif

#include "compiler/runtime/OMRRelocationTarget.hpp"

#include <stddef.h>
#include <stdint.h>
#include "env/jittypes.h"
#include "runtime/RelocationRecord.hpp"




/* Mfence patching constants */

#define   VolCheckMask            0x00080000
#define   VolUpperLongCheckMask   0x00020000
#define   VolLowerLongCheckMask   0x00040000
#define   MFenceNOPPatch          0xff001f0f
#define   MFenceWordCheck         0xae0f
#define   LORNOPPatchDWord        0x00441f0f
#define   LORNOPPatchTrailingByte 0x00
#define   LCMPXCHGNOPPatchDWord   0x90666666
#define   LCMPXCHGCheckMaskWord   0x0ff0


// TR_RelocationTarget defines how a platform target implements the individual steps of processing
//    relocation records.
// This is intended to be a base class that should not be itself instantiated
namespace OMR
{
namespace X86
{
class OMR_EXTENSIBLE RelocationTarget  : public OMR::RelocationTarget
   {
   public:
      TR_ALLOC(TR_Memory::Relocation)
	void * operator new(size_t, TR::JitConfig *);
      RelocationTarget(TR::RelocationRuntime *reloRuntime) : OMR::RelocationTarget(reloRuntime) {}

      
       virtual void storeCallTarget(uintptr_t callTarget, uint8_t *reloLocation);
       virtual void storeRelativeTarget(uintptr_t callTarget, uint8_t *reloLocation);
       virtual uint8_t *loadAddressSequence(uint8_t *reloLocation);
       virtual void storeAddressSequence(uint8_t *address, uint8_t *reloLocation, uint32_t seqNumber);
       virtual uint8_t *eipBaseForCallOffset(uint8_t *reloLocationHigh);

       virtual void storeRelativeAddressSequence(uint8_t *address, uint8_t *reloLocation, uint32_t seqNumber) 
         {
         address = (uint8_t *)((intptrj_t)address - (intptrj_t)(reloLocation + 4));
         storeAddressSequence(address, reloLocation, seqNumber);
         }
      
//     virtual bool isOrderedPairRelocation(TR::RelocationRecord *reloRecord, TR::RelocationTarget *reloTarget);

  
//     virtual void patchMTIsolatedOffset(uint32_t offset, uint8_t *reloLocation);
   };


}
}
#endif   // OMR_X86RELOCATION_TARGET_INCL
