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

#include "x/codegen/HelperCallSnippet.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/FrontEnd.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/Machine.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterDependencyStruct.hpp"
#include "codegen/Snippet.hpp"
#include "codegen/SnippetGCMap.hpp"
#include "compile/Compilation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/jittypes.h"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "infra/Assert.hpp"
#include "ras/Debug.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/Runtime.hpp"
#include "x/codegen/RestartSnippet.hpp"
#include "env/IO.hpp"
#include "env/CompilerEnv.hpp"

TR::X86HelperCallSnippet::X86HelperCallSnippet(TR::CodeGenerator   *cg,
                                                 TR::Node            *node,
                                                 TR::LabelSymbol      *restartlab,
                                                 TR::LabelSymbol      *snippetlab,
                                                 TR::SymbolReference *helper,
                                                 int32_t             stackPointerAdjustment)
   : TR::X86RestartSnippet(cg, node, restartlab, snippetlab, helper->canCauseGC()),
     _destination(helper),
     _callNode(0),
     _offset(-1),
     _callInstructionBufferAddress(NULL),
     _stackPointerAdjustment(stackPointerAdjustment),
     _alignCallDisplacementForPatching(false)
   {
   // The jitReportMethodEnter helper requires special handling; the first
   // child of the call node is the receiver object of the current (owning)
   // method, which is not a constant value. We pass the receiver object to
   // the helper by pushing the first argument to the current method.
   TR::Compilation* comp = cg->comp();
   TR::SymbolReferenceTable *symRefTab = comp->getSymRefTab();
   TR::ResolvedMethodSymbol    *owningMethod = comp->getJittedMethodSymbol();

   if (getDestination() == symRefTab->findOrCreateReportMethodEnterSymbolRef(owningMethod))
      {
      _offset = ((int32_t) owningMethod->getNumParameterSlots() * 4);
      }
   }

TR::X86HelperCallSnippet::X86HelperCallSnippet(TR::CodeGenerator *cg,
                                                 TR::LabelSymbol    *restartlab,
                                                 TR::LabelSymbol    *snippetlab,
                                                 TR::Node          *callNode,
                                                 int32_t           stackPointerAdjustment)
   : TR::X86RestartSnippet(cg, callNode, restartlab, snippetlab, callNode->getSymbolReference()->canCauseGC()),
     _destination(callNode->getSymbolReference()),
     _callNode(callNode),
     _offset(-1),
     _callInstructionBufferAddress(NULL),
     _stackPointerAdjustment(stackPointerAdjustment),
     _alignCallDisplacementForPatching(false)
   {
   TR::Compilation* comp = cg->comp();
   TR::SymbolReferenceTable *symRefTab    = comp->getSymRefTab();
   TR::ResolvedMethodSymbol    *owningMethod = comp->getJittedMethodSymbol();

   if (getDestination() == symRefTab->findOrCreateReportMethodEnterSymbolRef(owningMethod))
      {
      _offset = ((int32_t) owningMethod->getNumParameterSlots() * 4);
      }
   }

uint8_t *TR::X86HelperCallSnippet::emitSnippetBody()
   {
   uint8_t *buffer = cg()->getBinaryBufferCursor();
   getSnippetLabel()->setCodeLocation(buffer);

   uint8_t * grm = genRestartJump(genHelperCall(buffer));

   return grm;
   }


void
TR::X86HelperCallSnippet::addMetaDataForLoadAddrArg(
      uint8_t *buffer,
      TR::Node *child)
   {
   TR::StaticSymbol *sym = child->getSymbol()->getStaticSymbol();

   if (cg()->comp()->getOption(TR_EnableHCR)
       && (!child->getSymbol()->isClassObject()
           || cg()->wantToPatchClassPointer((TR_OpaqueClassBlock*)sym->getStaticAddress(), buffer)))
      {
      if (TR::Compiler->target.is64Bit())
         cg()->jitAddPicToPatchOnClassRedefinition(((void *) (uintptrj_t)sym->getStaticAddress()), (void *) buffer);
      else
         cg()->jitAdd32BitPicToPatchOnClassRedefinition(((void *) (uintptrj_t)sym->getStaticAddress()), (void *) buffer);
      }

   }


