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

#ifndef OMR_AMD64_RELOCATION_TARGET_INCL
#define OMR_AMD64_RELOCATION_TARGET_INCL

#ifndef OMR_RELOCATION_TARGET_CONNECTOR
#define OMR_RELOCATION_TARGET_CONNECTOR
namespace OMR { namespace X86 {  namespace AMD64 { class RelocationTarget; }}}
namespace OMR { typedef OMR::X86::AMD64::RelocationTarget RelocationTargetConnector; }
#else
#error OMR::X86::AMD64::RelocationTarget expected to be a primary connector, but a OMR connector is already defined
#endif


#include <stddef.h>
#include <stdint.h>
#include "env/jittypes.h"
namespace TR {class RelocationRecord;}


#include "compiler/x/runtime/OMRRelocationTarget.hpp"
namespace OMR
{
namespace X86
{
namespace AMD64
{
class OMR_EXTENSIBLE RelocationTarget : public OMR::X86::RelocationTarget
   {
   public:
      TR_ALLOC(TR_Memory::Relocation)
      void * operator new(size_t, TR::JitConfig *);
      RelocationTarget(TR::RelocationRuntime *reloRuntime) : OMR::X86::RelocationTarget(reloRuntime) {}
      virtual bool isOrderedPairRelocation(TR::RelocationRecord *reloRecord, TR::RelocationTarget *reloTarget);

 
      virtual void storeRelativeAddressSequence(uint8_t *address, uint8_t *reloLocation, uint32_t seqNumber) 
         {
         storeAddressSequence(address, reloLocation, seqNumber);
         }
        virtual uint8_t *eipBaseForCallOffset(uint8_t *reloLocationHigh);

      virtual bool useTrampoline(uint8_t * helperAddress, uint8_t *baseLocation);
   };
}
}
}

#endif//OMR_AMD64_RELOCATION_TARGET