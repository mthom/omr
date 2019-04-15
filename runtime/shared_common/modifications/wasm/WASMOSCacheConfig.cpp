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
