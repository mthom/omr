C/*******************************************************************************
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

#ifndef J9SHARED_CACHE_HPP
#define J9SHARED_CACHE_HPP

#include "compiler/env/SharedCache.hpp"

#include <stdint.h>
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "runtime/Runtime.hpp"

// substitute for J9Method.. for now.
typedef (void*) OMRMethod;

//class TR_J9VMBase;
class TR_ResolvedMethod;
// doesn't exist just yet.
// namespace TR { class CompilationInfo; }

struct OMRSharedCacheDescriptor {
  // I don't know if we need a header just yet. It's an idea.
  //  OMRSharedCacheHeader* cacheStartAddress;
  void* methodStartAddress; // was: romclassStartAddress;
  //  void* metadataStartAddress; // other shit goes here.
  UDATA cacheSizeBytes;
  // void* deployedROMClassStartAddress;
  OMRSharedClassCacheDescriptor* next;  
};

class OMRSharedCache : public TR_SharedCache
   {
public:
   TR_ALLOC(TR_Memory::SharedCache)

   OMRSharedCache(); // TR_J9VMBase *fe); deprecate the front end!

   //bool isHint(TR_ResolvedMethod *, TR_SharedCacheHint, uint16_t *dataField = NULL);
   //bool isHint(OMRMethod *, TR_SharedCacheHint, uint16_t *dataField = NULL);
   //uint16_t getAllEnabledHints(OMRMethod *method);
   //void addHint(OMRMethod *, TR_SharedCacheHint);
   //void addHint(TR_ResolvedMethod *, TR_SharedCacheHint);
   bool isMostlyFull();

   virtual void *pointerFromOffsetInSharedCache(void *offset);
   virtual void *offsetInSharedCacheFromPointer(void *ptr);

   //void persistIprofileInfo(TR::ResolvedMethodSymbol *, TR::Compilation *comp);
   //void persistIprofileInfo(TR::ResolvedMethodSymbol *, TR_ResolvedMethod*, TR::Compilation *comp);

     //   J9Class * matchRAMclassFromROMclass(J9ROMClass * clazz, TR::Compilation * comp);

//   bool canRememberClass(TR_OpaqueClassBlock *classPtr)
//      {
//      return (rememberClass((J9Class *) classPtr, false) != NULL);
//      }
//
//   uintptrj_t *rememberClass(TR_OpaqueClassBlock *classPtr)
//      {
//      return (uintptrj_t *) rememberClass((J9Class *) classPtr, true);
//      }
//
//   UDATA *rememberClass(J9Class *clazz, bool create=true);

   UDATA rememberDebugCounterName(const char *name);
   const char *getDebugCounterName(UDATA offset);

//   bool classMatchesCachedVersion(J9Class *clazz, UDATA *chainData=NULL);
//  bool classMatchesCachedVersion(TR_OpaqueClassBlock *classPtr, UDATA *chainData=NULL)
//     {
//     return classMatchesCachedVersion((J9Class *) classPtr, chainData);
//     }

//   TR_OpaqueClassBlock *lookupClassFromChainAndLoader(uintptrj_t *chainData, void *classLoader);

   bool isPointerInSharedCache(void *ptr, void * & cacheOffset);
   
   enum TR_J9SharedCacheDisabledReason
      {
      UNINITIALIZED,
      NOT_DISABLED,
      AOT_DISABLED,
      AOT_HEADER_INVALID,
      AOT_HEADER_FAILED_TO_ALLOCATE,
      OMR_SHARED_CACHE_FAILED_TO_ALLOCATE,
      SHARED_CACHE_STORE_ERROR,
      SHARED_CACHE_FULL,
      // The following are probably equivalent to SHARED_CACHE_FULL - 
      // they could have failed because of no space but no error code is returned.
      SHARED_CACHE_CLASS_CHAIN_STORE_FAILED,
      AOT_HEADER_STORE_FAILED
      };
//   
//   static void setSharedCacheDisabledReason(TR_J9SharedCacheDisabledReason state) { _sharedCacheState = state; }
//   static TR_J9SharedCacheDisabledReason getSharedCacheDisabledReason() { return _sharedCacheState; }
     // TR::CompilationInfo doesn't exist for OMR yet.
     static TR_YesNoMaybe isSharedCacheDisabledBecauseFull(); // TR::CompilationInfo *compInfo);
//   static void setStoreSharedDataFailedLength(UDATA length) {_storeSharedDataFailedLength = length; }
   
private:
     // don't need this! it's a singleton instance.
     // OMRJITConfig *jitConfig() { return _jitConfig; }
     // J9JavaVM *javaVM() { return _javaVM; }
     //   TR_J9VMBase *fe() { return _fe; }
     // OMRSharedClassConfig *sharedCacheConfig() { return _sharedCacheConfig; }

     //   TR_AOTStats *aotStats() { return _aotStats; }

   void log(char *format, ...);

     // uint32_t getHint(J9VMThread * vmThread, OMRMethod *method);

     // COMMENTARY: This was what Irwin was talking about Wednesday? I guess?
//   void convertUnsignedOffsetToASCII(UDATA offset, char *myBuffer);
//   void createClassKey(UDATA classOffsetInCache, char *key, uint32_t & keyLength);

//   uint32_t numInterfacesImplemented(J9Class *clazz);

//   bool writeClassToChain(J9ROMClass *romClass, UDATA * & chainPtr);
//   bool writeClassesToChain(J9Class **superclasses, int32_t numSuperclasses, UDATA * & chainPtr);
//   bool writeInterfacesToChain(J9Class *clazz, UDATA * & chainPtr);
//   bool fillInClassChain(J9Class *clazz, UDATA *chainData, uint32_t chainLength,
//                         uint32_t numSuperclasses, uint32_t numInterfaces);
//
//   bool romclassMatchesCachedVersion(J9ROMClass *romClass, UDATA * & chainPtr, UDATA *chainEnd);
//   UDATA *findChainForClass(J9Class *clazz, const char *key, uint32_t keyLength);

//   uint16_t _initialHintSCount;
//   uint16_t _hintsEnabledMask;

//   J9JavaVM *_javaVM;
// TR_J9JITConfig *_jitConfig;
// TR_J9VMBase *_fe;
// TR::CompilationInfo *_compInfo;

     //   TR_AOTStats *_aotStats;
     // OMRSharedCacheConfig *_sharedCacheConfig;
   OMRSharedCacheDescriptorList* _cacheDescriptorList;
   UDATA _cacheStartAddress;
   UDATA _cacheSizeInBytes;
   UDATA _numDigitsForCacheOffsets;

   uint32_t _logLevel;
   bool _verboseHints;
   
   static TR_OMRSharedCacheDisabledReason _sharedCacheState;
   static TR_YesNoMaybe                   _sharedCacheDisabledBecauseFull;
   static UDATA                           _storeSharedDataFailedLength;
   };

#endif
