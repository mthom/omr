#if !defined(WASM_OS_CACHE_HEADER_HPP_INCLUDED)
#define WASM_OS_CACHE_HEADER_HPP_INCLUDED

// this is meant to augment an existing header, like
// OSMemoryMappedCacheHeader, or OSSharedMemoryCacheHeader.  also,
// it's a region of the cache! but then, the OSCacheHeader might be
// too, hence the virtual base class of OSCacheRegion.  OSCacheRegion
// doesn't currently contain state, so the virtual designator isn't
// necessary *now*, but who knows, that might change.
template <class OSCacheHeader>
class WASMOSCacheHeader: public OSCacheHeader, virtual public OSCacheRegion
{
public:
  
};

#endif
