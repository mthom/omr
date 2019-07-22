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

#include "x/codegen/X86SystemLinkage.hpp"

#include <stddef.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/FPTreeEvaluator.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/LiveRegister.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RegisterPair.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/TRMemory.hpp"
#include "il/ILOpCodes.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/symbol/AutomaticSymbol.hpp"
#include "il/symbol/ParameterSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/List.hpp"
#include "ras/Debug.hpp"
#include "codegen/X86Instruction.hpp"
#include "x/codegen/X86Ops.hpp"
#include "env/CompilerEnv.hpp"

static void evaluateCommonedNodes(TR::Node *node, TR::CodeGenerator *cg)
   {
   // There is a rule that if a node with a symref is evaluated, it must be
   // evaluated in the first treetop under which it appears.  (The so-called
   // "prompt evaluation" rule).  Since we don't know what future trees will
   // do, this effectively means that any symref-bearing node that is commoned
   // with another treetop must be evaluated now.
   // We approximate this by saying that any node with a refcount >= 2 must be
   // evaluated now.  The "refcount >= 2" is a conservative approximation of
   // "commoned with another treetop" because the latter is not cheap to figure out.
   // "Any node" is an approximation of "any node with a symref"; we do that
   // because it allows us to use a simple linear-time tree walk without
   // resorting to visit counts.
   //
   TR::Compilation * comp= cg->comp();
   if (node->getRegister() == NULL)
      {
      if (node->getReferenceCount() >= 2)
         {
         if (comp->getOption(TR_TraceCG))
            traceMsg(comp, "Promptly evaluating commoned node %s\n", cg->getDebug()->getName(node));
         cg->evaluate(node);
         }
      else
         {
         for (int32_t i = 0; i < node->getNumChildren(); i++)
            evaluateCommonedNodes(node->getChild(i), cg);
         }
      }
   }

TR::X86SystemLinkage::X86SystemLinkage(TR::CodeGenerator *cg)
   : TR::Linkage(cg)
   {
   }

const TR::X86LinkageProperties& TR::X86SystemLinkage::getProperties()
   {
   return _properties;
   }

TR::Register *TR::X86SystemLinkage::buildIndirectDispatch(TR::Node *callNode)
   {
   TR::StackMemoryRegion stackMemoryRegion(*comp()->trMemory());
   TR_FrontEnd *fe = comp()->fe();
   
   TR::X86CallSite site(callNode, this);

   // buildCallArgs.
   site.setArgSize(buildArgs(site.getCallNode(), site.getPreConditionsUnderConstruction()));
   
   bool skipVFTmaskInstruction = false;
   if (callNode->getSymbol()->castToMethodSymbol()->firstArgumentIsReceiver())
      {
      // if this is an indirect call, the receiver is at index 1.
      TR::Node *rcvrChild = callNode->getChild(callNode->getFirstArgumentIndex()); 
      TR::Node  *vftChild = callNode->getFirstChild();
      bool loadVFTForNullCheck = false;

      if (cg()->getCurrentEvaluationTreeTop()->getNode()->getOpCodeValue() == TR::NULLCHK
         && vftChild->getOpCode().isLoadIndirect()
         && vftChild->getFirstChild() == cg()->getCurrentEvaluationTreeTop()->getNode()->getNullCheckReference()
         && vftChild->getFirstChild()->isNonNull() == false)
         loadVFTForNullCheck = true;

      if (rcvrChild->isNonNull() == false || callNode->getFirstChild()->getReferenceCount() > 1)
         {
         /*
         if (vftChild->getRegister() == NULL)
            {
            cg()->generateDebugCounter(
               TR::DebugCounter::debugCounterName(comp(), "cg.vftload/%s/(%s)/%d/%d", "loadvft",
                                    comp()->signature(),
                                    callNode->getByteCodeInfo().getCallerIndex(),
                                    callNode->getByteCodeInfo().getByteCodeIndex()));
            }*/
         site.evaluateVFT();
         }
      }

   // Children of the VFT expression may also survive the call.
   // (Note that the following is not sufficient for the VFT node
   // itself, which should use site.evaluateVFT instead.)
   //
   if (skipVFTmaskInstruction == false)
      evaluateCommonedNodes(callNode->getFirstChild(), cg());

   TR::Instruction *startBookmark = cg()->getAppendInstruction();
   TR::LabelSymbol *startLabel    = generateLabelSymbol(cg());
   TR::LabelSymbol *doneLabel     = generateLabelSymbol(cg());
   startLabel->setStartInternalControlFlow();
   doneLabel->setEndInternalControlFlow();

   // Allocate thunk if necessary
   //
   void *virtualThunk = NULL;
   
   if (getProperties().getNeedsThunksForIndirectCalls())
      {
      TR::MethodSymbol *methodSymbol = callNode->getSymbol()->castToMethodSymbol();
      TR_Method       *method       = methodSymbol->getMethod();

      virtualThunk = TR::Compiler->getVirtualDispatchThunk(method);

      if (!virtualThunk)
	virtualThunk = TR::Compiler->setVirtualDispatchThunk(method, generateVirtualIndirectThunk(callNode));
      }

   site.setThunkAddress((uint8_t *)virtualThunk);

   TR::LabelSymbol *picMismatchLabel = NULL;
   // Build the call
   //
   //if (site.getMethodSymbol()->isVirtual() || site.getMethodSymbol()->isComputed())
   buildVirtualOrComputedCall(site, picMismatchLabel, doneLabel, (uint8_t *)virtualThunk);   

   // Construct postconditions
   //
   TR::Node *vftChild = callNode->getFirstChild();
   TR::Register *vftRegister = vftChild->getRegister();
   TR::Register *returnRegister;
   
   if (vftChild->getRegister() && (vftChild->getReferenceCount() > 1))
      {
      // VFT child survives the call, so we must include it in the postconditions.
      returnRegister = buildCallPostconditions(site);
      if (vftChild->getRegister() && vftChild->getRegister()->getRegisterPair())
         {
         site.addPostCondition(vftChild->getRegister()->getRegisterPair()->getHighOrder(), TR::RealRegister::NoReg);
         site.addPostCondition(vftChild->getRegister()->getRegisterPair()->getLowOrder(), TR::RealRegister::NoReg);
         }
      else
         site.addPostCondition(vftChild->getRegister(), TR::RealRegister::NoReg);
      cg()->recursivelyDecReferenceCount(vftChild);
      }
   else
      {
      // VFT child dies here; decrement it early so it doesn't interfere with dummy regs.
      cg()->recursivelyDecReferenceCount(vftChild);
      returnRegister = buildCallPostconditions(site);
      }

   site.stopAddingConditions();

   // Create the internal control flow region and VFP adjustment
   //
   generateLabelInstruction(startBookmark, LABEL, startLabel, site.getPreConditionsUnderConstruction(), cg());
   if (!getProperties().getCallerCleanup())
      generateVFPCallCleanupInstruction(-site.getArgSize(), callNode, cg());
   generateLabelInstruction(LABEL, callNode, doneLabel, site.getPostConditionsUnderConstruction(), cg());

   // Stop using the killed registers that are not going to persist
   //
   stopUsingKilledRegisters(site.getPostConditionsUnderConstruction(), returnRegister);

   if (callNode->getType().isFloatingPoint())
      {
      static char *forceX87LinkageForSSE = feGetEnv("TR_ForceX87LinkageForSSE");
      if (callNode->getReferenceCount() == 1 && returnRegister->getKind() == TR_X87)
         {
         // If the method returns a floating-point value that is not used, insert a
         // dummy store to eventually pop the value from the floating-point stack.
         //
         generateFPSTiST0RegRegInstruction(FSTRegReg, callNode, returnRegister, returnRegister, cg());
         }
      else if (forceX87LinkageForSSE && returnRegister->getKind() == TR_FPR)
         {
         // If the caller expects the return value in an XMMR, insert a
         // transfer from the floating-point stack to the XMMR via memory.
         //
         coerceFPReturnValueToXMMR(callNode, site.getPostConditionsUnderConstruction(), site.getMethodSymbol(), returnRegister);
         }
      }

   if (cg()->enableRegisterAssociations())
      associatePreservedRegisters(site.getPostConditionsUnderConstruction(), returnRegister);

   cg()->setImplicitExceptionPoint(site.getImplicitExceptionPoint());

   return returnRegister;
   }

