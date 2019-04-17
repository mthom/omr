#if !defined(DATA_SECTION_ENTRY_ITERATOR_HPP_INCLUDED)
#define DATA_SECTION_ENTRY_ITERATOR_HPP_INCLUDED

#include "OSCacheBumpRegionFocus.hpp"

#include "WASMCacheEntry.hpp"

struct WASMCacheEntryDescriptor {
  WASMCacheEntryDescriptor()
    : entry(NULL)
    , codeLocation(NULL)
  {}

  WASMCacheEntryDescriptor(WASMCacheEntry* entry)
    : entry(entry)
    , codeLocation(entry + sizeof(WASMCacheEntry))
  {}

  inline bool atEnd() const {
    return codeLocation == NULL;
  }

  WASMCacheEntry* entry;
  void* codeLocation;
};

static WASMCacheEntryDescriptor nullCacheEntryDescriptor;

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

    WASMCacheEntryDescriptor descriptor(_focus++);
    _focus += entry->codeLength;

    return descriptor;
  }

protected:
  OSCacheBumpRegionFocus<WASMCacheEntry> _focus;
  const OSCacheBumpRegionFocus<WASMCacheEntry> _limit;
};

#endif
