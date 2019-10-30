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

#include "jitbuilder/runtime/CodeCacheManager.hpp"
#include "env/CompilerEnv.hpp"
#include "env/SharedCache.hpp"
#include "runtime/CodeCache.hpp"
#include "runtime/RelocationRuntime.hpp"

#include <iostream>
#include <map>

TR::AotAdapter *
OMR::AotAdapter::self()
   {
   return static_cast<TR::AotAdapter *>(this);
   }

void OMR::AotAdapter::initializeAOTClasses(TR::RawAllocator* rawAllocator, TR::CodeCacheManager* cc)
{
  _sharedCache = new (PERSISTENT_NEW) TR::SharedCache("som_shared_cache", "/tmp");
  _reloRuntime = new (PERSISTENT_NEW) TR::SharedCacheRelocationRuntime(NULL, cc);
  _codeCacheManager = cc;
}

void OMR::AotAdapter::setOldNewAddressesMap(const std::map<::SOMCacheMetadataItemHeader, ::AbstractVMObject*>* map)
{
  auto *rr = *reinterpret_cast<TR::SharedCacheRelocationRuntime**>(reinterpret_cast<U_8*>(&_reloRuntime) + 0x10);
  rr->setOldNewAddressesMap(map);
}

void OMR::AotAdapter::setReverseLookupMap(std::shared_ptr<std::map<::AbstractVMObject*, ::AbstractVMObject*>>& map)
{
  auto *rr = *reinterpret_cast<TR::SharedCacheRelocationRuntime**>(reinterpret_cast<U_8*>(&_reloRuntime) + 0x10);
  rr->setReverseLookupMap(map);
}

std::shared_ptr<std::map<AbstractVMObject*, AbstractVMObject*>>&
OMR::AotAdapter::getReverseLookupMap()
{
  auto *rr = *reinterpret_cast<TR::SharedCacheRelocationRuntime**>(reinterpret_cast<U_8*>(&_reloRuntime) + 0x10);
  return rr->getReverseLookupMap();
}

::AbstractVMObject* OMR::AotAdapter::reverseLookup(::AbstractVMObject* obj)
{
  auto *rr = *reinterpret_cast<TR::SharedCacheRelocationRuntime**>(reinterpret_cast<U_8*>(&_reloRuntime) + 0x10);
  return rr->reverseLookup(obj);
}

TR::SharedCache* OMR::AotAdapter::getSharedCache() {
   // WTF? Fix this! >:O
   return *reinterpret_cast<TR::SharedCache**>(reinterpret_cast<U_8*>(&_sharedCache) + 0x10);
}

void OMR::AotAdapter::storeExternalSymbol(const char *symbolName, void* symbolAddress)
{
    _reloRuntime->registerLoadedSymbol(symbolName,symbolAddress);
}

// TODO: how is this different from the AOTMethodHeader constructor?
// and why is malloc being used? set up TR_ALLOC inside
// AOTMethodHeader, use placement new along with the constructor.
void* OMR::AOTMethodHeader::serialize()
{
    uintptrj_t allocSize = this->sizeOfSerializedVersion();
    uint8_t* buffer = (uint8_t*) malloc(allocSize);
    uint8_t* ptr = buffer;

    memcpy(ptr,&allocSize,sizeof(uintptrj_t));
    ptr+=sizeof(uintptrj_t);

    memcpy(ptr,&this->compiledCodeSize,sizeof(uint32_t));
    ptr+=sizeof(uint32_t);   

    memcpy(ptr,this->compiledCodeStart,this->compiledCodeSize);
    ptr+=this->compiledCodeSize;

    memcpy(ptr,&this->relocationsSize,sizeof(uint32_t));
    ptr+=sizeof(uint32_t);   

    memcpy(ptr,this->relocationsStart,this->relocationsSize);
    ptr+=this->relocationsSize;

    return buffer;
}

OMR::AOTMethodHeader::AOTMethodHeader(uint8_t* rawData)
{
    // uintptrj_t sizeOfHeader = (uintptrj_t) rawData;
    // Skip the header size
    rawData+=sizeof(uintptrj_t);
    this->compiledCodeSize = *((uint32_t*) rawData);

    rawData+=sizeof(uint32_t);
    this->compiledCodeStart=rawData;

    rawData+=compiledCodeSize;
    this->relocationsSize = *((uint32_t*) rawData); 

    rawData+=sizeof(uint32_t);
    this->relocationsStart=rawData;   

}

uintptrj_t OMR::AOTMethodHeader::sizeOfSerializedVersion(){
    return sizeof(uintptrj_t) +2*sizeof(uint8_t*)+2*sizeof(uint32_t)+this->compiledCodeSize+this->relocationsSize;
}

