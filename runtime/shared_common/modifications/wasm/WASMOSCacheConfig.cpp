#include "OSCache.hpp"
#include "WASMOSCache.hpp"
#include "WASMOSCacheConfig.hpp"
#include "WASMOSCacheHeaderMapping.hpp"
#include "WASMOSCacheLayout.hpp"

template <class OSCacheConfigImpl>
U_32 WASMOSCacheConfig<OSCacheConfigImpl>::getDataSectionSize()
{
  WASMOSCacheHeaderMapping<header_type>* mapping = _layout->_header->derivedMapping();
  return mapping->_dataSectionSize;
}

template <class OSCacheConfigImpl>
void WASMOSCacheConfig<OSCacheConfigImpl>::serializeCacheLayout(OSCache* osCache, void* blockAddress, U_32 size)
{
  _layout->init(blockAddress, size);
	
  OSCacheRegionSerializer* serializer = osCache->constructSerializer();

  for(int i = 0; i < _layout->numberOfRegions(); ++i) {
    _layout[i]->serialize(serializer);
  }
}

template <class OSCacheConfigImpl>
void WASMOSCacheConfig<OSCacheConfigImpl>::initializeCacheLayout(OSCache* osCache, void* blockAddress, U_32 size)
{
  _layout->init(blockAddress, size);

  OSCacheRegionInitializer* initializer = osCache->constructInitializer();

  for(int i = 0; i < _layout->numberOfRegions(); ++i) {
    _layout[i]->initialize(initializer);
  }
}



