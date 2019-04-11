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
  WASMOSCacheLayout(UDATA osPageSize, bool pageBoundaryAligned)
    : _blockSize(0)
    , OSCacheLayout(osPageSize)
  {
    // first two arguments are the OSCacheLayout* and the region ID.
    _header = new WASMOSCacheHeader<SuperOSCacheHeader>(this, 0, pageBoundaryAligned);
    _dataSection = new OSCacheContiguousRegion(this, 1, _header->regionSize(), pageBoundaryAligned);

    addRegion(_header);
    addRegion(_dataSection);
  }

  void init(void* blockAddress, uintptr_t size);

  // once the cache is attached to, is the data well-formed?
  bool isValid();

  // depending on page boundary alignment, and possibly other factors, the effective
  // cache size may differ from the block size.
  UDATA effectiveCacheSize() {
    return _header->regionSize() + _dataSection->regionSize();
  }

  UDATA actualCacheSize() {
    return _blockSize;
  }

protected:
  UDATA _blockSize;
  WASMOSCacheHeader* _header;
  OSCacheContiguousRegion* _dataSection;
};

#endif
