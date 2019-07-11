#if !defined(DATA_SECTION_ENTRY_ITERATOR_HPP_INCLUDED)
#define DATA_SECTION_ENTRY_ITERATOR_HPP_INCLUDED

#include "OSCacheBumpRegionFocus.hpp"

#include "WASMCacheEntry.hpp"

struct WASMCacheEntryDescriptor {
  WASMCacheEntryDescriptor()
    : entry(NULL)
    , codeLocation(NULL)
    , relocationRecordSize(0)
  {}

  WASMCacheEntryDescriptor(WASMCacheEntry* entry)
    : entry(entry)
    , codeLocation(entry + sizeof(WASMCacheEntry))
    , relocationRecordSize(0)
  {}

  inline bool operator ==(const WASMCacheEntryDescriptor& rhs) const {
    return entry == rhs.entry && codeLocation == rhs.codeLocation;
  }

  operator bool() const;
  
  WASMCacheEntry* entry;
  void* codeLocation;
  uint32_t relocationRecordSize;
};

static const WASMCacheEntryDescriptor nullCacheEntryDescriptor;

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

    if(entry == NULL || entry->codeLength == 0 || !(_focus < _limit)) {
      return nullCacheEntryDescriptor;
    }

    WASMCacheEntryDescriptor descriptor(_focus++);
    _focus += entry->codeLength;
    uint32_t relocationRecordSize = 0;
    memcpy(&relocationRecordSize,&(*_focus),4);
    relocationRecordSize+=4;
    descriptor.relocationRecordSize = relocationRecordSize;
    _focus += relocationRecordSize;

    return descriptor;
  }

protected:  
  OSCacheBumpRegionFocus<WASMCacheEntry> _focus;
  const OSCacheBumpRegionFocus<WASMCacheEntry> _limit;
};

#endif