void OMR::AotAdapter::storeAOTMethodAndDataInTheCache(const char* methodName)
{
    TR::AOTMethodHeader* hdr =_methodNameToHeaderMap[methodName];
    _sharedCache->storeEntry(methodName, hdr->serialize(), hdr->sizeOfSerializedVersion());
}

TR::AOTMethodHeader* OMR::AotAdapter::loadAOTMethodAndDataFromTheCache(const char* methodName)
{
    TR::AOTMethodHeader* methodHeader = NULL;
    void* cacheEntry = reinterpret_cast<uint8_t*> (_sharedCache->loadEntry(methodName));
    if (cacheEntry!=NULL){
        methodHeader = new TR::AOTMethodHeader((uint8_t*)cacheEntry);
        void* codeStart = methodHeader->compiledCodeStart;
        _reloRuntime->registerLoadedSymbol(methodName, codeStart);
        registerAOTMethodHeader(methodName,methodHeader);
    }

    return methodHeader;
}

void OMR::AotAdapter::registerAOTMethodHeader(std::string methodName,TR::AOTMethodHeader* header) {
    _methodNameToHeaderMap[methodName] = header;
}

TR::RelocationRuntime* OMR::AotAdapter::rr(){
    return _reloRuntime->self();
}

TR::AOTMethodHeader*
OMR::AotAdapter::createAndRegisterAOTMethodHeader(const char* methodName, uint8_t* codeStart, uint32_t codeSize,
						  uint8_t* dataStart, uint32_t dataSize)
{
     TR::AOTMethodHeader *hdr = new TR::AOTMethodHeader(codeStart, codeSize, dataStart, dataSize);
     registerAOTMethodHeader(methodName, hdr);
     return hdr;
}

void OMR::AotAdapter::relocateRegisteredMethod(const char* methodName)
{
   TR::AOTMethodHeader* hdr = getRegisteredAOTMethodHeader(methodName);
    _reloRuntime->self()->prepareRelocateAOTCodeAndData(hdr, hdr->compiledCodeStart);
}

TR::AOTMethodHeader* OMR::AotAdapter::getRegisteredAOTMethodHeader(const char* methodName)
{
   std::string method(methodName);
   TR::AOTMethodHeader* result =_methodNameToHeaderMap[methodName];

   if (result==NULL)
     {
       result = loadAOTMethodAndDataFromTheCache(methodName);
     }

   return result;
}

/* This function was redefined because the original (below) was trying
   to do too much. If the method isn't found in the map, it isn't our
   problem. This class is meant to be a convenience, not a nanny
   state.  Also, 'loadAOTMethodAndDataFromTheCache' was removed
   because it's the job of the shared cache to load AOT data, not the
   AOT adapter.

TR::AOTMethodHeader* OMR::AotAdapter::getRegisteredAOTMethodHeader(const char* methodName){
    std::string method(methodName);
    TR::AOTMethodHeader* result =_methodNameToHeaderMap[methodName];

    if (result==NULL)
        {
        result = loadAOTMethodAndDataFromTheCache(methodName);
        }

    return result;
 }
*/

void OMR::AotAdapter::storeHeaderForCompiledMethod(const char* methodName)
{
    std::string method(methodName);

    if (_methodNameToHeaderMap[methodName] != NULL)
    {
       storeAOTMethodAndDataInTheCache(methodName);
    } else {
       std::cerr << "Method " << methodName << " not found!" << std::endl;
    }
}

bool isMethodAllocatedAlready(void* pc){
    return false;
}

void* OMR::AotAdapter::getMethodCode(const char* methodName)
    {
    TR::AOTMethodHeader* methodHeader = getRegisteredAOTMethodHeader(methodName);
    if (methodHeader == NULL)
        return NULL;
    if (false == isMethodAllocatedAlready(methodHeader->compiledCodeStart))
        {
        int32_t numReserved;
        static TR::CodeCache *codeCache = _codeCacheManager->reserveCodeCache(false, methodHeader->compiledCodeSize, 0, &numReserved);
        if(!codeCache)
            {
            return nullptr;
            }

        uint32_t  codeLength=methodHeader->compiledCodeSize;
        uint8_t * coldCode = nullptr;
        void * warmCode =  _codeCacheManager->allocateCodeMemory(codeLength, 0, &codeCache, &coldCode, false);

        if (!warmCode)
	   {
	     codeCache->unreserve();
	     return nullptr;
	   }

        memcpy(warmCode,methodHeader->compiledCodeStart,codeLength);
        methodHeader->compiledCodeStart=(uint8_t*)warmCode;
        _reloRuntime->registerLoadedSymbol(methodName,warmCode);

        return warmCode;
        } 
        else // isMethodAllocatedAlready
        {
            void* warmCode= methodHeader->compiledCodeStart;
            _reloRuntime->registerLoadedSymbol(methodName,warmCode);
            return methodHeader->compiledCodeStart;
         }
    }
