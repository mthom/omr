#include "OSMemoryMappedCache.hpp"
#include "OSSharedMemoryCache.hpp"

#include "SOMOSCache.hpp"
#include "SOMOSCacheConfig.hpp"
#include "SOMOSCacheIterator.hpp"

template <class SuperOSCache>
SOMOSCache<SuperOSCache>::SOMOSCache(OMRPortLibrary* library,
				       const char* cacheName,
				       const char* ctrlDirName,
				       IDATA numLocks,
				       SOMOSCacheConfig<typename SuperOSCache::config_type>* config,
				       SOMOSCacheConfigOptions* configOptions,
				       UDATA osPageSize)
  : SuperOSCache(library, cacheName, ctrlDirName, numLocks,
		 (_config = config),
		 configOptions)
{
  startup(cacheName, ctrlDirName);
}

template <class SuperOSCache>
OSCacheIterator*
SOMOSCache<SuperOSCache>::constructCacheIterator(char* resultBuf) {
  return new (PERSISTENT_NEW) SOMOSCacheIterator<typename SuperOSCache::iterator_type>(this->_cacheLocation, resultBuf);
}

template class SOMOSCache<OSMemoryMappedCache>;
template class SOMOSCache<OSSharedMemoryCache>;