uint8_t *TR::X86HelperCallSnippet::genHelperCall(uint8_t *buffer)
   {
   // add esp, _stackPointerAdjustment
   //
   if (_stackPointerAdjustment < -128 || _stackPointerAdjustment > 127)
      {
      if (TR::Compiler->target.is64Bit())
         {
         *buffer++ = 0x48; // Rex
         }
      *buffer++ = 0x81;
      *buffer++ = 0xc4;
      *(int32_t*)buffer = _stackPointerAdjustment;
      buffer += 4;
      }
   else if (_stackPointerAdjustment != 0)
      {
      if (TR::Compiler->target.is64Bit())
         {
         *buffer++ = 0x48; // Rex
         }
      *buffer++ = 0x83;
      *buffer++ = 0xc4;
      *buffer++ = (uint8_t)_stackPointerAdjustment;
      }

   if (_callNode)
      {
      if(!debug("amd64unimplemented"))
         TR_ASSERT(TR::Compiler->target.is32Bit(), "AMD64 genHelperCall with _callNode not yet implemented");
      int32_t i = 0;

      if (_offset != -1)
         {
         // push [vfp +N]  ; N is the offset from vfp to arg 0
         //
         *buffer++ = 0xFF;
         if (cg()->getLinkage()->getProperties().getAlwaysDedicateFramePointerRegister())
            {
            if (_offset > -128 && _offset <= 127)
               {
               *buffer++ = 0x73;
               *(int8_t *)buffer++ = _offset;
               }
            else
               {
               *buffer++ = 0xB3;
               *(int32_t *)buffer = _offset;
               buffer += 4;
               }
            }
         else
            {
            _offset += cg()->getFrameSizeInBytes();
            if (_offset > -128 && _offset <= 127)
               {
               *buffer++ = 0x74;
               *buffer++ = 0x24;
               *(int8_t *)buffer++ = _offset;
               }
            else
               {
               *buffer++ = 0xB4;
               *buffer++ = 0x24;
               *(int32_t *)buffer = _offset;
               buffer += 4;
               }
            }
         i = 1; // skip the first child
         }

      TR::RegisterDependencyConditions  *deps = getRestartLabel()->getInstruction()->getDependencyConditions();
      int32_t registerArgs = 0;

      for ( ; i < _callNode->getNumChildren(); ++i)
         {
         TR::Node *child = _callNode->getChild(i);

         if (child->getOpCodeValue() == TR::loadaddr)
            {
            if (!child->getRegister() ||
                child->getRegister() != deps->getPostConditions()->getRegisterDependency(registerArgs)->getRegister())
               {
               TR::StaticSymbol *sym = child->getSymbol()->getStaticSymbol();
               TR_ASSERT(sym, "Bad argument to helper call");
               *buffer++ = 0x68; // push   imm4   argValue
               *(uint32_t *)buffer = (uint32_t)(uintptrj_t)sym->getStaticAddress();

               addMetaDataForLoadAddrArg(buffer, child);

               buffer += 4;
               continue;
               }
            }
         else if (child->getOpCode().isLoadConst())
            {
            int32_t argValue = child->getInt();
            if (argValue >= -128 && argValue <= 127)
               {
               *buffer++ = 0x6a; // push   imms   argValue
               *(int8_t *)buffer = argValue;
               buffer += 1;
               }
            else
               {
               *buffer++ = 0x68; // push   imm4   argValue
               *(int32_t *)buffer = argValue;
               buffer += 4;
               }
            continue;
            }

         // Find out the register from the dependency list on the restart
         // label instruction
         //
         TR_ASSERT(child->getRegister(), "Bad argument to helper call");

         *buffer = 0x50;
         TR_ASSERT(deps, "null dependencies on restart label of helper call snippet with register args");
         cg()->machine()->getRealRegister(deps->getPostConditions()->getRegisterDependency(registerArgs++)->getRealRegister())->setRegisterFieldInOpcode(buffer++);
         }
      }


   // Insert alignment padding if the instruction might be patched dynamically.
   //
   if (_alignCallDisplacementForPatching && TR::Compiler->target.isSMP())
      {
      uintptrj_t mod = (uintptrj_t)(buffer) % cg()->getInstructionPatchAlignmentBoundary();
      mod = cg()->getInstructionPatchAlignmentBoundary() - mod;

      if (mod <= 4)
         {
         // Perhaps use a multi-byte NOP here...
         //
         while (mod--)
            {
            *buffer++ = 0x90;
            }
         }
      }

   _callInstructionBufferAddress = buffer;

   uint8_t *callInstructionAddress = buffer;
   *buffer++ = 0xe8; // CallImm4
   *(int32_t *)buffer = branchDisplacementToHelper(callInstructionAddress, getDestination(), cg());

   cg()->addProjectSpecializedRelocation(buffer,(uint8_t *)getDestination(), NULL, TR_HelperAddress, __FILE__, __LINE__, _callNode);

   buffer += 4;

   gcMap().registerStackMap(buffer, cg());

   // sub esp, _stackPointerAdjustment
   //
   if (_stackPointerAdjustment < -128 || _stackPointerAdjustment > 127)
      {
      if (TR::Compiler->target.is64Bit())
         {
         *buffer++ = 0x48; // Rex
         }
      *buffer++ = 0x81;
      *buffer++ = 0xec;
      *(int32_t*)buffer = _stackPointerAdjustment;
      buffer += 4;
      }
   else if (_stackPointerAdjustment != 0)
      {
      if (TR::Compiler->target.is64Bit())
         {
         *buffer++ = 0x48; // Rex
         }
      *buffer++ = 0x83;
      *buffer++ = 0xec;
      *buffer++ = (uint8_t)_stackPointerAdjustment;
      }

   return buffer;
   }