TR::Register *TR::X86SystemLinkage::buildCallPostconditions(TR::X86CallSite &site)
   {
   TR::RegisterDependencyConditions *dependencies = site.getPostConditionsUnderConstruction();
   TR_ASSERT(dependencies != NULL, "assertion failure");

   const TR::X86LinkageProperties &properties   = getProperties();
   const TR::RealRegister::RegNum  noReg        = TR::RealRegister::NoReg;
   TR::Node                       *callNode     = site.getCallNode();
   TR::MethodSymbol               *methodSymbol = callNode->getSymbolReference()->getSymbol()->castToMethodSymbol();
   bool                calleePreservesRegisters = methodSymbol->preservesAllRegisters();

// #ifdef J9VM_OPT_JAVA_CRYPTO_ACCELERATION
//    // AES helpers actually use Java private linkage and do not preserve all
//    // registers.  This should really be handled by the linkage.
//    //
//    if (cg()->enableAESInHardwareTransformations() && methodSymbol && methodSymbol->isHelper())
//       {
//       TR::SymbolReference *methodSymRef = callNode->getSymbolReference();
//       switch (methodSymRef->getReferenceNumber())
//          {
//          case TR_doAESInHardwareInner:
//          case TR_expandAESKeyInHardwareInner:
//             calleePreservesRegisters = false;
//             break;
// 
//          default:
//             break;
//          }
//       }
// #endif

   // We have to be careful to allocate the return register after the
   // dependency conditions for the other killed registers have been set up,
   // otherwise it will be marked as interfering with them.

   // Figure out which is the return register
   //
   TR::RealRegister::RegNum  returnRegIndex, highReturnRegIndex=noReg;
   TR_RegisterKinds          returnKind;
   
   switch(callNode->getDataType())
      {
      default:
         TR_ASSERT(0, "Unrecognized call node data type: #%d", (int)callNode->getDataType());
         // fall through
      case TR::NoType:
         returnRegIndex  = noReg;
         returnKind      = TR_NoRegister;
         break;
      case TR::Int64:
         if (cg()->usesRegisterPairsForLongs())
            {
            returnRegIndex     = getProperties().getLongLowReturnRegister();
            highReturnRegIndex = getProperties().getLongHighReturnRegister();
            returnKind         = TR_GPR;
            break;
            }
         // else fall through
      case TR::Int8:
      case TR::Int16:
      case TR::Int32:
      case TR::Address:
         returnRegIndex  = getProperties().getIntegerReturnRegister();
         returnKind      = TR_GPR;
         break;
      case TR::Float:
      case TR::Double:
         returnRegIndex  = getProperties().getFloatReturnRegister();
         returnKind      = TR_FPR;
         break;
      }

   // Find the registers that are already in the postconditions so we don't add them again.
   // (The typical example is the ramMethod.)
   //
   int32_t gprsAlreadyPresent = TR::RealRegister::noRegMask;
   TR_X86RegisterDependencyGroup *group = dependencies->getPostConditions();
   
   for (int i = 0; i < dependencies->getAddCursorForPost(); i++)
      {
      TR::RegisterDependency *dep = group->getRegisterDependency(i);
      TR_ASSERT(dep->getRealRegister() <= TR::RealRegister::LastAssignableGPR, "Currently, only GPRs can be added to call postcondition before buildCallPostconditions; found %s", cg()->getDebug()->getRealRegisterName(dep->getRealRegister()-1));
      gprsAlreadyPresent |= TR::RealRegister::gprMask((TR::RealRegister::RegNum)dep->getRealRegister());
      }

   // Add postconditions indicating the state of arg regs (other than the return reg)
   //
   if (calleePreservesRegisters)
      {
      // For all argument-register preconditions, add an identical
      // postcondition, thus indicating that the arguments are preserved.
      // Note: this assumes the postcondition regdeps have preconditions too; see COPY_PRECONDITIONS_TO_POSTCONDITIONS.
      //
      TR_X86RegisterDependencyGroup *preConditions = dependencies->getPreConditions();
      for (int i = 0; i < dependencies->getAddCursorForPre(); i++)
         {
         TR::RegisterDependency *preCondition = preConditions->getRegisterDependency(i);
         TR::RealRegister::RegNum   regIndex     = preCondition->getRealRegister();

         if (regIndex <= TR::RealRegister::LastAssignableGPR && (gprsAlreadyPresent & TR::RealRegister::gprMask(regIndex)))
            continue;

         if (
            regIndex != returnRegIndex && regIndex != highReturnRegIndex
            && (properties.isIntegerArgumentRegister(regIndex) || properties.isFloatArgumentRegister(regIndex))
            ){
            dependencies->addPostCondition(preCondition->getRegister(), regIndex, cg());
            }
         }
      }
   else
      {
      // Kill all non-preserved int and float regs besides the return register,
      // by assigning them to unused virtual registers
      //
      TR::RealRegister::RegNum regIndex;

      for (regIndex = TR::RealRegister::FirstGPR; regIndex <= TR::RealRegister::LastAssignableGPR; regIndex = (TR::RealRegister::RegNum)(regIndex + 1))
         {
         // Skip non-assignable registers
         //
         if (machine()->getRealRegister(regIndex)->getState() == TR::RealRegister::Locked)
            continue;

         // Skip registers already present
         if (gprsAlreadyPresent & TR::RealRegister::gprMask(regIndex))
            continue;

         if ((regIndex != returnRegIndex) && (regIndex != highReturnRegIndex) && !properties.isPreservedRegister(regIndex))
            {
            TR::Register *dummy = cg()->allocateRegister(TR_GPR);
            dummy->setPlaceholderReg();
            dependencies->addPostCondition(dummy, regIndex, cg());
            cg()->stopUsingRegister(dummy);
            }
         }

      TR_LiveRegisters *lr = cg()->getLiveRegisters(TR_FPR);
      if(!lr || lr->getNumberOfLiveRegisters() > 0)
         {
         for (regIndex = TR::RealRegister::FirstXMMR; regIndex <= TR::RealRegister::LastXMMR; regIndex = (TR::RealRegister::RegNum)(regIndex + 1))
            {
            TR_ASSERT(regIndex != highReturnRegIndex, "highReturnRegIndex should not be an XMM register.");
            if ((regIndex != returnRegIndex) && !properties.isPreservedRegister(regIndex))
               {
               TR::Register *dummy = cg()->allocateRegister(TR_FPR);
               dummy->setPlaceholderReg();
               dependencies->addPostCondition(dummy, regIndex, cg());
               cg()->stopUsingRegister(dummy);
               }
            }
         }
      }

   // Preserve the VM thread register
   //
   dependencies->addPostCondition(cg()->getMethodMetaDataRegister(), getProperties().getMethodMetaDataRegister(), cg());

   // Now that everything is dead, we can allocate the return register without
   // interference
   //
   TR::Register *returnRegister;
   if (highReturnRegIndex)
      {
      TR::Register *lo = cg()->allocateRegister(returnKind);
      TR::Register *hi = cg()->allocateRegister(returnKind);
      returnRegister = cg()->allocateRegisterPair(lo, hi);
      dependencies->addPostCondition(lo, returnRegIndex,     cg());
      dependencies->addPostCondition(hi, highReturnRegIndex, cg());
      }
   else if (returnRegIndex)
      {
      TR_ASSERT(returnKind != TR_NoRegister, "assertion failure");
      if (callNode->getDataType() == TR::Address)
         {
         returnRegister = cg()->allocateCollectedReferenceRegister();
         }
      else
         {
         returnRegister = cg()->allocateRegister(returnKind);
         if (callNode->getDataType() == TR::Float)
            returnRegister->setIsSinglePrecision();
         }
      dependencies->addPostCondition(returnRegister, returnRegIndex, cg());
      }
   else
      {
      returnRegister = NULL;
      }

   return returnRegister;
   }


