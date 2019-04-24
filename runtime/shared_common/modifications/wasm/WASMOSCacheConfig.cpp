#include "OSCache.hpp"
#include "WASMOSCache.hpp"
#include "WASMOSCacheConfig.hpp"
#include "WASMOSCacheHeaderMapping.hpp"
#include "WASMOSCacheLayout.hpp"

template <class OSCacheConfigImpl>
void WASMOSCacheConfig<OSCacheConfigImpl>::serializeCacheLayout(OSCache* osCache, void* blockAddress, U_32 size)
{
  _layout->init(blockAddress, size);
	
  OSCacheRegionSerializer* serializer = osCache->constructSerializer();

  for(int i = 0; i < _layout->numberOfRegions(); ++i) {
    _layout->operator[](i)->serialize(serializer);
  }
}

template <class OSCacheConfigImpl>
void WASMOSCacheConfig<OSCacheConfigImpl>::initializeCacheLayout(OSCache* osCache, void* blockAddress, U_32 size)
{
  _layout->init(blockAddress, size);

  OSCacheRegionInitializer* initializer = osCache->constructInitializer();

  for(int i = 0; i < _layout->numberOfRegions(); ++i) {
    _layout->operator[](i)->initialize(initializer);
  }
}

template <class OSCacheConfigImpl>
void WASMOSCacheConfig<OSCacheConfigImpl>::nullifyRegions()
{
  memset(_layout->operator[](HEADER_REGION_ID)->regionStartAddress(), 0,
	 _layout->operator[](HEADER_REGION_ID)->regionSize());
  memset(_layout->operator[](DATA_SECTION_REGION_ID)->regionStartAddress(), 0,
	 _layout->operator[](DATA_SECTION_REGION_ID)->regionSize());
}

template <class OSCacheConfigImpl>
U_32 WASMOSCacheConfig<OSCacheConfigImpl>::getDataSectionSize() {
  return _layout->operator[](DATA_SECTION_REGION_ID)->regionSize();
}

template <class OSCacheConfigImpl>
void* WASMOSCacheConfig<OSCacheConfigImpl>::getDataSectionLocation() {
  return _layout->operator[](DATA_SECTION_REGION_ID)->regionStartAddress();
}

template <class OSCacheConfigImpl>
U_32* WASMOSCacheConfig<OSCacheConfigImpl>::getDataLengthFieldLocation() {
  U_32 offset = offsetof(WASMOSCacheHeaderMapping<header_type>, _dataSectionSize);
  return ((U_32*) _layout->operator[](HEADER_REGION_ID)->regionStartAddress() + offset);
}

template <class OSCacheConfigImpl>
U_64* WASMOSCacheConfig<OSCacheConfigImpl>::getInitCompleteLocation() {
  UDATA offset = offsetof(WASMOSCacheHeaderMapping<header_type>, _cacheInitComplete);
  return ((UDATA*) _layout->operator[](HEADER_REGION_ID)->regionStartAddress() + offset);
}

template <class OSCacheConfigImpl>
bool WASMOSCacheConfig<OSCacheConfigImpl>::setCacheInitComplete() {
  WASMOSCacheHeader<header_type>* header = dynamic_cast<WASMOSCacheHeader<header_type>*>(_layout->operator[](HEADER_REGION_ID));
  header->derivedMapping()->_cacheInitComplete = 1;
  return true;
}

template <class OSCacheConfigImpl>
U_32 WASMOSCacheConfig<OSCacheConfigImpl>::getCacheSize() {
  return getDataSectionSize() + _layout->operator[](HEADER_REGION_ID)->regionSize();
}

template <class OSCacheConfigImpl>
U_32* WASMOSCacheConfig<OSCacheConfigImpl>::getCacheSizeFieldLocation() {
  U_32 offset = offsetof(WASMOSCacheHeaderMapping<header_type>, _cacheSize);
  return ((U_32*) _layout->operator[](HEADER_REGION_ID)->regionStartAddress() + offset);
}

template <class OSCacheConfigImpl>
void WASMOSCacheConfig<OSCacheConfigImpl>::detachRegions() {
  _layout->clearRegions();
}

template <class OSCacheConfigImpl>
bool WASMOSCacheConfig<OSCacheConfigImpl>::isCacheHeaderValid() {
  return true;
}

template class WASMOSCacheConfig<OSMemoryMappedCacheConfig>;
