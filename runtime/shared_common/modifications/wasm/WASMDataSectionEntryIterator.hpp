#if !defined(DATA_SECTION_ENTRY_ITERATOR_HPP_INCLUDED)
#define DATA_SECTION_ENTRY_ITERATOR_HPP_INCLUDED

#include "OSCacheBumpRegionFocus.hpp"

#include "WASMCacheEntry.hpp"

struct WASMCacheEntryDescriptor {
  WASMCacheEntryDescriptor(const char* methodSignature, void* codeLocation, U_32 codeLength)
    : codeLocation(codeLocation)
    , codeLength(codeLength)
  {
    strncpy(methodName, methodSignature, WASM_METHOD_NAME_MAX_LEN);
  }

  char methodName[WASM_METHOD_NAME_MAX_LEN];
  void* codeLocation;
  U_32 codeLength;

  inline bool atEnd() const {
    return codeLocation == NULL;
  }
};

static WASMCacheEntryDescriptor nullCacheEntryDescriptor("", NULL, 0);

class WASMDataSectionEntryIterator
{
public:
  WASMDataSectionEntryIterator(OSCacheBumpRegionFocus<WASMCacheEntry> focus,
			       OSCacheBumpRegionFocus<WASMCacheEntry> limit)
    : _focus(focus)
    , _limit(limit)
  {}
  
  WASMCacheEntryDescriptor next() {
    WASMCacheEntry* entry = _focus;

    if(entry == NULL || !(_focus < _limit)) {
      return nullCacheEntryDescriptor;
    }

    WASMCacheEntryDescriptor descriptor(entry->methodName, _focus++, entry->codeLength);
    _focus += entry->codeLength;

    return descriptor;
  }
    
protected:
  OSCacheBumpRegionFocus<WASMCacheEntry> _focus;
  const OSCacheBumpRegionFocus<WASMCacheEntry> _limit;
};

#endif
