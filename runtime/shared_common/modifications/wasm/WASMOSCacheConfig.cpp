#include "WASMOSCacheConfig.hpp"

template <class OSCacheConfigImpl>
void WASMOSCacheConfig<OSCacheConfigImpl>::notifyRegionMappingStartAddress(void* blockAddress, uintptr_t size)
{
  _layout->init(blockAddress, size);
}

template <class OSCacheConfigImpl>
J9SRP* WASMOSCacheConfig<OSCacheConfigImpl>::getDataSectionLocation()
{
  return (J9SRP*) _dataSection->regionStart();
}

template <class OSCacheConfigImpl>
U_64 WASMOSCacheConfig<OSCacheConfigImpl>::getDataSectionSize()
{
  return _dataSection->regionSize();
}
