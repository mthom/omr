#if !defined(WASM_OS_CACHE_HEADER_HPP_INCLUDED)
#define WASM_OS_CACHE_HEADER_HPP_INCLUDED

#include "CacheHeaderMappingImpl.hpp"
#include "OSCacheContiguousRegion.hpp"

#include "env/TRMemory.hpp"

template <class OSCacheHeader>
class WASMOSCacheLayout;

// this is meant to augment an existing header, like
// OSMemoryMappedCacheHeader, or OSSharedMemoryCacheHeader.  also,
// it's supposed to be a region of the cache! hence the virtual base
// class of OSCacheContiguousRegion.
template <class OSCacheHeader>
class WASMOSCacheHeader: public OSCacheHeader, virtual public OSCacheContiguousRegion
{
public:
  TR_ALLOC(TR_Memory::SharedCacheRegion)

  WASMOSCacheHeader(WASMOSCacheLayout<OSCacheHeader>* layout, int regionID, bool pageBoundaryAligned)
    : OSCacheContiguousRegion(layout, regionID, pageBoundaryAligned)
  {}
};

#endif
