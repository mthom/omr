#if !defined(DATA_SECTION_ENTRY_ITERATOR_HPP_INCLUDED)
#define DATA_SECTION_ENTRY_ITERATOR_HPP_INCLUDED

#include "OSCacheBumpRegionFocus.hpp"

#include "SOMCacheEntry.hpp"

struct SOMCacheEntryDescriptor {
  SOMCacheEntryDescriptor()
    : entry(NULL)
    , codeLocation(NULL)
  {}

  SOMCacheEntryDescriptor(SOMCacheEntry* entry)
    : entry(entry)
    , codeLocation(entry + sizeof(SOMCacheEntry))
    , relocationRecordSize(0)
  {}

  inline bool operator ==(const SOMCacheEntryDescriptor& rhs) const {
    return entry == rhs.entry && codeLocation == rhs.codeLocation;
  }

  operator bool() const;
  
  SOMCacheEntry* entry;
  void* codeLocation;
  uint32_t relocationRecordSize;
};

static const SOMCacheEntryDescriptor nullCacheEntryDescriptor;

class SOMDataSectionEntryIterator
{
public:
  SOMDataSectionEntryIterator(OSCacheBumpRegionFocus<SOMCacheEntry> focus,
			       OSCacheBumpRegionFocus<SOMCacheEntry> limit)
    : _focus(focus)
    , _limit(limit)
  {}

  SOMCacheEntryDescriptor next() {
    SOMCacheEntry* entry = _focus;

    if(entry == NULL || entry->codeLength == 0 || !(_focus < _limit)) {
      return nullCacheEntryDescriptor;
    }

    SOMCacheEntryDescriptor descriptor(_focus++);
    _focus += entry->codeLength;

    return descriptor;
  }

protected:  
  OSCacheBumpRegionFocus<SOMCacheEntry> _focus;
  const OSCacheBumpRegionFocus<SOMCacheEntry> _limit;
};

#endif
