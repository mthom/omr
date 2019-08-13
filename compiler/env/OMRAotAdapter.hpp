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
#include "jitbuilder/runtime/CodeCacheManager.hpp"
#include "runtime/Runtime.hpp"
#include <map>
#include <string>
#ifndef OMR_AOTADAPTER_CONNECTOR
#define OMR_AOTADAPTER_CONNECTOR
namespace OMR { class AotAdapter; }
namespace OMR { typedef OMR::AotAdapter AotAdapterConnector; }
#endif
namespace TR 
    {
    class AotAdapter;
    class SharedCacheRelocationRuntime;
    class SharedCache;
    class CodeCache;
    class RelocationRuntime;
    class CompilerEnv;
    }
namespace OMR
{


class OMR_EXTENSIBLE AotAdapter{
public:
    AotAdapter(){};
    TR::AotAdapter* self();
    TR::RelocationRuntime* rr();
    void initializeAOTClasses(TR::RawAllocator* allocator, TR::CodeCacheManager* CodeCacheManager);
    void storeExternalSymbol(const char *symbolName, void* symbolAddress);
    void storeHeaderForLastCompiledMethodUnderName(const char *methodName);
    void createAOTMethodHeader(uint8_t* codeStart, uint32_t codeSize,uint8_t* dataStart, uint32_t dataSize);
    void *getMethodCode(const char *methodName);
    void relocateMethod(const char *methodName);

 private:
    
    void storeAOTMethodAndDataInTheCache(const char *methodName);
    void registerAOTMethodHeader(std::string methodName,TR::AOTMethodHeader* hdr);
    TR::AOTMethodHeader* loadAOTMethodAndDataFromTheCache(const char *methodName);
    TR::AOTMethodHeader* getRegisteredAOTMethodHeader(const char * methodName);
  
    TR::SharedCache* _sharedCache;
    TR::SharedCacheRelocationRuntime* _reloRuntime;
    TR::CodeCacheManager*    _codeCacheManager;
    TR::CompilerEnv* _compiler;
    std::map<std::string,TR::AOTMethodHeader*> _methodNameToHeaderMap;
    std::string _lastMethodIdentifier = "LastMethod";
   
};
}
#endif // !defined(OMR_AOTADAPTER_INCL)
