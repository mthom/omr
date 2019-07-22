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

#ifndef X86SYSTEMLINKAGE_INCL
#define X86SYSTEMLINKAGE_INCL

#include <stdint.h>
#include "codegen/Linkage.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "il/DataTypes.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class Node; }
namespace TR { class ParameterSymbol; }
// namespace TR { class RegisterDependencyConditions; }
namespace TR { class ResolvedMethodSymbol; }

// This is an overkill way to make sure the preconditions survive up to the call instruction.
// It's also necessary to make sure any spills happen below the internal control flow, due
// to the nature of the backward register assignment pass.
// TODO: Put the preconditions on the call instruction itself.
//
#define COPY_PRECONDITIONS_TO_POSTCONDITIONS (1)

namespace TR {

typedef struct parmLayoutResult
   {
   enum statusEnum
      {
      IN_LINKAGE_REG = 0x1,
      IN_LINKAGE_REG_PAIR = 0x2, // Only on AMD64 SysV ABI: a struct might take 2 linkage registers.
      ON_STACK = 0x4,
      };
   uint8_t abstract;
   uint32_t offset;
   struct
      {
      TR_RegisterKinds regKind;
      uint16_t regIndex;
      } regs[2];
   parmLayoutResult()
      {
      abstract = (statusEnum)0;
      offset =0;
      regs[0].regKind = regs[1].regKind = (TR_RegisterKinds)-1;
      regs[0].regIndex = regs[1].regIndex = (uint16_t)-1;
      }
   } parmLayoutResult;


enum
   {
   BranchJNE     = 0,
   BranchJE      = 1,
   BranchNopJMP  = 2
   };

enum
   {
   PicSlot_NeedsShortConditionalBranch      = 0x01,
   PicSlot_NeedsLongConditionalBranch       = 0x02,
   PicSlot_NeedsPicSlotAlignment            = 0x04,
   PicSlot_NeedsPicCallAlignment            = 0x08,
   PicSlot_NeedsJumpToDone                  = 0x10,
   PicSlot_GenerateNextSlotLabelInstruction = 0x20
   };

struct PicParameters
   {
   intptrj_t defaultSlotAddress;
   int32_t roundedSizeOfSlot;
   int32_t defaultNumberOfSlots;
   };
  
class X86PICSlot
   {
   public:

   TR_ALLOC(TR_Memory::Linkage);

   X86PICSlot(uintptrj_t classAddress, TR_ResolvedMethod *method, bool jumpToDone=true, TR_OpaqueMethodBlock *m = NULL, int32_t slot = -1):
     _classAddress(classAddress), _method(method), _helperMethodSymbolRef(NULL), _branchType(BranchJNE), _methodAddress(m), _slot(slot)
      {
      if (jumpToDone) setNeedsJumpToDone(); // TODO: Remove this oddball.  We can tell whether we need a dump to done based on whether a doneLabel is passed to buildPICSlot
      }

   uintptrj_t          getClassAddress()                         { return _classAddress; }
   TR_ResolvedMethod  *getMethod()                               { return _method; }

   TR_OpaqueMethodBlock *getMethodAddress()                      { return _methodAddress; }

   int32_t getSlot()                                           { return _slot; }

   void                setHelperMethodSymbolRef(TR::SymbolReference *symRef)
                                                                 { _helperMethodSymbolRef = symRef; }
   TR::SymbolReference *getHelperMethodSymbolRef()                { return _helperMethodSymbolRef; }

   void                setJumpOnNotEqual()                       { _branchType = BranchJNE; }
   bool                needsJumpOnNotEqual()                     { return _branchType == BranchJNE; }

   void                setJumpOnEqual()                          { _branchType = BranchJE; }
   bool                needsJumpOnEqual()                        { return _branchType == BranchJE; }

   void                setNopAndJump()                           { _branchType = BranchNopJMP; }
   bool                needsNopAndJump()                         { return _branchType == BranchNopJMP; }

   bool                needsShortConditionalBranch()             {return _flags.testAny(PicSlot_NeedsShortConditionalBranch);}
   void                setNeedsShortConditionalBranch()          {_flags.set(PicSlot_NeedsShortConditionalBranch);}

   bool                needsLongConditionalBranch()              {return _flags.testAny(PicSlot_NeedsLongConditionalBranch);}
   void                setNeedsLongConditionalBranch()           {_flags.set(PicSlot_NeedsLongConditionalBranch);}

   bool                needsPicSlotAlignment()                   {return _flags.testAny(PicSlot_NeedsPicSlotAlignment);}
   void                setNeedsPicSlotAlignment()                {_flags.set(PicSlot_NeedsPicSlotAlignment);}

   bool                needsPicCallAlignment()                   {return _flags.testAny(PicSlot_NeedsPicCallAlignment);}
   void                setNeedsPicCallAlignment()                {_flags.set(PicSlot_NeedsPicCallAlignment);}

