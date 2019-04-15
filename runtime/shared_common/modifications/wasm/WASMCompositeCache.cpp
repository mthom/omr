#include "WASMOSCacheConfig.hpp"
#include "WASMCompositeCache.hpp"

template <class OSCacheType>
WASMCompositeCache<OSCacheType>::WASMCompositeCache(OSCacheType* osCache, UDATA osPageSize)
  : _osCache(osCache)
{
  _osCache->installConfig(new WASMOSCacheConfig<typename OSCacheType::config_type>(2, osPageSize));
}

template <class OSCacheType>
bool WASMCompositeCache<OSCacheType>::startup(const char* cacheName, const char* ctrlDirName)
{    
  if(!_osCache->startup(cacheName, ctrlDirName))
    return false;

  
}
