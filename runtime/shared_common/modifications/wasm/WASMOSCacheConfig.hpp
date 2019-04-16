#if !defined(WASM_OS_CACHE_CONFIG_HPP_INCLUDED)
#define WASM_OS_CACHE_CONFIG_HPP_INCLUDED

#include "WASMOSCacheLayout.hpp"

#include "env/TRMemory.hpp"

// this is meant to augment an existing header, like
// OSMemoryMappedCacheHeader, or OSSharedMemoryCacheHeader.  also,
// it's a region of the cache! but then, the OSCacheHeader might be
// too, hence the virtual base class of OSCacheRegion.  OSCacheRegion
// doesn't currently contain state, so the virtual designator isn't
// necessary *now*, but who knows, that might change.
template <class OSCacheConfigImpl>
class WASMOSCacheConfig: public OSCacheConfigImpl
{
public:
  TR_ALLOC(TR_Memory::SharedCacheConfig)

  typedef typename OSCacheConfigImpl::header_type header_type;

  WASMOSCacheConfig(U_32 numLocks, UDATA osPageSize)
    : OSCacheConfigImpl(numLocks)
    , _layout(new WASMOSCacheLayout<header_type>(osPageSize, osPageSize > 0))
  {}

  J9SRP* getDataSectionLocation() override;
  U_64 getDataSectionSize() override;

  virtual void serializeCacheLayout(OSCache* osCache, void* blockAddress, U_32 cacheSize);
  virtual void initializeCacheLayout(OSCache* osCache, void* blockAddress, U_32 cacheSize);
  
private:
  WASMOSCacheLayout<header_type> _layout;
};

#endif
