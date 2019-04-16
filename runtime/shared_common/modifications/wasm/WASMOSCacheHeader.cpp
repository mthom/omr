#include "WASMOSCacheHeader.hpp"

/*
template <>
void
WASMOSCacheHeader<OSMemoryMappedCacheHeader>::refresh(OMRPortLibrary* library)
{
  OSMemoryMappedCacheHeader::refresh(library);
  
  UDATA* remOffset = (UDATA*) regionStartAddress() + regionSize();

  _readerCount = remOffset;
  _cacheCrc = remOffset + sizeof(volatile UDATA*);

  *_readerCount = 0;
  *_cacheCrc = 0;
}
*/

WASMOSCacheHeaderMapping<OSMemoryMappedCacheHeader>*
WASMOSCacheHeader<OSMemoryMappedCacheHeader>::derivedMapping()
{
  return static_cast<WASMOSCacheHeaderMapping<OSMemoryMappedCacheHeader>*>(_mapping->_mapping);
}

WASMOSCacheHeaderMapping<OSSharedMemoryCacheHeader>*
WASMOSCacheHeader<OSSharedMemoryCacheHeader>::derivedMapping()
{
  return static_cast<WASMOSCacheHeaderMapping<OSSharedMemoryCacheHeader>*>(_mapping->_mapping);
}

void
WASMOSCacheHeader<OSMemoryMappedCacheHeader>::create(OMRPortLibrary* library)
{
  OSMemoryMappedCacheHeader::create(library);

  WASMOSCacheHeaderMapping<OSMemoryMappedCacheHeader>* mapping = derivedMapping();

  mapping->_cacheSize = _configOptions->cacheSize();
  mapping->_readerCount = 0;
  mapping->_cacheCrc = 0;
  mapping->_dataSectionSize = _configOptions->dataSectionSize();
}
