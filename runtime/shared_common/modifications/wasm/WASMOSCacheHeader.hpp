#if !defined(WASM_OS_CACHE_HEADER_HPP_INCLUDED)
#define WASM_OS_CACHE_HEADER_HPP_INCLUDED

#include "OSCacheContiguousRegion.hpp"

// this is meant to augment an existing header, like
// OSMemoryMappedCacheHeader, or OSSharedMemoryCacheHeader.  also,
// it's a region of the cache! but then, the OSCacheHeader might be
// too, hence the virtual base class of OSCacheContiguousRegion.
template <class OSCacheHeader>
class WASMOSCacheHeader: public OSCacheHeader, virtual public OSCacheContiguousRegion
{
public:
  TR_ALLOC(WASMOSCacheHeader<OSCacheHeader>)
  
  WASMOSCacheHeader(OSCacheLayout* layout, int regionID)
    : OSCacheContiguousRegion(layout, regionID, NULL, sizeof(WASMOSCacheHeader<OSCacheHeader>), true)
  {}
  
  volatile UDATA* _readerCount;
  UDATA* _cacheCrc;
};

#endif