void
TR_Debug::print(TR::FILE *pOutFile, TR::X86HelperCallSnippet  * snippet)
   {
   if (pOutFile == NULL)
      return;
   uint8_t *bufferPos = snippet->getSnippetLabel()->getCodeLocation();
   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, getName(snippet), getName(snippet->getDestination()));
   printBody(pOutFile, snippet, bufferPos);
   }

void
TR_Debug::printBody(TR::FILE *pOutFile, TR::X86HelperCallSnippet  * snippet, uint8_t *bufferPos)
   {
   TR_ASSERT(pOutFile != NULL, "assertion failure");
   TR::MethodSymbol *sym = snippet->getDestination()->getSymbol()->castToMethodSymbol();

   int32_t i = 0;

   if (snippet->getStackPointerAdjustment() != 0)
      {
      uint8_t size = 5 + (TR::Compiler->target.is64Bit()? 1 : 0);
      printPrefix(pOutFile, NULL, bufferPos, size);
      trfprintf(pOutFile, "add \t%s, %d\t\t\t%s Temporarily deallocate stack frame", TR::Compiler->target.is64Bit()? "rsp":"esp", snippet->getStackPointerAdjustment(),
                    commentString());
      bufferPos += size;
      }

   if (snippet->getCallNode())
      {
      if (snippet->getOffset() != -1)
         {
         uint32_t pushLength;

         bool useDedicatedFrameReg = _comp->cg()->getLinkage()->getProperties().getAlwaysDedicateFramePointerRegister();
         if (snippet->getOffset() >= -128 && snippet->getOffset() <= 127)
            pushLength = (useDedicatedFrameReg ? 3 : 4);
         else
            pushLength = (useDedicatedFrameReg ? 6 : 7);

         printPrefix(pOutFile, NULL, bufferPos, pushLength);
         trfprintf(pOutFile,
                       "push\t[%s +%d]\t%s Address of Receiver",
                       useDedicatedFrameReg ? "ebx" : "esp",
                       snippet->getOffset(),
                       commentString());

         bufferPos += pushLength;
         i = 1; // skip the first child
         }

      TR::RegisterDependencyConditions  *deps = snippet->getRestartLabel()->getInstruction()->getDependencyConditions();
      int32_t registerArgs = 0;
      for ( ; i < snippet->getCallNode()->getNumChildren(); i++)
         {
         TR::Node *child = snippet->getCallNode()->getChild(i);
         if (child->getOpCodeValue() == TR::loadaddr && !child->getRegister())
            {
            TR::StaticSymbol *sym = child->getSymbol()->getStaticSymbol();
            TR_ASSERT( sym, "Bad argument to helper call");
            printPrefix(pOutFile, NULL, bufferPos, 5);
            trfprintf(pOutFile, "push\t" POINTER_PRINTF_FORMAT, sym->getStaticAddress());
            bufferPos += 5;
            }
         else if (child->getOpCode().isLoadConst())
            {
            int32_t argValue = child->getInt();
            int32_t size = (argValue >= -128 && argValue <= 127) ? 2 : 5;
            printPrefix(pOutFile, NULL, bufferPos, size);
            trfprintf(pOutFile, "push\t" POINTER_PRINTF_FORMAT, argValue);
            bufferPos += size;
            }
         else
            {
            TR_ASSERT( child->getRegister(), "Bad argument to helper call");

            printPrefix(pOutFile, NULL, bufferPos, 1);
            trfprintf(pOutFile, "push\t");
            TR_ASSERT( deps, "null dependencies on restart label of helper call snippet with register args");
            print(pOutFile, _cg->machine()->getRealRegister(deps->getPostConditions()->getRegisterDependency(registerArgs++)->getRealRegister()), TR_WordReg);
            bufferPos++;
            }
         }
      }

   printPrefix(pOutFile, NULL, bufferPos, 5);
   trfprintf(pOutFile, "call\t%s \t%s Helper Address = " POINTER_PRINTF_FORMAT,
                 getName(snippet->getDestination()),
                 commentString(),
                 sym->getMethodAddress());
   bufferPos += 5;

   if (snippet->getStackPointerAdjustment() != 0)
      {
      uint8_t size = 5 + (TR::Compiler->target.is64Bit()? 1 : 0);
      printPrefix(pOutFile, NULL, bufferPos, size);
      trfprintf(pOutFile, "sub \t%s, %d\t\t\t%s Reallocate stack frame", TR::Compiler->target.is64Bit()? "rsp":"esp", snippet->getStackPointerAdjustment(),
                    commentString());
      bufferPos += size;
      }

   printRestartJump(pOutFile, snippet, bufferPos);
   }


