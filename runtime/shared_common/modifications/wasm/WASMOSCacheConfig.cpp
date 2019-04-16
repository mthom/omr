#include "OSCache.hpp"
#include "WASMOSCache.hpp"
#include "WASMOSCacheConfig.hpp"
#include "WASMOSCacheLayout.hpp"

template <class OSCacheConfigImpl>
J9SRP* WASMOSCacheConfig<OSCacheConfigImpl>::getDataSectionLocation()
{
  return (J9SRP*) _layout->_dataSection->regionStart();
}

template <class OSCacheConfigImpl>
U_64 WASMOSCacheConfig<OSCacheConfigImpl>::getDataSectionSize()
{
  return _layout->_dataSection->regionSize();
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



