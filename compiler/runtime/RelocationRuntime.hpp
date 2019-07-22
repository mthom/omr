/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#ifndef TR_RELOCATIONRUNTIME_INCL
#define TR_RELOCATIONRUNTIME_INCL

#include "runtime/OMRRelocationRuntime.hpp"

#include "runtime/CodeCacheConfig.hpp"
#include "env/TRMemory.hpp"

namespace TR
{

class OMR_EXTENSIBLE RelocationRuntime : public OMR::RelocationRuntimeConnector
   {
   public:
   RelocationRuntime(JitConfig* jitConfig);
};
class SharedCacheRelocationRuntime : public RelocationRuntime {
public:
      TR_ALLOC(TR_Memory::Relocation);
      void * operator new(size_t, TR::JitConfig *);
      SharedCacheRelocationRuntime(TR::SharedCache* cache) : RelocationRuntime(NULL) {
         _sharedCacheIsFull=false;
         _cache = cache;
         }

   //  virtual bool storeAOTHeader(OMR_VM *omrVm, TR_FrontEnd *fe, OMR_VMThread *curThread);
   //    virtual TR::AOTHeader *createAOTHeader(OMR_VM *omrVM, TR_FrontEnd *fe);
   //   virtual bool validateAOTHeader(OMR_VM *omrVm, TR_FrontEnd *fe, OMR_VMThread *curThread);

//      virtual void *isROMClassInSharedCaches(UDATA romClassValue, OMR_VM *omrVm);
 //     virtual bool isRomClassForMethodInSharedCache(OMRMethod *method, OMR_VM *omrVm);
  //    virtual TR_YesNoMaybe isMethodInSharedCache(OMRMethod *method, OMR_VM *omrVm);

      //virtual TR_OpaqueClassBlock *getClassFromCP(OMR_VMThread *vmThread, OMR_VM *omrVm, J9ConstantPool *constantPool, I_32 cpIndex, bool isStatic);
      virtual void *methodAddress(char *methodName);
      virtual void registerLoadedMethod(const char *&methodName,void *&methodAddress);

private:
      uint32_t getCurrentLockwordOptionHashValue(OMR_VM *vm) const;
      virtual uint8_t * allocateSpaceInCodeCache(UDATA codeSize);
      virtual uint8_t * allocateSpaceInDataCache(UDATA metaDataSize, UDATA type);
      virtual void initializeAotRuntimeInfo();
      virtual void initializeCacheDeltas();
      TR::SharedCache* _cache;
      virtual void incompatibleCache(U_32 module, U_32 reason, char *assumeMessage);

      void checkAOTHeaderFlags(TR_FrontEnd *fe, TR::AOTHeader * hdrInCache, intptr_t featureFlags);
      bool generateError(char *assumeMessage);

      bool _sharedCacheIsFull;

      static bool useDFPHardware(TR_FrontEnd *fe);
      static uintptr_t generateFeatureFlags(TR_FrontEnd *fe);

      static const char aotHeaderKey[];
      static const UDATA aotHeaderKeyLength;
      std::map<std::string,void *> _symbolLocation;
};
}

#endif