void TR::X86SystemLinkage::buildVPIC(TR::X86CallSite &site, TR::LabelSymbol *entryLabel, TR::LabelSymbol *doneLabel)
   {
     //   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
   TR_ASSERT(doneLabel, "a doneLabel is required for VPIC dispatches");

   if (entryLabel)
      generateLabelInstruction(LABEL, site.getCallNode(), entryLabel, cg());

   int32_t numVPicSlots = VPicParameters.defaultNumberOfSlots;

   TR::SymbolReference *callHelperSymRef =
      cg()->symRefTab()->findOrCreateRuntimeHelper(TR_X86populateVPicSlotCall, true, true, false);

   if (numVPicSlots > 1)
      {
      TR::X86PICSlot emptyPicSlot = TR::X86PICSlot(VPicParameters.defaultSlotAddress, NULL);
      emptyPicSlot.setNeedsShortConditionalBranch();
      emptyPicSlot.setJumpOnNotEqual();
      emptyPicSlot.setNeedsPicSlotAlignment();
      emptyPicSlot.setHelperMethodSymbolRef(callHelperSymRef);
      emptyPicSlot.setGenerateNextSlotLabelInstruction();

      // Generate all slots except the last
      // (short branch to next slot, jump to doneLabel)
      //
      while (--numVPicSlots)
         {
         TR::LabelSymbol *nextSlotLabel = generateLabelSymbol(cg());
         buildPICSlot(emptyPicSlot, nextSlotLabel, doneLabel, site);
         }
      }

   // Generate the last slot
   // (long branch to lookup snippet, fall through to doneLabel)
   //
   TR::X86PICSlot lastPicSlot = TR::X86PICSlot(VPicParameters.defaultSlotAddress, NULL, false);
   lastPicSlot.setJumpOnNotEqual();
   lastPicSlot.setNeedsPicSlotAlignment();
   lastPicSlot.setNeedsLongConditionalBranch();

   if (TR::Compiler->target.is32Bit())
      {
      lastPicSlot.setNeedsPicCallAlignment();
      }

   lastPicSlot.setHelperMethodSymbolRef(callHelperSymRef);

   TR::LabelSymbol *snippetLabel = generateLabelSymbol(cg());

   TR::Instruction *slotPatchInstruction = buildPICSlot(lastPicSlot, snippetLabel, NULL, site);

   TR::Instruction *startOfPicInstruction = site.getFirstPICSlotInstruction();
   
//   TR::X86PicDataSnippet *snippet = new (trHeapMemory()) TR::X86PicDataSnippet(
//      VPicParameters.defaultNumberOfSlots,
//      startOfPicInstruction,
//      snippetLabel,
//      doneLabel,
//      site.getSymbolReference(),
//      slotPatchInstruction,
//      site.getThunkAddress(),
//      false,
//      cg());
//
//   snippet->gcMap().setGCRegisterMask(site.getPreservedRegisterMask());
//   cg()->addSnippet(snippet);

   cg()->incPicSlotCountBy(VPicParameters.defaultNumberOfSlots);
   cg()->reserveNTrampolines(VPicParameters.defaultNumberOfSlots);
   }


TR::Instruction *TR::X86SystemLinkage::buildVFTCall(TR::X86CallSite &site, TR_X86OpCode dispatchOp, TR::Register *targetAddressReg, TR::MemoryReference *targetAddressMemref)
   {
   TR::Node *callNode = site.getCallNode();
   if (cg()->enableSinglePrecisionMethods() &&
       comp()->getJittedMethodSymbol()->usesSinglePrecisionMode())
      {
      auto cds = cg()->findOrCreate2ByteConstant(callNode, DOUBLE_PRECISION_ROUND_TO_NEAREST);
      generateMemInstruction(LDCWMem, callNode, generateX86MemoryReference(cds, cg()), cg());
      }

   TR::Instruction *callInstr;
   if (dispatchOp.sourceIsMemRef())
      {
      TR_ASSERT(targetAddressMemref, "Call via memory requires memref");
      // Fix the displacement at 4 bytes so j2iVirtual can decode it if necessary
      if (targetAddressMemref)
         targetAddressMemref->setForceWideDisplacement();
      callInstr = generateCallMemInstruction(dispatchOp.getOpCodeValue(), callNode, targetAddressMemref, cg());
      }
   else
      {
      TR_ASSERT(targetAddressReg, "Call via register requires register");
      TR::Node *callNode = site.getCallNode();
      TR::ResolvedMethodSymbol *resolvedMethodSymbol = callNode->getSymbol()->getResolvedMethodSymbol();
      bool mayReachJ2IThunk = true;
//      if (resolvedMethodSymbol && resolvedMethodSymbol->getRecognizedMethod() == TR::java_lang_invoke_ComputedCalls_dispatchDirect)
//         mayReachJ2IThunk = false;
      if (mayReachJ2IThunk && dispatchOp.isCallOp())
         {
         // Bad news.
         //
         // icallVMprJavaSendPatchupVirtual requires that a virtual call site
         // either (1) uses a CALLMem with a fixed VFT offset, or (2) puts the
         // VFT index into r8 and uses a CALLImm4 with a fixed call target.
         // We have neither a fixed VFT offset nor a fixed call target!
         // Adding support for CALLReg is difficult because the instruction is
         // a different length, making it hard to back up and disassemble it.
         //
         // Therefore, we cannot have the return address pointing after a
         // CALLReg instruction.  Instead, we use a CALLImm4 with a fixed
         // displacement to get to out-of-line instructions that do a JMPReg.

         // Mainline call
         //
         TR::LabelSymbol *jmpLabel   = TR::LabelSymbol::create(cg()->trHeapMemory(),cg());
         callInstr = generateLabelInstruction(CALLImm4, callNode, jmpLabel, cg());

         // Jump outlined
         //
         {
         TR_OutlinedInstructionsGenerator og(jmpLabel, callNode, cg());
         generateRegInstruction(JMPReg, callNode, targetAddressReg, cg());
         }

         // The targetAddressReg doesn't appear to be used in mainline code, so
         // register assignment may do weird things like spill it.  We'd prefer it
         // to stay in a register, though we don't care which.
         //
         TR::RegisterDependencyConditions *dependencies = site.getPostConditionsUnderConstruction();
         if (targetAddressReg && targetAddressReg->getRegisterPair())
            {
            dependencies->unionPreCondition(targetAddressReg->getRegisterPair()->getHighOrder(), TR::RealRegister::NoReg, cg());
            dependencies->unionPreCondition(targetAddressReg->getRegisterPair()->getLowOrder(), TR::RealRegister::NoReg, cg());
            }
         else
            dependencies->unionPreCondition(targetAddressReg, TR::RealRegister::NoReg, cg());
         }
      else
         {
         callInstr = generateRegInstruction(dispatchOp.getOpCodeValue(), callNode, targetAddressReg, cg());
         }
      }

   callInstr->setNeedsGCMap(site.getPreservedRegisterMask());

   TR_ASSERT_FATAL(
      !site.getSymbolReference()->isUnresolved() || site.getMethodSymbol()->isInterface(),
      "buildVFTCall: unresolved virtual site");

   if (cg()->enableSinglePrecisionMethods() &&
       comp()->getJittedMethodSymbol()->usesSinglePrecisionMode())
      {
      auto cds = cg()->findOrCreate2ByteConstant(callNode, SINGLE_PRECISION_ROUND_TO_NEAREST);
      generateMemInstruction(LDCWMem, callNode, generateX86MemoryReference(cds, cg()), cg());
      }

   return callInstr;
   }


