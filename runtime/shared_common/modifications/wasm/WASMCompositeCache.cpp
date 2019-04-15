#include "WASMOSCacheConfig.hpp"
#include "WASMCompositeCache.hpp"

template <class SuperOSCache>
WASMCompositeCache<SuperOSCache>::WASMCompositeCache(WASMOSCache<SuperOSCache>* osCache, UDATA osPageSize)
  : _osCache(osCache)
{
  _osCache->installConfig(new WASMOSCacheConfig<typename SuperOSCache::config_type>(2, osPageSize));
}

template <class SuperOSCache>
bool WASMCompositeCache<SuperOSCache>::startup(const char* cacheName, const char* ctrlDirName)
{    
  if(!_osCache->startup(cacheName, ctrlDirName))
    return false;

  
}
