#include "SOMOSCacheHeader.hpp"

SOMOSCacheHeaderMapping<OSMemoryMappedCacheHeader>*
SOMOSCacheHeader<OSMemoryMappedCacheHeader>::derivedMapping()
{
  return static_cast<SOMOSCacheHeaderMapping<OSMemoryMappedCacheHeader>*>(_mapping->_mapping);
}

SOMOSCacheHeaderMapping<OSSharedMemoryCacheHeader>*
SOMOSCacheHeader<OSSharedMemoryCacheHeader>::derivedMapping()
{
  return static_cast<SOMOSCacheHeaderMapping<OSSharedMemoryCacheHeader>*>(_mapping->_mapping);
}

template <class OSCacheHeader>
void refresh_mapping(SOMOSCacheConfigOptions* configOptions,
		     SOMOSCacheHeaderMapping<OSCacheHeader>* mapping)
{
  mapping->_cacheSize = configOptions->cacheSize();
  mapping->_dataSectionReaderCount = 0;
  mapping->_cacheCrc = 0;
  mapping->_dataSectionSize = configOptions->dataSectionSize();
}

void
SOMOSCacheHeader<OSMemoryMappedCacheHeader>::create(OMRPortLibrary* library)
{
  OSMemoryMappedCacheHeader::create(library);  

  derivedMapping()->_vmCounter = 0;
  derivedMapping()->_writeHash = 0;

  refresh_mapping(_configOptions, derivedMapping());
}

void
SOMOSCacheHeader<OSMemoryMappedCacheHeader>::refresh(OMRPortLibrary* library)
{
  OSMemoryMappedCacheHeader::refresh(library);
  refresh_mapping(_configOptions, derivedMapping());
}

void
SOMOSCacheHeader<OSSharedMemoryCacheHeader>::create(OMRPortLibrary* library, bool inDefaultControlDir)
{
  OSSharedMemoryCacheHeader::create(library, inDefaultControlDir);
  refresh_mapping(_configOptions, derivedMapping());
}

void
SOMOSCacheHeader<OSSharedMemoryCacheHeader>::refresh(OMRPortLibrary* library, bool inDefaultControlDir)
{
  OSSharedMemoryCacheHeader::refresh(library, inDefaultControlDir);
  refresh_mapping(_configOptions, derivedMapping());
}
