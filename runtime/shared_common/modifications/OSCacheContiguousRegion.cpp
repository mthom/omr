#include "OSCacheContiguousRegion.hpp"
#include "OSCacheLayout.hpp"

OSCacheContiguousRegion::OSCacheContiguousRegion(OSCacheLayout* layout, int regionID, void* regionStart,
						 UDATA regionSize, bool pageBoundaryAligned)
  : OSCacheRegion(layout, regionID)
  , _regionStart(regionStart),
  , _regionSize(regionSize)
  , _pageBoundaryAligned(pageBoundaryAligned)
{}

bool OSCacheContiguousRegion::alignToPageBoundary(UDATA osPageSize)
{
  if(_pageBoundaryAligned) {
    if(osPageSize > 0) {
      _regionStart = ROUND_UP_TO(osPageSize, _regionStart);

      void* naiveEnd   = _regionStart + _regionSize;
      void* roundedEnd = ROUND_DOWN_TO(osPageSize, naiveEnd);

      _regionSize = roundedEnd - _regionStart;
      _layout->notifyRegionSizeAdjustment(*this);
    }

    return true;
  } else {
    return false;
  }
}

void* OSCacheContiguousRegion::regionEnd()
{
  return _regionStart + _regionSize;
}

void* OSCacheContiguousRegion::regionStartAddress()
{
  return _regionStart;
}

UDATA OSCacheContiguousRegion::regionSize()
{
  return _regionSize;
}
