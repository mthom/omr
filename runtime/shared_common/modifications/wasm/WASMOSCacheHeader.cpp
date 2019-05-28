#include "WASMOSCacheHeader.hpp"

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

template <class OSCacheHeader>
void refresh_mapping(WASMOSCacheConfigOptions* configOptions,
		     WASMOSCacheHeaderMapping<OSCacheHeader>* mapping)
{
  mapping->_cacheSize = configOptions->cacheSize();
  mapping->_readerCount = 0;
  mapping->_cacheCrc = 0;
  mapping->_dataSectionSize = configOptions->dataSectionSize();
}

void
WASMOSCacheHeader<OSMemoryMappedCacheHeader>::create(OMRPortLibrary* library)
{
  OSMemoryMappedCacheHeader::create(library);
  refresh_mapping(_configOptions, derivedMapping());
}

void
WASMOSCacheHeader<OSMemoryMappedCacheHeader>::refresh(OMRPortLibrary* library)
{
  OSMemoryMappedCacheHeader::refresh(library);
  refresh_mapping(_configOptions, derivedMapping());
}

void
WASMOSCacheHeader<OSSharedMemoryCacheHeader>::create(OMRPortLibrary* library, bool inDefaultControlDir)
{
  OSSharedMemoryCacheHeader::create(library, inDefaultControlDir);
  refresh_mapping(_configOptions, derivedMapping());
}

void
WASMOSCacheHeader<OSSharedMemoryCacheHeader>::refresh(OMRPortLibrary* library, bool inDefaultControlDir)
{
  OSSharedMemoryCacheHeader::refresh(library, inDefaultControlDir);
  refresh_mapping(_configOptions, derivedMapping());
}
