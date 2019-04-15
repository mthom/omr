#include "OSCacheContiguousRegion.hpp"

#include "WASMOSCacheHeader.hpp"
#include "WASMOSCacheLayout.hpp"

template <class OSCacheHeader>
void WASMOSCacheLayout<OSCacheHeader>::init(void* blockAddress, uintptr_t size)
{
  _header->adjustRegionStart(blockAddress);
  _dataSection->adjustRegionStart((void*) ((UDATA) blockAddress + _header->regionSize()));
  
  _header->alignToPageBoundary(_osPageSize);
  _dataSection->alignToPageBoundary(_osPageSize);

  _blockSize = size;  
}

template <class OSCacheHeader>
void
WASMOSCacheLayout<OSCacheHeader>::notifyRegionMappingStartAddress(OSCache* osCache, void* blockAddress,
								  uintptr_t size)
{
  _layout->initialize(osCache, blockAddress, size);
}