int32_t TR::X86SystemLinkage::computeMemoryArgSize(
      TR::Node *callNode,
      int32_t first,
      int32_t last,
      int8_t direction)
   {
   int32_t                  sizeOfOutGoingArgs= 0;
   uint16_t                 numIntArgs = 0,  numFloatArgs = 0;
   int32_t i;
   for (i = first; i != last; i += direction)
      {
      TR::parmLayoutResult layoutResult;
      TR::Node *child = callNode->getChild(i);

      layoutParm(child, sizeOfOutGoingArgs, numIntArgs, numFloatArgs, layoutResult);
      }
   return sizeOfOutGoingArgs;
   }

static const TR::RealRegister::RegNum NOT_ASSIGNED = (TR::RealRegister::RegNum)-1;

// Copies parameters from where they enter the method (either on stack or in a
// linkage register) to their "home location" where the method body will expect
// to find them (either on stack or in a global register).
//
TR::Instruction *
TR::X86SystemLinkage::copyParametersToHomeLocation(TR::Instruction *cursor)
   {
   TR::Machine *machine = cg()->machine();
   TR::RealRegister *framePointer = machine->getRealRegister(TR::RealRegister::vfp);

   TR::ResolvedMethodSymbol             *bodySymbol = comp()->getJittedMethodSymbol();
   ListIterator<TR::ParameterSymbol>  paramIterator(&(bodySymbol->getParameterList()));
   TR::ParameterSymbol               *paramCursor;

   const TR::RealRegister::RegNum noReg = TR::RealRegister::NoReg;
   TR_ASSERT(noReg == 0, "noReg must be zero so zero-initializing movStatus will work");

   TR::MovStatus movStatus[TR::RealRegister::NumRegisters] = {{(TR::RealRegister::RegNum)0,(TR::RealRegister::RegNum)0,(TR_MovDataTypes)0}};

   // We must always do the stores first, then the reg-reg copies, then the
   // loads, so that we never clobber a register we will need later.  However,
   // the logic is simpler if we do the loads and stores in the same loop.
   // Therefore, we maintain a separate instruction cursor for the loads.
   //
   // We defer the initialization of loadCursor until we generate the first
   // load.  Otherwise, if we happen to generate some stores first, then the
   // store cursor would get ahead of the loadCursor, and the instructions
   // would end up in the wrong order despite our efforts.
   //
   TR::Instruction *loadCursor = NULL;

   // Phase 1: generate RegMem and MemReg movs, and collect information about
   // the required RegReg movs.
   //
   for (paramCursor = paramIterator.getFirst();
       paramCursor != NULL;
       paramCursor = paramIterator.getNext())
      {
      int8_t lri = paramCursor->getLinkageRegisterIndex();     // How the parameter enters the method
      TR::RealRegister::RegNum ai                              // Where method body expects to find it
         = (TR::RealRegister::RegNum)paramCursor->getAllocatedIndex();
      int32_t offset = paramCursor->getParameterOffset();      // Location of the parameter's stack slot
      TR_MovDataTypes movDataType = paramMovType(paramCursor); // What sort of MOV instruction does it need?

      // Copy the parameter to wherever it should be
      //
      if (lri == NOT_LINKAGE) // It's on the stack
         {
         if (ai == NOT_ASSIGNED) // It only needs to be on the stack
            {
            // Nothing to do
            }
         else // Method body expects it to be in the ai register
            {
            if (loadCursor == NULL)
               loadCursor = cursor;

            if (debug("traceCopyParametersToHomeLocation"))
               diagnostic("copyParametersToHomeLocation: Loading %d\n", ai);
            // ai := stack
            loadCursor = generateRegMemInstruction(
               loadCursor,
               TR::Linkage::movOpcodes(RegMem, movDataType),
               machine->getRealRegister(ai),
               generateX86MemoryReference(framePointer, offset, cg()),
               cg()
               );
            }
         }
      else // It's in a linkage register
         {
         TR::RealRegister::RegNum sourceIndex = getProperties().getArgumentRegister(lri, isFloat(movDataType));

         // Copy to the stack if necessary
         //
         if (ai == NOT_ASSIGNED || hasToBeOnStack(paramCursor))
            {
            if (comp()->getOption(TR_TraceCG))
              traceMsg(comp(), "copyToHomeLocation param %p, linkage reg index %d, allocated index %d, parameter offset %d, hasToBeOnStack %d, parm->isParmHasToBeOnStack() %d.\n", paramCursor, lri, ai, offset, hasToBeOnStack(paramCursor), paramCursor->isParmHasToBeOnStack());
            if (debug("traceCopyParametersToHomeLocation"))
               diagnostic("copyParametersToHomeLocation: Storing %d\n", sourceIndex);
            // stack := lri
            cursor = generateMemRegInstruction(
               cursor,
               TR::Linkage::movOpcodes(MemReg, movDataType),
               generateX86MemoryReference(framePointer, offset, cg()),
               machine->getRealRegister(sourceIndex),
               cg()
               );
            }

         // Copy to the ai register if necessary
         //
         if (ai != NOT_ASSIGNED && ai != sourceIndex)
            {
            // This parameter needs a RegReg move.  We don't know yet whether
            // we need the value in the target register, so for now we just
            // remember that we need to do this and keep going.
            //
            TR_ASSERT(movStatus[ai         ].sourceReg == noReg, "Each target reg must have only one source");
            TR_ASSERT(movStatus[sourceIndex].targetReg == noReg, "Each source reg must have only one target");
            if (debug("traceCopyParametersToHomeLocation"))
               diagnostic("copyParametersToHomeLocation: Planning to move %d to %d\n", sourceIndex, ai);
            movStatus[ai].sourceReg                  = sourceIndex;
            movStatus[sourceIndex].targetReg         = ai;
            movStatus[sourceIndex].outgoingDataType  = movDataType;
            }

         if (debug("traceCopyParametersToHomeLocation") && ai == sourceIndex)
            {
            diagnostic("copyParametersToHomeLocation: Parameter #%d already in register %d\n", lri, ai);
            }
         }
      }

   // Phase 2: Iterate through the parameters again to insert the RegReg moves.
   //
   for (paramCursor = paramIterator.getFirst();
       paramCursor != NULL;
       paramCursor = paramIterator.getNext())
      {
      if (paramCursor->getLinkageRegisterIndex() == NOT_LINKAGE)
         continue;

      const TR::RealRegister::RegNum paramReg =
         getProperties().getArgumentRegister(paramCursor->getLinkageRegisterIndex(), isFloat(paramMovType(paramCursor)));

      if (movStatus[paramReg].targetReg == 0)
         {
         // This parameter does not need to be copied anywhere
         if (debug("traceCopyParametersToHomeLocation"))
            diagnostic("copyParametersToHomeLocation: Not moving %d\n", paramReg);
         }
      else
         {
         if (debug("traceCopyParametersToHomeLocation"))
            diagnostic("copyParametersToHomeLocation: Preparing to move %d\n", paramReg);

         // If a mov's target register is the source for another mov, we need
         // to do that other mov first.  The idea is to find the end point of
         // the chain of movs starting with paramReg and ending with a
         // register whose current value is not needed; then do that chain of
         // movs in reverse order.
         //
         TR_ASSERT(noReg == 0, "noReg must be zero (not %d) for zero-filled initialization to work", noReg);

         TR::RealRegister::RegNum regCursor;

         // Find the last target in the chain
         //
         regCursor = movStatus[paramReg].targetReg;
         while(movStatus[regCursor].targetReg != noReg)
            {
            // Haven't found the end yet
            regCursor = movStatus[regCursor].targetReg;
            TR_ASSERT(regCursor != paramReg, "Can't yet handle cyclic dependencies");

            // TODO:AMD64 Use scratch register to break cycles
            // A properly-written pickRegister should never
            // cause cycles to occur in the first place.  However, we may want
            // to consider adding cycle-breaking logic so that (1) pickRegister
            // has more flexibility, and (2) we're more robust against
            // otherwise harmless bugs in pickRegister.
            }

         // Work our way backward along the chain, generating all the necessary movs
         //
         while(movStatus[regCursor].sourceReg != noReg)
            {
            TR::RealRegister::RegNum source = movStatus[regCursor].sourceReg;
            if (debug("traceCopyParametersToHomeLocation"))
               diagnostic("copyParametersToHomeLocation: Moving %d to %d\n", source, regCursor);
            // regCursor := regCursor.sourceReg
            cursor = generateRegRegInstruction(
               cursor,
               TR::Linkage::movOpcodes(RegReg, movStatus[source].outgoingDataType),
               machine->getRealRegister(regCursor),
               machine->getRealRegister(source),
               cg()
               );
            // Update movStatus as we go so we don't generate redundant movs
            movStatus[regCursor].sourceReg = noReg;
            movStatus[source   ].targetReg = noReg;
            // Continue with the next register in the chain
            regCursor = source;
            }
         }
      }

   // Return the last instruction we inserted, whether or not it was a load.
   //
   return loadCursor? loadCursor : cursor;
   }


