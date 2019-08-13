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

#include "env/OMRAotAdapter.hpp"
#include "env/CompilerEnv.hpp"
#include "runtime/CodeCache.hpp"
#include <iostream>
#include "runtime/RelocationRuntime.hpp"

 void OMR::AotAdapter::initializeAOTClasses(TR::RawAllocator* rawAllocator,TR::CodeCacheManager* cc){
  _sharedCache = new (PERSISTENT_NEW) TR::SharedCache("wasm_shared_cache", "/tmp");
  _reloRuntime = new (PERSISTENT_NEW) TR::SharedCacheRelocationRuntime (NULL,cc);
  _codeCacheManager = cc;
  TR::Compiler->cache = _sharedCache;
 }
void OMR::AotAdapter::storeExternalSymbol(const char *symbolName, void* symbolAddress){
    _reloRuntime->registerLoadedSymbol(symbolName,symbolAddress);
}
void* TR::AOTMethodHeader::serialize(){
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
TR::AOTMethodHeader::AOTMethodHeader(uint8_t* rawData){
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

uintptrj_t TR::AOTMethodHeader::sizeOfSerializedVersion(){
    return sizeof(uintptrj_t) +2*sizeof(uint8_t*)+2*sizeof(uint32_t)+this->compiledCodeSize+this->relocationsSize;
}


void OMR::AotAdapter::storeAOTMethodAndDataInTheCache(const char* methodName){
    TR::AOTMethodHeader* hdr =_methodNameToHeaderMap[_lastMethodIdentifier];
    _sharedCache->storeEntry(methodName,hdr->serialize(),hdr->sizeOfSerializedVersion());
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
void OMR::AotAdapter::registerAOTMethodHeader(std::string methodName,TR::AOTMethodHeader* header){
    _methodNameToHeaderMap[methodName] = header;
}

TR::RelocationRuntime* OMR::AotAdapter::rr(){
    return _reloRuntime->self();
}

void OMR::AotAdapter::createAOTMethodHeader(uint8_t* codeStart, uint32_t codeSize,uint8_t* dataStart, uint32_t dataSize){
     TR::AOTMethodHeader* hdr = (TR::AOTMethodHeader*)new (TR::AOTMethodHeader) (codeStart,codeSize,dataStart,dataSize);
     registerAOTMethodHeader(_lastMethodIdentifier,hdr );
}

void OMR::AotAdapter::relocateMethod(const char* methodName){
    TR::AOTMethodHeader* hdr =  getRegisteredAOTMethodHeader(methodName);
    _reloRuntime->self()->prepareRelocateAOTCodeAndData(hdr,hdr->compiledCodeStart);
}
 TR::AOTMethodHeader* OMR::AotAdapter::getRegisteredAOTMethodHeader(const char* methodName){
    std::string method(methodName);
    TR::AOTMethodHeader* result =_methodNameToHeaderMap[methodName];
    if (result==NULL)
        {
        result = loadAOTMethodAndDataFromTheCache(methodName);
        }
    return result;
 }
void OMR::AotAdapter::storeHeaderForLastCompiledMethodUnderName(const char* methodName){
    std::string method(methodName);
    if ( _methodNameToHeaderMap[_lastMethodIdentifier ] != NULL){
        _methodNameToHeaderMap[method]=_methodNameToHeaderMap[_lastMethodIdentifier ];
        storeAOTMethodAndDataInTheCache(methodName);
        _methodNameToHeaderMap[_lastMethodIdentifier] = NULL;
    }else
    {
    //   std::cerr<<"Last method not found!"<<std::endl;
    }
}

void* OMR::AotAdapter::getMethodCode(const char* methodName){
    TR::AOTMethodHeader* methodHeader = getRegisteredAOTMethodHeader(methodName);
    if (methodHeader == NULL)
        return NULL;
    TR::CodeCache *codeCache =_codeCacheManager->findCodeCacheFromPC(methodHeader->compiledCodeStart);
    if (codeCache==NULL)
        {
        int32_t numReserved;
        codeCache = _codeCacheManager->reserveCodeCache(false, methodHeader->compiledCodeSize, 0, &numReserved);
        if(!codeCache)
            {
            return nullptr;
            }
        uint32_t  codeLength=methodHeader->compiledCodeSize;
        uint8_t * coldCode = nullptr;
        void * warmCode =  _codeCacheManager->allocateCodeMemory(codeLength, 0, &codeCache, &coldCode, false);
        if(!warmCode){
            codeCache->unreserve();
            return nullptr;
        }
        memcpy(warmCode,methodHeader->compiledCodeStart,codeLength);
        methodHeader->compiledCodeStart=(uint8_t*)warmCode;
        _reloRuntime->registerLoadedSymbol(methodName,warmCode);

        return warmCode;
        }
    void* warmCode= methodHeader->compiledCodeStart;
    _reloRuntime->registerLoadedSymbol(methodName,warmCode);
    return methodHeader->compiledCodeStart;
}