uint32_t TR::X86HelperCallSnippet::getLength(int32_t estimatedSnippetStart)
   {
   uint32_t length = 35;

   if (_callNode)
      {
      int32_t i = 0;

      if (_offset != -1)
         {
         bool useDedicatedFrameReg = cg()->getLinkage()->getProperties().getAlwaysDedicateFramePointerRegister();
         if (_offset >= -128 && _offset <= 127)
            length += (useDedicatedFrameReg ? 3 : 4);
         else
            length += (useDedicatedFrameReg ? 6 : 7);

         i = 1; // skip the first child
         }

      TR::RegisterDependencyConditions  *deps = getRestartLabel()->getInstruction()->getDependencyConditions();
      int32_t registerArgs = 0;

      for ( ; i < _callNode->getNumChildren(); ++i)
         {
         TR::Node *child = _callNode->getChild(i);
         if (child->getOpCodeValue() == TR::loadaddr &&
            (!child->getRegister() ||
                child->getRegister() != deps->getPostConditions()->getRegisterDependency(registerArgs++)->getRegister()))
            {
            length += 5;
            }
         else if (child->getOpCode().isLoadConst())
            {
            int32_t argValue = child->getInt();
            if (argValue >= -128 && argValue <= 127)
               length += 2;
            else
               length += 5;
            }
         else
            {
            length += 1;
            }
         }
      }

   // Conservatively assume that 4 NOPs might be required for alignment.
   //
   if (_alignCallDisplacementForPatching && TR::Compiler->target.isSMP())
      {
      length += 4;
      }

   return length + estimateRestartJumpLength(estimatedSnippetStart + length);
   }


int32_t TR::X86HelperCallSnippet::branchDisplacementToHelper(
   uint8_t            *callInstructionAddress,
   TR::SymbolReference *helper,
   TR::CodeGenerator   *cg)
   {
   intptrj_t helperAddress = (intptrj_t)helper->getMethodAddress();
   intptrj_t nextInstructionAddress = (intptrj_t)(callInstructionAddress + 5);

   if (cg->directCallRequiresTrampoline(helperAddress, (intptrj_t)callInstructionAddress))
      {
      helperAddress = TR::CodeCacheManager::instance()->findHelperTrampoline(helper->getReferenceNumber(), (void *)(callInstructionAddress+1));

      TR_ASSERT_FATAL(TR::Compiler->target.cpu.isTargetWithinRIPRange(helperAddress, nextInstructionAddress),
                      "Local helper trampoline should be reachable directly");
      }

   return (int32_t)(helperAddress - nextInstructionAddress);
   }

