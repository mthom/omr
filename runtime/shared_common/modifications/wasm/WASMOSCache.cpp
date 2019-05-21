#include "OSMemoryMappedCache.hpp"

#include "WASMOSCache.hpp"
#include "WASMOSCacheConfig.hpp"
#include "WASMOSCacheIterator.hpp"

template <class SuperOSCache>
WASMOSCache<SuperOSCache>::WASMOSCache(OMRPortLibrary* library,
				       const char* cacheName,
				       const char* ctrlDirName,
				       IDATA numLocks,
				       WASMOSCacheConfigOptions* configOptions,
				       UDATA osPageSize)
  : SuperOSCache(library, cacheName, ctrlDirName, numLocks,
		 (_config = new (PERSISTENT_NEW) WASMOSCacheConfig<typename SuperOSCache::config_type>(numLocks, configOptions, osPageSize)),
		 configOptions)
{
  startup(cacheName, ctrlDirName);
}

template <class SuperOSCache>
OSCacheIterator*
WASMOSCache<SuperOSCache>::constructCacheIterator(char* resultBuf) {
  return new (PERSISTENT_NEW) WASMOSCacheIterator<typename SuperOSCache::iterator_type>(this->_cacheLocation, resultBuf);
}

template class WASMOSCache<OSMemoryMappedCache>;
