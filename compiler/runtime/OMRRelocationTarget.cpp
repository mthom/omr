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


#include "codegen/FrontEnd.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/jittypes.h"
#include "runtime/RelocationRecord.hpp"
#include "runtime/RelocationRuntime.hpp"
#include "runtime/RelocationTarget.hpp"
bool TR::RelocationTarget::isOrderedPairRelocation(TR::RelocationRecord *reloRecord, TR::RelocationTarget *reloTarget)
   {
   switch (reloRecord->type(reloTarget))
      {
      case TR_AbsoluteMethodAddressOrderedPair :
      case TR_ConstantPoolOrderedPair :
         return true;
      default:
      	return false;
      }

   return false;
   }



uint8_t *
OMR::RelocationTarget::loadCallTarget(uint8_t *reloLocation)
   {
   return loadPointer(reloLocation);
   }

void
OMR::RelocationTarget::storeCallTarget(uintptr_t callTarget, uint8_t *reloLocation)
   {
   TR_ASSERT(0, "Error: storeCallTarget not implemented in relocation target base class");
   }

void
OMR::RelocationTarget:: storeRelativeTarget(uintptr_t callTarget, uint8_t *reloLocation)
   {
   TR_ASSERT(0, "Error: storeRelativeTarget not implemented in relocation target base class");
   }

uint8_t *
OMR::RelocationTarget::loadBranchOffset(uint8_t *reloLocation)
   {
   // reloLocation points at the start of the branch offset, so just need to dereference as uint8_t *
   return loadPointer(reloLocation);
   }

void
OMR::RelocationTarget::storeBranchOffset(uint8_t *branchOffset, uint8_t *reloLocation)
   {
   storePointer(branchOffset, reloLocation);
   }


uint8_t *
OMR::RelocationTarget::loadAddress(uint8_t *reloLocation)
   {
   return loadPointer(reloLocation);
   }

void
OMR::RelocationTarget::storeAddress(uint8_t *address, uint8_t *reloLocation)
   {
   // reloLocation points at the start of the address, so just store the uint8_t * at reloLocation
   storePointer(address, reloLocation);
   }

uint8_t *
OMR::RelocationTarget::loadAddressSequence(uint8_t *reloLocation)
   {
   TR_ASSERT(0, "Error: loadAddressSequence not implemented in relocation target base class");
   return NULL;
   }

void
OMR::RelocationTarget::storeAddressSequence(uint8_t *address, uint8_t *reloLocation, uint32_t seqNumber)
   {
   TR_ASSERT(0, "Error: storeAddressSequence not implemented in relocation target base class");
   }


uint8_t *
OMR::RelocationTarget::loadClassAddressForHeader(uint8_t *reloLocation)
   {
   // reloLocation points at the start of the address, so just need to dereference as uint8_t *
#ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
   return (uint8_t *) loadUnsigned32b(reloLocation);
#else
   return (uint8_t *) loadPointer(reloLocation);
#endif
   }

void
OMR::RelocationTarget::storeClassAddressForHeader(uint8_t *clazz, uint8_t *reloLocation)
   {
   // reloLocation points at the start of the address, so just store the uint8_t * at reloLocation
#ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
   uintptr_t clazzPtr = (uintptr_t)clazz;
   storeUnsigned32b((uint32_t)clazzPtr, reloLocation);
#else
   storePointer(clazz, reloLocation);
#endif
   }

uint32_t
OMR::RelocationTarget::loadCPIndex(uint8_t *reloLocation)
   {
   TR_ASSERT(0, "Error: loadCPIndex not implemented in relocation target base class");
   return 0;
   }



uint8_t *
OMR::RelocationTarget::eipBaseForCallOffset(uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   TR_ASSERT(0, "Error: eipBaseForCallOffset not implemented in relocation target base class");
   return NULL;
   }


uint8_t *
OMR::RelocationTarget::loadCallTarget(uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   TR_ASSERT(0, "Error: loadCallTarget not implemented in relocation target base class");
   return NULL;
   }

void
OMR::RelocationTarget::storeCallTarget(uint8_t *callTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   TR_ASSERT(0, "Error: storeCallTarget not implemented in relocation target base class");
   }


uint8_t *
OMR::RelocationTarget::loadBranchOffset(uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   TR_ASSERT(0, "Error: loadBranchOffset not implemented in relocation target base class");
   return NULL;
   }

void
OMR::RelocationTarget::storeBranchOffset(uint8_t *branchOffset, uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   TR_ASSERT(0, "Error: storeBranchOffset not implemented in relocation target base class");
   }


uint8_t *
OMR::RelocationTarget::loadAddress(uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   TR_ASSERT(0, "Error: loadAddress not implemented in relocation target base class");
   return NULL;
   }

void
OMR::RelocationTarget::storeAddress(uint8_t *address, uint8_t *reloLocationHigh, uint8_t *reloLocationLow, uint32_t seqNumber)
   {
   TR_ASSERT(0, "Error: storeAddress not implemented in relocation target base class");
   }


uint8_t *
OMR::RelocationTarget::loadClassAddressForHeader(uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   TR_ASSERT(0, "Error: loadClassAddressForHeader not implemented in relocation target base class");
   return NULL;
   }

void
OMR::RelocationTarget::storeClassAddressForHeader(uint8_t *address, uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   TR_ASSERT(0, "Error: storeClassAddressForHeader not implemented in relocation target base class");
   }

uint32_t
OMR::RelocationTarget::loadCPIndex(uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   TR_ASSERT(0, "Error: loadCPIndex not implemented in relocation target base class");
   return 0;
   }

void
OMR::RelocationTarget::performThunkRelocation(uint8_t *thunkAddress, uintptr_t vmHelper)
   {
   TR_ASSERT(0, "Error: performThunkRelocation not implemented in relocation target base class");
   }