   bool                needsJumpToDone()                         {return _flags.testAny(PicSlot_NeedsJumpToDone);}
   void                setNeedsJumpToDone()                      {_flags.set(PicSlot_NeedsJumpToDone);}

   bool                generateNextSlotLabelInstruction()        {return _flags.testAny(PicSlot_GenerateNextSlotLabelInstruction);}
   void                setGenerateNextSlotLabelInstruction()     {_flags.set(PicSlot_GenerateNextSlotLabelInstruction);}

   protected:

   flags8_t            _flags;
   uintptrj_t          _classAddress;
   TR_ResolvedMethod  *_method;
   TR::SymbolReference *_helperMethodSymbolRef;
   TR_OpaqueMethodBlock *_methodAddress;
   int32_t             _slot;
   uint8_t             _branchType;
   };

class X86CallSite
   {
   public:

   X86CallSite(TR::Node *callNode, TR::Linkage *calleeLinkage);

   TR::Node      *getCallNode(){ return _callNode; }
   TR::Linkage *getLinkage(){ return _linkage; }

   TR::CodeGenerator *cg(){ return _linkage->cg(); }
   TR::Compilation    *comp(){ return _linkage->comp(); }
   TR_FrontEnd         *fe(){ return _linkage->fe(); }

   // Register dependency construction
   TR::RegisterDependencyConditions *getPreConditionsUnderConstruction() { return _preConditionsUnderConstruction; }
   TR::RegisterDependencyConditions *getPostConditionsUnderConstruction(){ return _postConditionsUnderConstruction; }

   void addPreCondition (TR::Register *virtualReg, TR::RealRegister::RegNum realReg){ _preConditionsUnderConstruction->unionPreCondition(virtualReg, realReg, cg()); }
   void addPostCondition(TR::Register *virtualReg, TR::RealRegister::RegNum realReg){ _postConditionsUnderConstruction->unionPostCondition(virtualReg, realReg, cg()); }
   void stopAddingConditions();

   // Immutable call site properties
   TR::ResolvedMethodSymbol *getCallerSym(){ return comp()->getMethodSymbol(); }
   TR::MethodSymbol         *getMethodSymbol(){ return _callNode->getSymbol()->castToMethodSymbol(); }
   TR::ResolvedMethodSymbol *getResolvedMethodSymbol(){ return _callNode->getSymbol()->getResolvedMethodSymbol(); }
   TR_ResolvedMethod       *getResolvedMethod(){ return getResolvedMethodSymbol()? getResolvedMethodSymbol()->getResolvedMethod() : NULL; }
   TR::SymbolReference      *getSymbolReference(){ return _callNode->getSymbolReference(); }
   TR_OpaqueClassBlock     *getInterfaceClassOfMethod(){ return _interfaceClassOfMethod; } // NULL for virtual methods

   // Abstraction of complex decision logic
   bool shouldUseInterpreterLinkage();
   //   bool vftPointerMayPersist();
   TR_ScratchList<TR::X86PICSlot> *getProfiledTargets(){ return _profiledTargets; } // NULL if there aren't any
   TR_VirtualGuardKind getVirtualGuardKind(){ return _virtualGuardKind; }
   TR_ResolvedMethod *getDevirtualizedMethod(){ return _devirtualizedMethod; }  // The method to be dispatched statically
   TR::SymbolReference *getDevirtualizedMethodSymRef(){ return _devirtualizedMethodSymRef; }
   TR::Register *evaluateVFT();
   TR::Instruction *getImplicitExceptionPoint(){ return _vftImplicitExceptionPoint; }
   TR::Instruction *setImplicitExceptionPoint(TR::Instruction *instr){ return _vftImplicitExceptionPoint = instr; }
   uint32_t getPreservedRegisterMask(){ return _preservedRegisterMask; }
   bool resolvedVirtualShouldUseVFTCall();

   TR::Instruction *getFirstPICSlotInstruction() {return _firstPICSlotInstruction;}
   void setFirstPICSlotInstruction(TR::Instruction *f) {_firstPICSlotInstruction = f;}
   uint8_t *getThunkAddress() {return _thunkAddress;}
   void setThunkAddress(uint8_t *t) {_thunkAddress = t;}

   float getMinProfiledCallFrequency(){ return .075F; }  // Tuned for megamorphic site in jess; so bear in mind before changing

   public:

   bool    argsHaveBeenBuilt(){ return _argSize >= 0; }
   void    setArgSize(int32_t s){ TR_ASSERT(!argsHaveBeenBuilt(), "assertion failure"); _argSize = s; }
   int32_t getArgSize(){ TR_ASSERT(argsHaveBeenBuilt(), "assertion failure"); return _argSize; }

   bool useLastITableCache(){ return _useLastITableCache; }

   private:

