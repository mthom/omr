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
    , _metadataSection(nullptr, 2, pageBoundaryAligned)
  {
    // first two arguments are this OSCacheLayout* and the region ID.
    //_header = ::new SOMOSCacheHeader<OSCacheHeader>(this, 0, pageBoundaryAligned);
    //_dataSection = ::new OSCacheContiguousRegion((OSCacheLayout*) this, 1, pageBoundaryAligned);

    _header.setCacheLayout(this);
    _dataSection.setCacheLayout(this);
    _metadataSection.setCacheLayout(this);
	
    addRegion(&_header);
    addRegion(&_dataSection);
    addRegion(&_metadataSection);
  }

  virtual ~SOMOSCacheLayout() {}

  // once the cache is attached to, is the data well-formed?
  // depending on page boundary alignment, and possibly other factors, the effective
  // cache size may differ from the block size.
  UDATA effectiveCacheSize() {
    return _header.regionSize() + _dataSection.regionSize() + _metadataSection.regionSize();
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
    UDATA frontier = (UDATA) blockAddress;

    _header.adjustRegionStartAndSize((void*) frontier, _header.regionSize());
    frontier += _header.regionSize();

    auto dataSectionSize = (size - _header.regionSize()) / 2;

    _dataSection.adjustRegionStartAndSize((void*) frontier, dataSectionSize);
    frontier += _dataSection.regionSize();

    auto metadataSectionSize = size - dataSectionSize - _header.regionSize();
    
    _metadataSection.adjustRegionStartAndSize((void*) frontier, metadataSectionSize);
  
    _header.alignToPageBoundary(_osPageSize);
    _dataSection.alignToPageBoundary(_osPageSize);
    _metadataSection.alignToPageBoundary(_osPageSize);

    _blockSize = size;  
  }
  
  inline void clearRegions() {
     _regions.clear();
  }
  
  UDATA _blockSize;

  SOMOSCacheHeader<OSCacheHeader> _header;
  OSCacheContiguousRegion _dataSection;
  OSCacheContiguousRegion _metadataSection;
};

#endif
