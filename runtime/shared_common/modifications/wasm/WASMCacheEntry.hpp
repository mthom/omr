#if !defined(WASM_CACHE_ENTRY_HPP_INCLUDED)
#define WASM_CACHE_ENTRY_HPP_INCLUDED

#include <string>

#include "omrport.h"

#define WASM_METHOD_NAME_MAX_LEN 128

struct WASMCacheEntry {
  WASMCacheEntry(const char* methodSignature, U_32 codeLength)
    : codeLength(codeLength)
  {
    strncpy(methodName, methodSignature, WASM_METHOD_NAME_MAX_LEN);
  }

  char methodName[WASM_METHOD_NAME_MAX_LEN];
  U_32 codeLength;  
};

#endif