TR::Instruction *
TR::X86SystemLinkage::savePreservedRegisters(TR::Instruction *cursor)
   {
   // For IA32 usePushForPreservedRegs will be true;
   // For X64, usePushForPreservedRegs always false;
   TR::ResolvedMethodSymbol *bodySymbol = comp()->getJittedMethodSymbol();
   const int32_t localSize = getProperties().getOffsetToFirstLocal() - bodySymbol->getLocalMappingCursor();
   const int32_t pointerSize = getProperties().getPointerSize();

   int32_t offsetCursor = -localSize + getProperties().getOffsetToFirstLocal() - pointerSize;

   if (_properties.getUsesPushesForPreservedRegs())
      {
      for (int32_t pindex = _properties.getMaxRegistersPreservedInPrologue()-1;
           pindex >= 0;
           pindex--)
         {
         TR::RealRegister::RegNum idx = _properties.getPreservedRegister((uint32_t)pindex);
         TR::RealRegister *reg = machine()->getRealRegister(idx);
         if (reg->getHasBeenAssignedInMethod() && reg->getState() != TR::RealRegister::Locked)
            {
            cursor = new (trHeapMemory()) TR::X86RegInstruction(cursor, PUSHReg, reg, cg());
            }
         }
      }
   else
      {
      for (int32_t pindex = getProperties().getMaxRegistersPreservedInPrologue()-1;
           pindex >= 0;
           pindex--)
         {
         TR::RealRegister::RegNum idx = _properties.getPreservedRegister((uint32_t)pindex);
         TR::RealRegister *reg = machine()->getRealRegister(getProperties().getPreservedRegister((uint32_t)pindex));
         if(reg->getHasBeenAssignedInMethod() && reg->getState() != TR::RealRegister::Locked)
            {
            cursor = generateMemRegInstruction(
               cursor,
               TR::Linkage::movOpcodes(MemReg, fullRegisterMovType(reg)),
               generateX86MemoryReference(machine()->getRealRegister(TR::RealRegister::vfp), offsetCursor, cg()),
               reg,
               cg()
               );
            offsetCursor -= pointerSize;
            }
         }
      }
   return cursor;
   }


