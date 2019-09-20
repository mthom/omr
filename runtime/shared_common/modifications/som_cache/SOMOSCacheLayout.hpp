#if !defined(SOM_OSCACHE_LAYOUT_HPP_INCLUDED)
#define SOM_OSCACHE_LAYOUT_HPP_INCLUDED

#include "OSCacheLayout.hpp"
#include "OSCacheContiguousRegion.hpp"
#include "SOMOSCacheHeader.hpp"

const auto PRELUDE_SECTION_SIZE = 100 * 1024;

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
    , _preludeSection(nullptr, 3, pageBoundaryAligned)
  {
    addRegion(&_header);
    addRegion(&_dataSection);
    addRegion(&_metadataSection);
    addRegion(&_preludeSection);

    for(auto* region : _regions)
      region->setCacheLayout(this);
  }

  virtual ~SOMOSCacheLayout() {}

  // once the cache is attached to, is the data well-formed?
  // depending on page boundary alignment, and possibly other factors, the effective
  // cache size may differ from the block size.
  UDATA effectiveCacheSize() {
    UDATA size = 0;

    for(const auto* region : _regions)
       size += region->regionSize();

    return size;
  }

  UDATA actualCacheSize() {
    return _blockSize;
  }

  SOMOSCacheHeader<OSCacheHeader>* getHeader() {
    return &_header;
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

    const auto dataSectionSize = (size - _header.regionSize()) / 2;

    _dataSection.adjustRegionStartAndSize((void*) frontier, dataSectionSize);
    frontier += _dataSection.regionSize();

    const auto metadataSectionSize = size - dataSectionSize - PRELUDE_SECTION_SIZE - _header.regionSize();
    
    _metadataSection.adjustRegionStartAndSize((void*) frontier, metadataSectionSize);
    frontier += _metadataSection.regionSize();

    _preludeSection.adjustRegionStartAndSize((void*) frontier, PRELUDE_SECTION_SIZE);

    _header.alignToPageBoundary(_osPageSize);
    _dataSection.alignToPageBoundary(_osPageSize);
    _metadataSection.alignToPageBoundary(_osPageSize);
    _preludeSection.alignToPageBoundary(_osPageSize);

    _blockSize = size;  
  }
  
  inline void clearRegions() {
     _regions.clear();
  }
  
  UDATA _blockSize;

  SOMOSCacheHeader<OSCacheHeader> _header;

  OSCacheContiguousRegion _dataSection;
  OSCacheContiguousRegion _metadataSection;
  OSCacheContiguousRegion _preludeSection;
};

#endif