bool TR::X86PicDataSnippet::shouldEmitJ2IThunkPointer()
   {
     /*
   if (!TR::Compiler->target.is64Bit())
      return false; // no j2i thunks on 32-bit

   if (!isInterface())
      return unresolvedDispatch(); // invokevirtual could be private

   // invokeinterface
   if (forceUnresolvedDispatch())
      return true; // forced to assume it could be private/Object

   // Since interface method symrefs are always unresolved, check to see
   // whether we know that it's a normal interface call. If we don't, then
   // it could be private/Object.
   uintptrj_t itableIndex = (uintptrj_t)-1;
   int32_t cpIndex = _methodSymRef->getCPIndex();
   TR_ResolvedMethod *owningMethod = _methodSymRef->getOwningMethod(comp());
   TR_OpaqueClassBlock *interfaceClass =
      owningMethod->getResolvedInterfaceMethod(cpIndex, &itableIndex);
   return interfaceClass == NULL;
     */
   return true;
   }

uint8_t *TR::X86PicDataSnippet::encodeConstantPoolInfo(uint8_t *cursor)
   {
   uintptrj_t cpAddr = (uintptrj_t)_methodSymRef->getOwningMethod(cg()->comp())->constantPool();
   *(uintptrj_t *)cursor = cpAddr;

   uintptr_t inlinedSiteIndex = (uintptr_t)-1;
   if (_startOfPicInstruction->getNode() != NULL)
      inlinedSiteIndex = _startOfPicInstruction->getNode()->getInlinedSiteIndex();

   if (_hasJ2IThunkInPicData)
      {
      TR_ASSERT(
         TR::Compiler->target.is64Bit(),
         "expecting a 64-bit target for thunk relocations");

      auto info =
         (TR_RelocationRecordInformation *)comp()->trMemory()->allocateMemory(
            sizeof (TR_RelocationRecordInformation),
            heapAlloc);

      int offsetToJ2IVirtualThunk = isInterface() ? 0x22 : 0x18;

      info->data1 = cpAddr;
      info->data2 = inlinedSiteIndex;
      info->data3 = offsetToJ2IVirtualThunk;

      cg()->addExternalRelocation(
         new (cg()->trHeapMemory()) TR::ExternalRelocation(
            cursor,
            (uint8_t *)info,
            NULL,
            TR_J2IVirtualThunkPointer,
            cg()),
         __FILE__,
         __LINE__,
         _startOfPicInstruction->getNode());
      }
   else if (_thunkAddress)
      {
      TR_ASSERT(TR::Compiler->target.is64Bit(), "expecting a 64-bit target for thunk relocations");
      cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                                             *(uint8_t **)cursor,
                                                                             (uint8_t *)inlinedSiteIndex,
                                                                             TR_Thunks, cg()),
                             __FILE__,
                             __LINE__,
                             _startOfPicInstruction->getNode());
      }
   else
      {
      cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                                                  (uint8_t *)cpAddr,
                                                                                   (uint8_t *)inlinedSiteIndex,
                                                                                   TR_ConstantPool,
                                                                                   cg()),
                             __FILE__,
                             __LINE__,
                             _startOfPicInstruction->getNode());
      }

   // DD/DQ cpIndex
   //
   cursor += sizeof(uintptrj_t);
   *(uintptrj_t *)cursor = (uintptrj_t)_methodSymRef->getCPIndexForVM();
   cursor += sizeof(uintptrj_t);

   return cursor;
   }

uint8_t *TR::X86PicDataSnippet::encodeJ2IThunkPointer(uint8_t *cursor)
   {
   TR_ASSERT_FATAL(_hasJ2IThunkInPicData, "did not expect j2i thunk pointer");
   TR_ASSERT_FATAL(_thunkAddress != NULL, "null virtual j2i thunk");

   // DD/DQ j2iThunk
   *(uintptrj_t *)cursor = (uintptrj_t)_thunkAddress;
   cursor += sizeof(uintptrj_t);

   return cursor;
   }

