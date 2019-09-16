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

#ifndef OMR_AOTADAPTER_INCL
#define OMR_AOTADAPTER_INCL
#include "env/RawAllocator.hpp"
#include "infra/Annotations.hpp"
#include "compiler/env/jittypes.h"
#include "compiler/env/SharedCache.hpp"

// #include "runtime/Runtime.hpp"
#include <map>
#include <string>

#ifndef OMR_AOTADAPTER_CONNECTOR
#define OMR_AOTADAPTER_CONNECTOR
namespace OMR { class AotAdapter; }
namespace OMR { typedef OMR::AotAdapter AotAdapterConnector; }
#endif

#ifndef OMR_AOTMETHODHEADER_CONNECTOR
#define OMR_AOTMETHODHEADER_CONNECTOR
namespace OMR { class AOTMethodHeader; }
namespace OMR { typedef OMR::AOTMethodHeader AOTMethodHeaderConnector; }
#endif
namespace TR 
    {
    class AotAdapter;
    class SharedCacheRelocationRuntime;
      //    class SharedCache;
    class CodeCache;
    class CodeCacheManager;
    class RelocationRuntime;
    class CompilerEnv;
    class AOTMethodHeader;
    }
namespace OMR
{
class OMR_EXTENSIBLE AOTMethodHeader
   {
      // at compile time, the constructor runs with four arguments, 
      // relocationsSize, compiledCodeSize, compiledCodeStart and relocationsStart
      // at loadtime we don't know anything, so we run constructor with no 
      // parameters and the values from cache are derived.
      // This is one possible implementation, for a cache with contiguous
      // code and relocations data stored
   public:
      AOTMethodHeader() {}
      AOTMethodHeader(uint8_t* compiledCodeStart, uint32_t compiledCodeSize, uint8_t* relocationsStart, uint32_t relocationsSize):
         compiledCodeStart(compiledCodeStart),
         compiledCodeSize(compiledCodeSize),
         relocationsStart(relocationsStart),
         relocationsSize(relocationsSize)
         {};
      AOTMethodHeader(uint8_t* rawData);

      uint8_t* compiledCodeStart;
      uint32_t compiledCodeSize;
      uint8_t* relocationsStart;
      uint32_t relocationsSize;
      
      virtual void* serialize();
      virtual uintptrj_t sizeOfSerializedVersion();
      
      // uintptrj_t  exceptionTableStart;
      // // Here, compiledDataStart is a pointer to any data persisted along with the
      // // compiled code. offset to RelocationsTable points to Relocations, should
      // // be equal 
      // uintptrj_t compiledDataStart;
      // uintptrj_t compiledDataSize;

   
   };

class AbstractVMObject;
struct SOMCacheMetadataItemHeader;

class OMR_EXTENSIBLE AotAdapter
  {
public:
    AotAdapter() {}

    TR::AotAdapter* self();
    TR::RelocationRuntime* rr();

    void initializeAOTClasses(TR::RawAllocator* allocator, TR::CodeCacheManager* CodeCacheManager);
    void storeExternalSymbol(const char *symbolName, void* symbolAddress);
    void registerAOTMethodHeader(std::string methodName, TR::AOTMethodHeader* hdr);
    void storeHeaderForCompiledMethod(const char *methodName);
    virtual TR::AOTMethodHeader* createAndRegisterAOTMethodHeader(const char* methodName, uint8_t* codeStart, uint32_t codeSize,
								  uint8_t* dataStart, uint32_t dataSize);
    void *getMethodCode(const char *methodName);
    void relocateRegisteredMethod(const char *methodName);

    void setOldNewAddressesMap(const std::map<::SOMCacheMetadataItemHeader, ::AbstractVMObject*>* map);
    void setReverseLookupMap(const std::map<::AbstractVMObject*, ::AbstractVMObject*>* map);

    TR::SharedCache* getSharedCache();  

 private:    
    void storeAOTMethodAndDataInTheCache(const char *methodName);    
    TR::AOTMethodHeader* loadAOTMethodAndDataFromTheCache(const char *methodName);    
    TR::AOTMethodHeader* getRegisteredAOTMethodHeader(const char * methodName);

    TR::SharedCacheRelocationRuntime* _reloRuntime;
    TR::CodeCacheManager*    _codeCacheManager;
    TR::SharedCache* _sharedCache;
    //    TR::CompilerEnv* _compiler;
    std::map<std::string, TR::AOTMethodHeader*> _methodNameToHeaderMap;
    TR::CodeCache* _cacheInUse;
};
}
#endif // !defined(OMR_AOTADAPTER_INCL)