void
TR::X86SystemLinkage::createPrologue(TR::Instruction *cursor)
   {
#if defined(DEBUG)
   // TODO:AMD64: Get this into the debug DLL

   class TR_DebugFrameSegmentInfo
      {
      private:

      TR_DebugFrameSegmentInfo *_next;
      const char *_description;
      TR::RealRegister *_register;
      int32_t _lowOffset;
      uint8_t _size;
      TR::Compilation * _comp;

      public:

      TR_ALLOC(TR_Memory::CodeGenerator)

      TR_DebugFrameSegmentInfo(
         TR::Compilation * c,
         int32_t lowOffset,
         uint8_t size,
         const char *description,
         TR_DebugFrameSegmentInfo *next,
         TR::RealRegister *reg=NULL
         ):
         _comp(c),
         _next(next),
         _description(description),
         _register(reg),
         _lowOffset(lowOffset),
         _size(size)
         {}

      TR::Compilation * comp() { return _comp; }

      TR_DebugFrameSegmentInfo *getNext(){ return _next; }

      TR_DebugFrameSegmentInfo *sort()
         {
         TR_DebugFrameSegmentInfo *result;
         TR_DebugFrameSegmentInfo *tail = _next? _next->sort() : NULL;
         TR_DebugFrameSegmentInfo *before=NULL, *after;
         for (after = tail; after; before=after, after=after->_next)
            {
            if (after->_lowOffset > _lowOffset)
               break;
            }
         _next = after;
         if (before)
            {
            before->_next = this;
            result = tail;
            }
         else
            {
            result = this;
            }
         return result;
         }

      void print(TR_Debug *debug)
         {
         if (_next)
            _next->print(debug);
         if (_size > 0)
            {
            diagnostic("        % 4d: % 4d -> % 4d (% 4d) %5.5s %s\n",
               _lowOffset, _lowOffset, _lowOffset + _size - 1, _size,
               _register? debug->getName(_register, TR_DoubleWordReg) : "",
               _description
               );
            }
         else
            {
            diagnostic("        % 4d: % 4d -> ---- (% 4d) %5.5s %s\n",
               _lowOffset, _lowOffset, _size,
               _register? debug->getName(_register, TR_DoubleWordReg) : "",
               _description
               );
            }
         }

      };

   TR_DebugFrameSegmentInfo *debugFrameSlotInfo=NULL;
#endif

   TR::RealRegister *espReal = machine()->getRealRegister(TR::RealRegister::esp);

   TR::ResolvedMethodSymbol *bodySymbol = comp()->getJittedMethodSymbol();

   const TR::X86LinkageProperties &properties = getProperties();

   const uint32_t outgoingArgSize = cg()->getLargestOutgoingArgSize();

   // Compute the nature of the preserved regs
   //
   uint32_t preservedRegsSize = 0;
   uint32_t registerSaveDescription = 0; // bit N corresponds to _preservedRegisters[N], with 1=preserved

   int32_t pindex; // Preserved register index
   for (pindex = 0; pindex < properties.getMaxRegistersPreservedInPrologue(); pindex++)
      {
      TR::RealRegister *reg = machine()->getRealRegister(properties.getPreservedRegister((uint32_t)pindex));
      if (reg->getHasBeenAssignedInMethod() && reg->getState() != TR::RealRegister::Locked)
         {
         preservedRegsSize += properties.getPointerSize();
         registerSaveDescription |= reg->getRealRegisterMask();
         }
      }

   cg()->setRegisterSaveDescription(registerSaveDescription);

   // Compute frame size
   //
   // allocSize: bytes to be subtracted from the stack pointer when allocating the frame
   // frameSize: total bytes of stack the prologue consumes
   //
   //const int32_t localSize = -properties.getOffsetToFirstLocal() - bodySymbol->getLocalMappingCursor();
   const int32_t localSize = properties.getOffsetToFirstLocal() - bodySymbol->getLocalMappingCursor();
   TR_ASSERT(localSize >= 0, "expecting positive localSize");

   // Note that the return address doesn't appear here because it is allocated by
   // the call instruction
   //
   int32_t allocSize = localSize
      + ( properties.getUsesPushesForPreservedRegs()? 0 : preservedRegsSize )
      + ( properties.getReservesOutgoingArgsInPrologue()? outgoingArgSize : 0 );

   int32_t frameSize = localSize + preservedRegsSize
      + ( properties.getReservesOutgoingArgsInPrologue()? outgoingArgSize : 0 )
      + ( properties.getAlwaysDedicateFramePointerRegister() ? properties.getGPRWidth() : 0);

   uint32_t adjust = 0;
   if (_properties.getOutgoingArgAlignment() && !cg()->isLeafMethod())
      {
      // AMD64 SysV spec requires: The end of the input argument area shall be aligned on a 16 (32, if __m256 is passed on stack) byte boundary. In other words, the value (%rsp + 8) is always a multiple of 16 (32) when control is transferred to the function entry point.
      TR_ASSERT(_properties.getOutgoingArgAlignment() == 16 || _properties.getOutgoingArgAlignment() == 4, "AMD64 SysV linkage require outgoingArgAlignment be 16/32 bytes aligned, while IA32 linkage require 4 bytes aligned.  We currently haven't support 32 bytes alignment for AMD64 SysV ABI yet.\n");
      const uint32_t stackAlignMinus1 = _properties.getOutgoingArgAlignment() - 1;
      adjust = ((frameSize + properties.getRetAddressWidth()  + stackAlignMinus1) & ~stackAlignMinus1) - (frameSize + properties.getRetAddressWidth());
      frameSize += adjust;
      allocSize += adjust;
      }

   cg()->setFrameSizeInBytes(frameSize);

   // Set the VFP state for the PROCENTRY instruction
   //
   if (properties.getAlwaysDedicateFramePointerRegister())
      {
      cursor = new (trHeapMemory()) TR::X86RegInstruction(
         cursor,
         PUSHReg,
         machine()->getRealRegister(properties.getFramePointerRegister()),
         cg());

      TR::RealRegister *stackPointerReg = machine()->getRealRegister(TR::RealRegister::esp);
      cursor = new (trHeapMemory()) TR::X86RegRegInstruction(
         cursor,
         MOVRegReg(),
         machine()->getRealRegister(properties.getFramePointerRegister()),
         stackPointerReg,
         cg());

      cg()->initializeVFPState(properties.getFramePointerRegister(), _properties.getPointerSize());
      }
   else
      {
      cg()->initializeVFPState(TR::RealRegister::esp, 0);
      }

   // Entry breakpoint
   //
   if (comp()->getOption(TR_EntryBreakPoints))
      {
      cursor = new (trHeapMemory()) TR::Instruction(BADIA32Op, cursor, cg());
      }

   // Allocate the stack frame
   //
   const int32_t singleWordSize = TR::Compiler->target.is32Bit() ? 4 : 8;
   if (allocSize == 0)
      {
      // No need to do anything
      }
   else if (allocSize == singleWordSize)
      {
      TR::RealRegister *realReg = getSingleWordFrameAllocationRegister();
      cursor = new (trHeapMemory()) TR::X86RegInstruction(cursor, PUSHReg, realReg, cg());
      }
   else
      {
      const TR_X86OpCodes subOp = (allocSize <= 127)? SUBRegImms() : SUBRegImm4();
      cursor = new (trHeapMemory()) TR::X86RegImmInstruction(cursor, subOp, espReal, allocSize, cg());
      }

   // Save preserved regs, and tell the frontend how many there are
   //
   bodySymbol->setProloguePushSlots(preservedRegsSize / properties.getPointerSize());

   cursor = savePreservedRegisters(cursor);

   if (comp()->getOption(TR_TraceCG))
      {
      traceMsg(comp(), "create prologue using system linkage, after savePreservedRegisters, cursor is %p.\n", cursor);
      }

   cursor = copyParametersToHomeLocation(cursor);

   if (comp()->getOption(TR_TraceCG))
      {
      traceMsg(comp(), "create prologue using system linkage, after copyParametersToHomeLocation, cursor is %p.\n", cursor);
      }

#if defined(DEBUG)
   if (comp()->getOption(TR_TraceCG))
      {
      traceMsg(comp(), "\nFrame size: locals=%d frame=%d\n",localSize, frameSize);
      }
   ListIterator<TR::ParameterSymbol>  paramIterator(&(bodySymbol->getParameterList()));
   TR::ParameterSymbol               *paramCursor;
   for (
      paramCursor = paramIterator.getFirst();
      paramCursor != NULL;
      paramCursor = paramIterator.getNext()
      ){
      TR::RealRegister::RegNum ai = (TR::RealRegister::RegNum)paramCursor->getAllocatedIndex();
      debugFrameSlotInfo = new (trHeapMemory()) TR_DebugFrameSegmentInfo(comp(),
         paramCursor->getOffset(), paramCursor->getSize(), "Parameter",
         debugFrameSlotInfo,
         (ai==NOT_ASSIGNED)? NULL : machine()->getRealRegister(ai)
         );
      }

   if (properties.getAlwaysDedicateFramePointerRegister())
      {
      debugFrameSlotInfo = new (trHeapMemory()) TR_DebugFrameSegmentInfo(comp(),
         properties.getOffsetToFirstLocal(), properties.getGPRWidth(), "EBP-save",
         debugFrameSlotInfo
         );
      }

   ListIterator<TR::AutomaticSymbol>  autoIterator(&bodySymbol->getAutomaticList());
   TR::AutomaticSymbol               *autoCursor;
   for (
      autoCursor = autoIterator.getFirst();
      autoCursor != NULL;
      autoCursor = autoIterator.getNext()
      ){
      debugFrameSlotInfo = new (trHeapMemory()) TR_DebugFrameSegmentInfo(comp(),
         autoCursor->getOffset(), autoCursor->getSize(), "Local",
         debugFrameSlotInfo
         );
      }

   debugFrameSlotInfo = new (trHeapMemory()) TR_DebugFrameSegmentInfo(comp(),
      0, properties.getRetAddressWidth(), "Return address",
      debugFrameSlotInfo
      );
   debugFrameSlotInfo = new (trHeapMemory()) TR_DebugFrameSegmentInfo(comp(),
      properties.getOffsetToFirstLocal() - localSize - preservedRegsSize,
      preservedRegsSize, "Preserved args size",
      debugFrameSlotInfo
      );
   debugFrameSlotInfo = new (trHeapMemory()) TR_DebugFrameSegmentInfo(comp(),
      properties.getOffsetToFirstLocal() - localSize - preservedRegsSize - adjust,
      adjust, "adjust size for stack alignment",
      debugFrameSlotInfo
      );
   debugFrameSlotInfo = new (trHeapMemory()) TR_DebugFrameSegmentInfo(comp(),
      properties.getOffsetToFirstLocal() - localSize - preservedRegsSize - outgoingArgSize - adjust,
      outgoingArgSize, "Outgoing args",
      debugFrameSlotInfo
      );
   if (comp()->getOption(TR_TraceCG))
      {
      diagnostic("\nFrame layout:\n");
      diagnostic("        +rsp  +vfp     end  size        what\n");
      debugFrameSlotInfo->sort()->print(cg()->getDebug());
      diagnostic("\n");
      }
#endif

   }


