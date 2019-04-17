#if !defined(WASM_OSCACHE_LAYOUT_HPP_INCLUDED)
#define WASM_OSCACHE_LAYOUT_HPP_INCLUDED

#include "OSCacheLayout.hpp"
#include "OSCacheContiguousRegion.hpp"
#include "WASMOSCacheHeader.hpp"

#include "env/TRMemory.hpp"

template <class OSCacheHeader>
class WASMOSCacheLayout: public OSCacheLayout
{
public:
  TR_ALLOC(TR_Memory::SharedCacheLayout)

  WASMOSCacheLayout(UDATA osPageSize, bool pageBoundaryAligned)
    : _blockSize(0)
    , OSCacheLayout(osPageSize)
  {
    // first two arguments are the OSCacheLayout* and the region ID.
    _header = new WASMOSCacheHeader<OSCacheHeader>(this, 0, pageBoundaryAligned);
    _dataSection = new OSCacheContiguousRegion(this, 1, _header->regionSize(), pageBoundaryAligned);

    addRegion(_header);
    addRegion(_dataSection);
  }

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
    
  void init(void* blockAddress, uintptr_t size);

protected:
  UDATA _blockSize;
  WASMOSCacheHeader<OSCacheHeader>* _header;
  OSCacheContiguousRegion* _dataSection;
};

#endif
