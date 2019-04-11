#include "OSCacheContiguousRegion.hpp"

#include "WASMOSCacheHeader.hpp"
#include "WASMOSCacheLayout.hpp"

void WASMOSCacheLayout::init(void* blockAddress, uintptr_t size)
{
  _header->adjustRegionStart(blockAddress);
  _dataSection->adjustRegionStart((void*) ((UDATA) blockAddress + _header->regionSize()));
  
  _header->alignToPageBoundary(_osPageSize);
  _dataSection->alignToPageBoundary(_osPageSize);

  _blockSize = size;  
}