TR::Instruction *
TR::X86SystemLinkage::restorePreservedRegisters(TR::Instruction *cursor)
   {
   TR::ResolvedMethodSymbol *bodySymbol = comp()->getJittedMethodSymbol();
   const int32_t localSize   = _properties.getOffsetToFirstLocal() - bodySymbol->getLocalMappingCursor();
   const int32_t pointerSize = _properties.getPointerSize();

   if (cg()->pushPreservedRegisters())
      {
      for (int32_t pindex = 0;
           pindex <_properties.getMaxRegistersPreservedInPrologue();
           pindex++)
         {
         TR::RealRegister::RegNum idx = _properties.getPreservedRegister((uint32_t)pindex);
         TR::RealRegister *reg = machine()->getRealRegister(idx);
         if (reg->getHasBeenAssignedInMethod())
            {
            cursor = new (trHeapMemory()) TR::X86RegInstruction(cursor, POPReg, reg, cg());
            }
         }
      }
   else
      {
      int32_t offsetCursor = -localSize +  _properties.getOffsetToFirstLocal() - pointerSize;
      for (int32_t pindex = _properties.getMaxRegistersPreservedInPrologue()-1;
           pindex >= 0;
           pindex--)
         {
         TR::RealRegister::RegNum idx = _properties.getPreservedRegister((uint32_t)pindex);
         TR::RealRegister *reg = machine()->getRealRegister(idx);

         if (comp()->getOption(TR_TraceCG))
            {
            traceMsg(comp(), "reg %d, getHasBeenAssignedInMethod %d\n", idx, reg->getHasBeenAssignedInMethod());
            }

         if (reg->getHasBeenAssignedInMethod())
            {
            cursor = generateRegMemInstruction(
               cursor,
               TR::Linkage::movOpcodes(RegMem, fullRegisterMovType(reg)),
               reg,
               generateX86MemoryReference(machine()->getRealRegister(TR::RealRegister::vfp), offsetCursor, cg()),
               cg()
               );
            offsetCursor -= pointerSize;
            }
         }
      }

   return cursor;
   }


void
TR::X86SystemLinkage::createEpilogue(TR::Instruction *cursor)
   {
   TR::RealRegister    *espReal      = machine()->getRealRegister(TR::RealRegister::esp);
   TR::ResolvedMethodSymbol *bodySymbol = comp()->getJittedMethodSymbol();

   const int32_t localSize = _properties.getOffsetToFirstLocal() - bodySymbol->getLocalMappingCursor();
   int32_t allocSize = _properties.getUsesPushesForPreservedRegs()? localSize : cg()->getFrameSizeInBytes();

   if (cg()->pushPreservedRegisters())
      {
      // Deallocate outgoing arg area in preparation to pop preserved regs
      //
      allocSize = localSize;
      const uint32_t outgoingArgSize = cg()->getLargestOutgoingArgSize();
      TR_X86OpCodes op = (outgoingArgSize <= 127) ? ADDRegImms() : ADDRegImm4();
      cursor = new (trHeapMemory()) TR::X86RegImmInstruction(cursor, op, espReal, outgoingArgSize, cg());
      }

   // Restore preserved regs
   //
   cursor = restorePreservedRegisters(cursor);

   if (comp()->getOption(TR_TraceCG))
      {
      traceMsg(comp(), "create epilogue using system linkage, after restorePreservedRegisters, cursor is %x.\n", cursor);
      }

   // Deallocate the stack frame
   //
   const int32_t singleWordSize = TR::Compiler->target.is32Bit() ? 4 : 8;
   if (_properties.getAlwaysDedicateFramePointerRegister())
      {
      // Restore stack pointer from frame pointer
      //
      cursor = new (trHeapMemory()) TR::X86RegRegInstruction(cursor, MOVRegReg(), espReal, machine()->getRealRegister(_properties.getFramePointerRegister()), cg());
      cursor = new (trHeapMemory()) TR::X86RegInstruction(cursor, POPReg, machine()->getRealRegister(_properties.getFramePointerRegister()), cg());
      }
   else if (allocSize == 0)
      {
      // No need to do anything
      }
   else if (allocSize == singleWordSize)
      {
      TR::RealRegister *realReg = getSingleWordFrameAllocationRegister();
      cursor = new (trHeapMemory()) TR::X86RegInstruction(cursor, POPReg, realReg, cg());
      }
   else
      {
      TR_X86OpCodes op = (allocSize <= 127) ? ADDRegImms() : ADDRegImm4();
      cursor = new (trHeapMemory()) TR::X86RegImmInstruction(cursor, op, espReal, allocSize, cg());
      }

   if (comp()->getOption(TR_TraceCG))
      {
      traceMsg(comp(), "create epilogue using system linkage, after delocating stack frame, cursor is %x.\n", cursor);
      }

   if (cursor->getNext()->getOpCodeValue() == RETImm2)
      {
      toIA32ImmInstruction(cursor->getNext())->setSourceImmediate(bodySymbol->getNumParameterSlots() << getProperties().getParmSlotShift());

      if (comp()->getOption(TR_TraceCG))
         {
         traceMsg(comp(), "create epilogue using system linkage, ret_IMM set to %d.\n", bodySymbol->getNumParameterSlots() << getProperties().getParmSlotShift());
         }
      }
   }


// TODO: add struct/union support in this function
void
TR::X86SystemLinkage::copyLinkageInfoToParameterSymbols()
   {
   TR::ResolvedMethodSymbol *bodySymbol = comp()->getJittedMethodSymbol();
   ListIterator<TR::ParameterSymbol>paramIterator(&(bodySymbol->getParameterList()));
   const TR::X86LinkageProperties &properties = getProperties();
   uint16_t numIntArgs = 0, numFloatArgs = 0;
   int32_t sizeOfOutGoingArgs = 0;
   int32_t maxIntArgs   = properties.getNumIntegerArgumentRegisters();
   int32_t maxFloatArgs = properties.getNumFloatArgumentRegisters();

  for (TR::ParameterSymbol *paramCursor = paramIterator.getFirst(); paramCursor != NULL; paramCursor = paramIterator.getNext())
      {
      TR::parmLayoutResult layoutResult;
      layoutParm(paramCursor, sizeOfOutGoingArgs, numIntArgs, numFloatArgs, layoutResult);
      if (layoutResult.abstract & TR::parmLayoutResult::IN_LINKAGE_REG_PAIR)
         {
         TR_ASSERT(false, "haven't provide register pair support yet.\n");
         }
      else if (layoutResult.abstract & TR::parmLayoutResult::IN_LINKAGE_REG)
         {
         paramCursor->setLinkageRegisterIndex(layoutResult.regs[0].regIndex);
         }
      // If we're out of registers, just stop now instead of looping doing nothing
      //
      if (numIntArgs >= maxIntArgs && numFloatArgs >= maxFloatArgs)
         break;

      }
   }


void
TR::X86SystemLinkage::mapIncomingParms(TR::ResolvedMethodSymbol *method)
   {
   TR_ASSERT(!getProperties().passArgsRightToLeft(), "Right-to-left not yet implemented on AMD64");
   int32_t sizeOfOutGoingArgs = 0;
   uint16_t numIntArgs = 0, numFloatArgs = 0;
   ListIterator<TR::ParameterSymbol> parameterIterator(&method->getParameterList());
   uint32_t bump = getProperties().getOffsetToFirstParm();

   for (TR::ParameterSymbol *parmCursor = parameterIterator.getFirst(); parmCursor; parmCursor = parameterIterator.getNext())
      {
      TR::parmLayoutResult layoutResult;
      layoutParm(parmCursor, sizeOfOutGoingArgs, numIntArgs, numFloatArgs, layoutResult);
      if (layoutResult.abstract & TR::parmLayoutResult::ON_STACK)
         {
         parmCursor->setParameterOffset(layoutResult.offset + bump);
         if (comp()->getOption(TR_TraceCG))
            traceMsg(comp(), "mapIncomingParms setParameterOffset %d for param symbol %p\n", parmCursor->getParameterOffset(), parmCursor);
         }
      }
   }


