#if !defined(WASM_OS_CACHE_LAYOUT_HPP_INCLUDED)
#define WASM_OS_CACHE_LAYOUT_HPP_INCLUDED

#include "WASMOSCacheHeader.hpp"

#include "env/TRMemory.hpp"

class WASMOSCacheLayout: public OSCacheLayout
{
public:
  TR_ALLOC(TR_Memory::SharedCacheLayout)
  
  // the template argument is the templated superclass of the WASMOSCacheHeader.
  template <typename SuperOSCacheHeader>
  WASMOSCacheLayout(UDATA osPageSize, UDATA dataSectionSize)
    : OSCacheLayout(osPageSize)
  {
    // first two arguments are the OSCacheLayout* and the region ID.
    addRegion(new WASMOSCacheHeader<SuperOSCacheHeader>(this, 0));
    addRegion(new OSCacheContiguousRegion(this, 1, NULL, dataSectionSize, true));
  }

  // once the cache is attached to, is the data well-formed?
  bool isValid();
  // return true on success, false on failure.
  bool initialize();
};

#endif