uint8_t *TR::X86PicDataSnippet::emitSnippetBody()
   {
   uint8_t *startOfSnippet = cg()->getBinaryBufferCursor();

   uint8_t *cursor = startOfSnippet;

   TR::X86SystemLinkage *x86Linkage = toX86PrivateLinkage(cg()->getLinkage());

   int32_t disp32;

   TR_RuntimeHelper resolveSlotHelper, populateSlotHelper;
   int32_t sizeofPicSlot;
   /*
   if (isInterface())
      {
      // IPIC
      //
      // Slow interface lookup dispatch.
      //

      // Align the IPIC data to a pointer-sized boundary to ensure that the
      // interface class and itable offset are naturally aligned.
      uintptr_t offsetToIpicData = 10;
      uintptr_t unalignedIpicDataStart = (uintptr_t)cursor + offsetToIpicData;
      uintptr_t alignMask = sizeof (uintptrj_t) - 1;
      uintptr_t alignedIpicDataStart =
         (unalignedIpicDataStart + alignMask) & ~alignMask;
      cursor += alignedIpicDataStart - unalignedIpicDataStart;

      getSnippetLabel()->setCodeLocation(cursor);

      // Slow path lookup dispatch
      //
      _dispatchSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_X86IPicLookupDispatch, false, false, false);

      *cursor++ = 0xe8;  // CALL
      disp32 = cg()->branchDisplacementToHelperOrTrampoline(cursor+4, _dispatchSymRef);
      *(int32_t *)cursor = disp32;

      cg()->addExternalRelocation(new (cg()->trHeapMemory())
         TR::ExternalRelocation(cursor,
                                    (uint8_t *)_dispatchSymRef,
                                    TR_HelperAddress,
                                    cg()), __FILE__, __LINE__, _startOfPicInstruction->getNode());
      cursor += 4;

      // Lookup dispatch needs its stack map here.
      //
      gcMap().registerStackMap(cursor, cg());

      // Restart jump (always long for predictable size).
      //
      disp32 = _doneLabel->getCodeLocation() - (cursor + 5);
      *cursor++ = 0xe9;
      *(int32_t *)cursor = disp32;
      cursor += 4;

      // DD/DQ constantPool address
      // DD/DQ cpIndex
      //
      if (unresolvedDispatch())
         {
         cursor = encodeConstantPoolInfo(cursor);
         }
      else
         {
         TR_ASSERT_FATAL(0, "Can't handle resolved IPICs here yet!");
         }

      // Because the interface class and itable offset (immediately following)
      // are written at runtime and might be read concurrently by another
      // thread, they must be naturally aligned to guarantee that all accesses
      // to them are atomic.
      TR_ASSERT_FATAL(
         ((uintptr_t)cursor & (sizeof(uintptrj_t) - 1)) == 0,
         "interface class and itable offset IPIC data slots are unaligned");

      // Reserve space for resolved interface class and itable offset.
      // These slots will be populated during interface class resolution.
      // The itable offset slot doubles as a direct J9Method pointer slot.
      //
      // DD/DQ  0x00000000
      // DD/DQ  0x00000000
      //
      *(uintptrj_t*)cursor = 0;
      cursor += sizeof(uintptrj_t);
      *(uintptrj_t*)cursor = 0;
      cursor += sizeof(uintptrj_t);

      if (TR::Compiler->target.is64Bit())
         {
         // REX+MOV of MOVRegImm64 instruction
         //
         uint16_t *slotPatchInstructionBytes = (uint16_t *)_slotPatchInstruction->getBinaryEncoding();
         *(uint16_t *)cursor = *slotPatchInstructionBytes;
         cursor += 2;

         if (unresolvedDispatch() && _hasJ2IThunkInPicData)
            cursor = encodeJ2IThunkPointer(cursor);
         }
      else
      {
         // ModRM byte of CMPMemImm4 instruction
         //
      uint8_t *slotPatchInstructionBytes = _slotPatchInstruction->getBinaryEncoding();
      *cursor = *(slotPatchInstructionBytes+1);
      cursor++;
      }

      resolveSlotHelper = TR_X86resolveIPicClass;
      populateSlotHelper = TR_X86populateIPicSlotClass;
      sizeofPicSlot = x86Linkage->IPicParameters.roundedSizeOfSlot;
      }
   else
      {*/
      // VPIC
      //
      // Slow path dispatch through vtable
      //

      uint8_t callModRMByte = 0;

      // DD/DQ constantPool address
      // DD/DQ cpIndex
      //
      if (unresolvedDispatch())
         {
         // Align the real snippet entry point because it will be patched with
         // the vtable dispatch when the method is resolved.
         //
         intptrj_t entryPoint = ((intptrj_t)cursor +
                                 ((3 * sizeof(uintptrj_t)) +
                                  (hasJ2IThunkInPicData() ? sizeof(uintptrj_t) : 0) +
                                  (TR::Compiler->target.is64Bit() ? 4 : 1)));

         intptrj_t requiredEntryPoint =
            (entryPoint + (cg()->getLowestCommonCodePatchingAlignmentBoundary()-1) &
            (intptrj_t)(~(cg()->getLowestCommonCodePatchingAlignmentBoundary()-1)));

         cursor += (requiredEntryPoint - entryPoint);

         // Put the narrow integers before the pointer-sized ones. This way,
         // directMethod (which is mutable) will be aligned simply as a
         // consequence of the alignment required for patching the code that
         // immediately follows the VPIC data.
         if (TR::Compiler->target.is64Bit())
            {
            // REX prefix of MOVRegImm64 instruction
            //
            uint8_t *slotPatchInstructionBytes = (uint8_t *)_slotPatchInstruction->getBinaryEncoding();
            *cursor++ = *slotPatchInstructionBytes++;

            // MOV op of MOVRegImm64 instruction
            //
            *cursor++ = *slotPatchInstructionBytes;

            // REX prefix for the CALLMem instruction.
            //
            *cursor++ = *(slotPatchInstructionBytes+9);

            // Convert the CMP ModRM byte into the ModRM byte for the CALLMem instruction.
            //
            slotPatchInstructionBytes += 11;
            callModRMByte = (*slotPatchInstructionBytes & 7) + 0x90;
            *cursor++ = callModRMByte;
            }
         else
            {
            // CMP ModRM byte
            //
            uint8_t *slotPatchInstructionBytes = (uint8_t *)_slotPatchInstruction->getBinaryEncoding();
            *cursor++ = *(slotPatchInstructionBytes+1);
            }

         // DD/DQ cpAddr
         // DD/DQ cpIndex
         //
         cursor = encodeConstantPoolInfo(cursor);

         // Because directMethod (immediately following) is written at runtime
         // and might be read concurrently by another thread, it must be
         // naturally aligned to ensure that all accesses to it are atomic.
         TR_ASSERT_FATAL(
            ((uintptr_t)cursor & (sizeof(uintptrj_t) - 1)) == 0,
            "directMethod VPIC data slot is unaligned");

         // DD/DQ directMethod (initially null)
         *(uintptrj_t *)cursor = 0;
         cursor += sizeof(uintptrj_t);

         if (TR::Compiler->target.is64Bit())
            {
            // DD/DQ j2iThunk
            cursor = encodeJ2IThunkPointer(cursor);
            }
         }
      else
         {
         TR_ASSERT_FATAL(0, "Can't handle resolved VPICs here yet!");
         }

      _dispatchSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_X86populateVPicVTableDispatch, false, false, false);

      getSnippetLabel()->setCodeLocation(cursor);

      if (!isInterface() && _methodSymRef->isUnresolved())
         {
         TR_ASSERT((((intptrj_t)cursor & (cg()->getLowestCommonCodePatchingAlignmentBoundary()-1)) == 0),
                 "Mis-aligned VPIC snippet");
         }

      *cursor++ = 0xe8;  // CALL
      disp32 = cg()->branchDisplacementToHelperOrTrampoline(cursor+4, _dispatchSymRef);
      *(int32_t *)cursor = disp32;

      cg()->addExternalRelocation(new (cg()->trHeapMemory())
         TR::ExternalRelocation(cursor,
				(uint8_t *)_dispatchSymRef,
				TR_HelperAddress,
				cg()), __FILE__, __LINE__, _startOfPicInstruction->getNode());
      cursor += 4;

      // Populate vtable dispatch needs its stack map here.
      //
      gcMap().registerStackMap(cursor, cg());

      // Add padding after the call to snippet to hold the eventual indirect call instruction.
      //
      if (TR::Compiler->target.is64Bit())
         {
         *(uint16_t *)cursor = 0;
         cursor += 2;

         if (callModRMByte == 0x94)
            {
            // SIB byte required for CMP
            //
            *(uint8_t *)cursor = 0;
            cursor++;
            }
         }
      else
         {
         *(uint8_t *)cursor = 0;
         cursor++;
         }

      // Restart jump (always long for predictable size).
      //
      // TODO: no longer the case since data moved before call.
      //
      disp32 = _doneLabel->getCodeLocation() - (cursor + 5);
      *cursor++ = 0xe9;
      *(int32_t *)cursor = disp32;
      cursor += 4;

      resolveSlotHelper = TR_X86resolveVPicClass;
      populateSlotHelper = TR_X86populateVPicSlotClass;
      sizeofPicSlot = x86Linkage->VPicParameters.roundedSizeOfSlot;
      //}

   if (_numberOfSlots >= 1)
      {
      // Patch each Pic slot to route through the population helper
      //
      int32_t numPicSlots = _numberOfSlots;
      uint8_t *picSlotCursor = _startOfPicInstruction->getBinaryEncoding();

      TR::SymbolReference *resolveSlotHelperSymRef =
         cg()->symRefTab()->findOrCreateRuntimeHelper(resolveSlotHelper, false, false, false);
      TR::SymbolReference *populateSlotHelperSymRef =
         cg()->symRefTab()->findOrCreateRuntimeHelper(populateSlotHelper, false, false, false);

      // Patch first slot test with call to resolution helper.
      //
      *picSlotCursor++ = 0xe8;    // CALL
      disp32 = cg()->branchDisplacementToHelperOrTrampoline(picSlotCursor+4, resolveSlotHelperSymRef);
      *(int32_t *)picSlotCursor = disp32;

      cg()->addExternalRelocation(new (cg()->trHeapMemory())
         TR::ExternalRelocation(picSlotCursor,
                                    (uint8_t *)resolveSlotHelperSymRef,
                                    TR_HelperAddress,
                                    cg()),  __FILE__, __LINE__, _startOfPicInstruction->getNode());

         picSlotCursor = (uint8_t *)(picSlotCursor - 1 + sizeofPicSlot);

         // Patch remaining slots with call to populate helper.
         //
         while (--numPicSlots)
            {
            *picSlotCursor++ = 0xe8;    // CALL
            disp32 = cg()->branchDisplacementToHelperOrTrampoline(picSlotCursor+4, populateSlotHelperSymRef);
            *(int32_t *)picSlotCursor = disp32;

            cg()->addExternalRelocation(new (cg()->trHeapMemory())
               TR::ExternalRelocation(picSlotCursor,
                                          (uint8_t *)populateSlotHelperSymRef,
                                          TR_HelperAddress,
                                          cg()), __FILE__, __LINE__, _startOfPicInstruction->getNode());
            picSlotCursor = (uint8_t *)(picSlotCursor - 1 + sizeofPicSlot);
            }
      }

   return cursor;
   }

