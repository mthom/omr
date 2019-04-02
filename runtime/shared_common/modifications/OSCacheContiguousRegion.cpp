#include "OSCacheContiguousRegion.hpp"
#include "OSCacheMemoryProtector.hpp"
#include "OSCacheLayout.hpp"

OSCacheContiguousRegion::OSCacheContiguousRegion(OSCacheLayout* layout, int regionID, void* regionStart,
						 UDATA regionSize, bool pageBoundaryAligned)
  : OSCacheRegion(layout, regionID)
  , _regionStart(regionStart)
  , _regionSize(regionSize)
  , _pageBoundaryAligned(pageBoundaryAligned)
{}

bool OSCacheContiguousRegion::alignToPageBoundary(UDATA osPageSize)
{
  if(_pageBoundaryAligned) {
    if(osPageSize > 0) {
      _regionStart = (void*) ROUND_UP_TO(osPageSize, (UDATA) _regionStart);

      UDATA naiveEnd   = (UDATA) _regionStart + _regionSize;
      UDATA roundedEnd = ROUND_DOWN_TO(osPageSize, naiveEnd);

      _regionSize = roundedEnd - (UDATA) _regionStart;
      _layout->notifyRegionSizeAdjustment(*this);
    }

    return true;
  } else {
    return false;
  }
}

bool OSCacheContiguousRegion::isAddressInRegion(void* itemAddress, UDATA itemSize)
{
  return (void*) ((UDATA) regionStartAddress() + regionSize()) >= (void*) ((UDATA) itemAddress + itemSize)
      && regionStartAddress() <= itemAddress;
}

void* OSCacheContiguousRegion::regionEnd()
{
  return (void*) ((UDATA)_regionStart + _regionSize);
}

void* OSCacheContiguousRegion::regionStartAddress() const
{
  return _regionStart;
}

UDATA OSCacheContiguousRegion::regionSize() const
{
  return _regionSize;
}

UDATA OSCacheContiguousRegion::renderToMemoryProtectionFlags()
{
  return _protectionOptions->renderToFlags();
}

IDATA OSCacheContiguousRegion::setPermissions(OSCacheMemoryProtector* protector)
{
  return protector->setPermissions(*this);
}