   TR::Node       *_callNode;
   TR::Linkage *_linkage;
   TR_OpaqueClassBlock *_interfaceClassOfMethod;
   int32_t  _argSize;
   uint32_t _preservedRegisterMask;
   TR::RegisterDependencyConditions *_preConditionsUnderConstruction;
   TR::RegisterDependencyConditions *_postConditionsUnderConstruction;
   TR::Instruction *_vftImplicitExceptionPoint;
   TR::Instruction *_firstPICSlotInstruction;

     //   void computeProfiledTargets();
   TR_ScratchList<TR::X86PICSlot> *_profiledTargets;

     // void setupVirtualGuardInfo();
   TR_VirtualGuardKind  _virtualGuardKind;
   TR_ResolvedMethod   *_devirtualizedMethod;
   TR::SymbolReference  *_devirtualizedMethodSymRef;

   uint8_t *_thunkAddress;

   bool _useLastITableCache;
   };
  
class X86SystemLinkage : public TR::Linkage
   {
   public:
     
   struct PicParameters IPicParameters;
   struct PicParameters VPicParameters;

   virtual uint8_t *generateVirtualIndirectThunk(TR::Node *callNode) = 0;

   protected:
   X86SystemLinkage(TR::CodeGenerator *cg);

   TR::X86LinkageProperties _properties;

   TR::Instruction* copyParametersToHomeLocation(TR::Instruction *cursor);

   TR::Instruction* savePreservedRegisters(TR::Instruction *cursor);
   TR::Instruction* restorePreservedRegisters(TR::Instruction *cursor);

   virtual void createPrologue(TR::Instruction *cursor);
   virtual void createEpilogue(TR::Instruction *cursor);

   int32_t computeMemoryArgSize(TR::Node *callNode, int32_t first, int32_t last, int8_t direction);
   int32_t getParameterStartingPos(int32_t &dataCursor, uint32_t align);
   int32_t layoutTypeOnStack(TR::DataType, int32_t&, TR::parmLayoutResult&);
   virtual int32_t buildArgs(TR::Node *callNode, TR::RegisterDependencyConditions *deps) = 0;
   virtual uint32_t getAlignment(TR::DataType) = 0;
   virtual int32_t layoutParm(TR::Node *parmNode, int32_t &dataCursor, uint16_t &intReg, uint16_t &floatReg, TR::parmLayoutResult &layoutResult) = 0;
   virtual int32_t layoutParm(TR::ParameterSymbol *paramSymbol, int32_t &dataCursor, uint16_t &intReg, uint16_t &floatRrgs, TR::parmLayoutResult&) = 0;

   virtual void buildVirtualOrComputedCall(TR::X86CallSite &site, TR::LabelSymbol *entryLabel, TR::LabelSymbol *doneLabel, uint8_t *thunk) = 0;
   virtual TR::Register* buildVolatileAndReturnDependencies(TR::Node*, TR::RegisterDependencyConditions*) = 0;
   virtual TR::Instruction *buildVFTCall(X86CallSite &site, TR_X86OpCode dispatchOp, TR::Register *targetAddressReg, TR::MemoryReference *targetAddressMemref);
   virtual void buildVPIC(X86CallSite &site, TR::LabelSymbol *entryLabel, TR::LabelSymbol *doneLabel);
   virtual TR::Instruction *buildPICSlot(TR::X86PICSlot picSlot, TR::LabelSymbol *mismatchLabel, TR::LabelSymbol *doneLabel, X86CallSite &site)=0;
   virtual TR::Register *buildCallPostconditions(X86CallSite &site);

   /**
    * @brief Returns a register appropriate for allocating/de-allocating small stack frames
    *
    * When the size of a new stack frame that is the same size as a word on the
    * platform, the frame can be simply allocated/de-allocated by pushing/popping
    * a volatile, non-return register. This function returns a register that
    * is guarenteed to be safe for such uses.
    */
   virtual TR::RealRegister* getSingleWordFrameAllocationRegister() = 0;
     
   public:

   const TR::X86LinkageProperties& getProperties();

   virtual TR::Register *buildIndirectDispatch(TR::Node *callNode) = 0;
   virtual TR::Register *buildDirectDispatch(TR::Node *callNode, bool spillFPRegs) = 0;

   virtual void mapIncomingParms(TR::ResolvedMethodSymbol *method);
   virtual void mapIncomingParms(TR::ResolvedMethodSymbol *method, uint32_t &stackIndex) { mapIncomingParms(method); };


   virtual void copyLinkageInfoToParameterSymbols();
   virtual void copyGlRegDepsToParameterSymbols(TR::Node *bbStart, TR::CodeGenerator *cg);

   virtual void setUpStackSizeForCallNode(TR::Node *node) = 0;
   };

inline TR::X86SystemLinkage *toX86SystemLinkage(TR::Linkage *l) {return (TR::X86SystemLinkage *)l;}

}

#endif
