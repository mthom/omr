#if !defined(SOM_CACHE_ENTRY_HPP_INCLUDED)
#define SOM_CACHE_ENTRY_HPP_INCLUDED

#include <string>

#include "omrport.h"

#define SOM_METHOD_NAME_MAX_LEN 128

struct SOMCacheEntry {
  SOMCacheEntry(const char* methodSignature, U_32 codeLength)
    : codeLength(codeLength)
  {
    strncpy(methodName, methodSignature, SOM_METHOD_NAME_MAX_LEN);
  }

  char methodName[SOM_METHOD_NAME_MAX_LEN];
  U_32 codeLength;  
};

#endif
