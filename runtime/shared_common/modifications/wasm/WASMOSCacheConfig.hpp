#if !defined(WASM_OS_CACHE_LAYOUT_HPP_INCLUDED)
#define WASM_OS_CACHE_LAYOUT_HPP_INCLUDED

#include "OSCacheConfig.hpp"


#include "WASMOSCacheHeader.hpp"
#include "WASMOSCacheLayout.hpp"

#include "env/TRMemory.hpp"

// this is meant to augment an existing header, like
// OSMemoryMappedCacheHeader, or OSSharedMemoryCacheHeader.  also,
// it's a region of the cache! but then, the OSCacheHeader might be
// too, hence the virtual base class of OSCacheRegion.  OSCacheRegion
// doesn't currently contain state, so the virtual designator isn't
// necessary *now*, but who knows, that might change.
template <typename OSCacheConfigImpl>
class WASMOSCacheConfig: public OSCacheConfigImpl
{
public:
  TR_ALLOC(TR_Memory::SharedCacheConfig)

  typedef typename OSCacheConfigImpl::header_type header_type;

  WASMOSCacheConfig(U_32 numLocks, UDATA osPageSize, UDATA dataSectionSize)
    : OSCacheConfigImpl(numLocks)
  {
    _layout = new WASMOSCacheLayout(osPageSize, dataSectionSize);    
  }

  virtual bool initHeader() {
    _header = new WASMOSCacheHeader<typename header_type>(_layout, 0); // 0 == header region ID.
    return _header->isValid();
  }
  
protected:
  
};

#endif