void
TR::X86SystemLinkage::copyGlRegDepsToParameterSymbols(
      TR::Node *bbStart,
      TR::CodeGenerator *cg)
   {
   TR_ASSERT(bbStart->getOpCodeValue() == TR::BBStart, "assertion failure");
   if (bbStart->getNumChildren() > 0)
      {
      TR::Node *glRegDeps = bbStart->getFirstChild();
      if (!glRegDeps) // No global register info, so nothing to do
         return;

      TR_ASSERT(glRegDeps->getOpCodeValue() == TR::GlRegDeps, "First child of first Node must be a GlRegDeps");

      uint16_t childNum;
      for (childNum=0; childNum < glRegDeps->getNumChildren(); childNum++)
         {
         TR::Node *child = glRegDeps->getChild(childNum);
         TR::ParameterSymbol *sym = child->getSymbol()->getParmSymbol();
         sym->setAllocatedIndex(cg->getGlobalRegister(child->getGlobalRegisterNumber()));
         }
      }
   }


int32_t
TR::X86SystemLinkage::getParameterStartingPos(
      int32_t &dataCursor,
      uint32_t align)
   {
   if (align <= getProperties().getParmSlotSize())
      {
      const int32_t stackSlotMinus1 = getProperties().getParmSlotSize() - 1;
      dataCursor = (dataCursor + stackSlotMinus1) & ~stackSlotMinus1;
      }
   else
      {
      uint32_t alignMinus1 = align - 1;
      dataCursor = (dataCursor + alignMinus1) & (~alignMinus1);
      }
   return dataCursor;
   }


int32_t
TR::X86SystemLinkage::layoutTypeOnStack(
      TR::DataType type,
      int32_t &dataCursor,
      TR::parmLayoutResult &layoutResult)
   {
   uint32_t typeAlign = getAlignment(type);
   layoutResult.offset = getParameterStartingPos(dataCursor, typeAlign);
   switch (type)
      {
      case TR::Float:
         dataCursor += 4;
         break;
      case TR::Double:
         dataCursor += 8;
         break;
      case TR::Int8:
         dataCursor += 1;
         break;
      case TR::Int16:
         dataCursor += 2;
         break;
      case TR::Int32:
         dataCursor += 4;
         break;
      case TR::Int64:
         dataCursor += 8;
         break;
      case TR::Address:
         dataCursor += TR::Compiler->target.is32Bit() ? 4 : 8;
         break;
      case TR::Aggregate:
      default:
         // TODO: struct/union support should be added as a case branch.
         TR_ASSERT(false, "types still not supported in data layout yet.\n");
         return 0;
      }
   return typeAlign;
   }

TR::X86CallSite::X86CallSite(TR::Node *callNode, TR::Linkage *calleeLinkage)
   :_callNode(callNode)
   ,_linkage(calleeLinkage)
   ,_vftImplicitExceptionPoint(NULL)
   ,_firstPICSlotInstruction(NULL)
   ,_profiledTargets(NULL)
   ,_interfaceClassOfMethod(NULL)
   ,_argSize(-1)
   ,_preservedRegisterMask(0)
   ,_thunkAddress(NULL)
   ,_useLastITableCache(false)
   {
     
//   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
//   if (getMethodSymbol()->isInterface())
//      {
//      // Find the class pointer to the interface class if it is already loaded.
//      // This is needed by both static PICs
//      //
//      TR::Method *interfaceMethod = getMethodSymbol()->getMethod();
//      int32_t len = interfaceMethod->classNameLength();
//      char * s = classNameToSignature(interfaceMethod->classNameChars(), len, comp());
//      _interfaceClassOfMethod = fej9->getClassFromSignature(s, len, getSymbolReference()->getOwningMethod(comp()));
//      }

//   setupVirtualGuardInfo();
//   computeProfiledTargets();

   // Initialize the register dependencies with conservative estimates of the
   // number of conditions
   //
   uint32_t numPreconditions =
        calleeLinkage->getProperties().getNumIntegerArgumentRegisters()
      + calleeLinkage->getProperties().getNumFloatArgumentRegisters()
      + 3; // VM Thread + eax + possible vtableIndex/J9Method arg on IA32

   uint32_t numPostconditions =
        calleeLinkage->getProperties().getNumberOfVolatileGPRegisters()
      + calleeLinkage->getProperties().getNumberOfVolatileXMMRegisters()
      + 3; // return reg + VM Thread + scratch

   _preConditionsUnderConstruction  = generateRegisterDependencyConditions(numPreconditions, 0, cg());
   _postConditionsUnderConstruction = generateRegisterDependencyConditions((COPY_PRECONDITIONS_TO_POSTCONDITIONS? numPreconditions : 0), numPostconditions + (COPY_PRECONDITIONS_TO_POSTCONDITIONS? numPreconditions : 0), cg());

   _preservedRegisterMask = getLinkage()->getProperties().getPreservedRegisterMapForGC();
   if (getMethodSymbol()->preservesAllRegisters())
      {
      _preservedRegisterMask |= TR::RealRegister::getAvailableRegistersMask(TR_GPR);
      if (callNode->getDataType() != TR::NoType)
         {
         // Cross our fingers and hope things that preserve all regs only return ints
         _preservedRegisterMask &= ~TR::RealRegister::gprMask(getLinkage()->getProperties().getIntegerReturnRegister());
         }
      }
   }

bool TR::X86CallSite::shouldUseInterpreterLinkage()
   {
   return false;
//   if (getMethodSymbol()->isVirtual() &&
//      !getSymbolReference()->isUnresolved() &&
//      getMethodSymbol()->isVMInternalNative() &&
//      !getResolvedMethod()->virtualMethodIsOverridden() &&
//      !getResolvedMethod()->isAbstract())
//      return true;
//   else
//      return false;
   }


TR::Register *TR::X86CallSite::evaluateVFT()
   {
   TR::Node *vftNode = getCallNode()->getFirstChild();
   if (vftNode->getRegister())
      return vftNode->getRegister();
   else
      {
      TR::Register *result = cg()->evaluate(vftNode);
      //_vftImplicitExceptionPoint = cg()->getImplicitExceptionPoint();
      return result;
      }
   }

bool TR::X86CallSite::resolvedVirtualShouldUseVFTCall()
   {
   return true;
//   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
//   TR_ASSERT(getMethodSymbol()->isVirtual() && !getSymbolReference()->isUnresolved(), "assertion failure");
//
//   return
//      !fej9->forceUnresolvedDispatch() &&
//      (!comp()->getOption(TR_EnableVPICForResolvedVirtualCalls)    ||
//       getProfiledTargets()                                        ||
//       getCallNode()->isTheVirtualCallNodeForAGuardedInlinedCall() ||
//       ( comp()->getSymRefTab()->findObjectNewInstanceImplSymbol() &&
//         comp()->getSymRefTab()->findObjectNewInstanceImplSymbol()->getSymbol() == getResolvedMethodSymbol()));
   }

void TR::X86CallSite::stopAddingConditions()
   {
   if (COPY_PRECONDITIONS_TO_POSTCONDITIONS)
      {
      TR_X86RegisterDependencyGroup *preconditions  = getPreConditionsUnderConstruction()->getPreConditions();
      TR_X86RegisterDependencyGroup *postconditions = getPostConditionsUnderConstruction()->getPostConditions();
      for (uint8_t i = 0; i < getPreConditionsUnderConstruction()->getAddCursorForPre(); i++)
         {
         TR::RegisterDependency *pre  = preconditions->getRegisterDependency(i);
         getPostConditionsUnderConstruction()->unionPreCondition(pre->getRegister(), pre->getRealRegister(), cg(), pre->getFlags());
         TR::RegisterDependency *post = postconditions->findDependency(pre->getRealRegister(), getPostConditionsUnderConstruction()->getAddCursorForPost());
         if (!post)
            getPostConditionsUnderConstruction()->addPostCondition(pre->getRegister(), pre->getRealRegister(), cg(), pre->getFlags());
         }
      }

   _preConditionsUnderConstruction->stopAddingPreConditions();
   _preConditionsUnderConstruction->stopAddingPostConditions();
   _postConditionsUnderConstruction->stopAddingPreConditions();
   _postConditionsUnderConstruction->stopAddingPostConditions();
   }
