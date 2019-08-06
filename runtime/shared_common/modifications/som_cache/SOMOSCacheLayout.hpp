#if !defined(SOM_OSCACHE_LAYOUT_HPP_INCLUDED)
#define SOM_OSCACHE_LAYOUT_HPP_INCLUDED

#include "OSCacheLayout.hpp"
#include "OSCacheContiguousRegion.hpp"
#include "SOMOSCacheHeader.hpp"

template <class OSCacheHeader>
class SOMOSCacheLayout: public OSCacheLayout
{
public:
  SOMOSCacheLayout(UDATA osPageSize, bool pageBoundaryAligned)
    : OSCacheLayout(osPageSize)
    , _blockSize(0)
    , _header(0, pageBoundaryAligned)
    , _dataSection(nullptr, 1, pageBoundaryAligned)
  {
    // first two arguments are the OSCacheLayout* and the region ID.
    //_header = ::new SOMOSCacheHeader<OSCacheHeader>(this, 0, pageBoundaryAligned);
    //_dataSection = ::new OSCacheContiguousRegion((OSCacheLayout*) this, 1, pageBoundaryAligned);

    _header.setCacheLayout(this);
    _dataSection.setCacheLayout(this);
	
    addRegion(&_header);
    addRegion(&_dataSection);
  }

  virtual ~SOMOSCacheLayout() {
    this->_regions.clear();
    
//    delete _header;
//    delete _dataSection;
  }

  // once the cache is attached to, is the data well-formed?
  // depending on page boundary alignment, and possibly other factors, the effective
  // cache size may differ from the block size.
  UDATA effectiveCacheSize() {
    return _header.regionSize() + _dataSection.regionSize();
  }

  UDATA actualCacheSize() {
    return _blockSize;
  }
    
  // regions in this layout cannot adjust their sizes, so just say
  // it passed.
  bool notifyRegionSizeAdjustment(OSCacheRegion&) override {
    return true;
  }

protected:
  friend class SOMOSCacheConfig<typename OSCacheHeader::config_type>;

  void init(void* blockAddress, uintptr_t size) override
  {
    _header.adjustRegionStartAndSize(blockAddress, _header.regionSize());
    _dataSection.adjustRegionStartAndSize((void*) ((UDATA) blockAddress + _header.regionSize()),
					   size - _header.regionSize());
  
    _header.alignToPageBoundary(_osPageSize);
    _dataSection.alignToPageBoundary(_osPageSize);

    _blockSize = size;  
  }
  
  inline void clearRegions() {
    _regions.clear();
  }
  
  UDATA _blockSize;
  SOMOSCacheHeader<OSCacheHeader> _header;
  OSCacheContiguousRegion _dataSection;
};

#endif
