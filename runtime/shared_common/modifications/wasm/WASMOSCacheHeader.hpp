#if !defined(WASM_OS_CACHE_HEADER_HPP_INCLUDED)
#define WASM_OS_CACHE_HEADER_HPP_INCLUDED

#include "CacheHeaderImpl.hpp"
#include "OSCacheContiguousRegion.hpp"

// this is meant to augment an existing header, like
// OSMemoryMappedCacheHeader, or OSSharedMemoryCacheHeader.  also,
// it's supposed to be a region of the cache! hence the virtual base
// class of OSCacheContiguousRegion.
template <class OSCacheHeader>
class WASMOSCacheHeader: public OSCacheHeader, virtual public OSCacheContiguousRegion
{
public:
  TR_ALLOC(TR::SharedCacheRegion)

  //TODO: needs template specializations.
  WASMOSCacheHeader(OSCacheLayout* layout, int regionID, bool pageBoundaryAligned);

protected:
  CacheHeaderImpl<OSCacheHeader>* _mapping;
};

#endif
