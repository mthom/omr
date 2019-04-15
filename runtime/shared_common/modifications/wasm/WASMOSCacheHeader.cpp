#include "WASMOSCacheHeader.hpp"

template <class OSCacheHeader>
WASMOSCacheHeader<OSCacheHeader>::WASMOSCacheHeader(WASMOSCacheLayout<OSCacheHeader>* layout,
						    WASMOSCacheConfigOptions* configOptions,
						    int regionID, bool pageBoundaryAligned)
  : OSCacheContiguousRegion(layout, regionID, pageBoundaryAligned)
  , _numLocks(5)
  , _mapping(new WASMOSCacheHeaderMappingImpl<OSCacheHeader>())
  , _configOptions(configOptions)
{}

template <class OSCacheHeader>
UDATA WASMOSCacheHeader<OSCacheHeader>::regionSize()
{
  return OSCacheHeader::regionSize() + sizeof(volatile UDATA) + sizeof(UDATA);
}

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

template <OSCacheHeader>
WASMOSCacheHeaderMapping<OSCacheHeader>*
WASMOSCacheHeader::derivedMapping()
{
  return dynamic_cast<WASMOSCacheHeaderMapping<OSCacheHeader>*>(_mapping);
}

template <>
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