uint32_t TR::X86PicDataSnippet::getLength(int32_t estimatedSnippetStart)
   {
   if (isInterface())
      {
      return   5                                 // Lookup dispatch
             + 5                                 // JMP done
             + (4 * sizeof(uintptrj_t))          // Resolve slots
             + (TR::Compiler->target.is64Bit() ? 2 : 1)   // ModRM or REX+MOV
             + (_hasJ2IThunkInPicData ? sizeof(uintptrj_t) : 0) // j2i thunk pointer
             + sizeof (uintptrj_t) - 1;          // alignment
      }
   else
      {
      return   6                                 // CALL [Mem] (pessimistically assume a SIB is needed)
             + (TR::Compiler->target.is64Bit() ? 2 : 0)   // REX for CALL + SIB for CALL (64-bit)
             + 5                                 // JMP done
             + (2 * sizeof(uintptrj_t))          // cpAddr, cpIndex
             + (unresolvedDispatch() ? sizeof(uintptrj_t) : 0)  // directMethod
             + (_hasJ2IThunkInPicData ? sizeof(uintptrj_t) : 0) // j2i thunk

             // 64-bit Data
             // -----------
             //  2 (REX+MOV)
             // +2 (REX+ModRM for CALL)
             //
             // 32-bit Data
             // -----------
             //  1 (ModRM for CMP)
             //
             + (TR::Compiler->target.is64Bit() ? 4 : 1)
             + cg()->getLowestCommonCodePatchingAlignmentBoundary()-1;
      }
   }
