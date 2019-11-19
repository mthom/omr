#include "OSMemoryMappedCache.hpp"
#include "OSSharedMemoryCache.hpp"

#include "SOMOSCache.hpp"
#include "SOMOSCacheConfig.hpp"
#include "SOMOSCacheIterator.hpp"

template <class SuperOSCache>
SOMOSCache<SuperOSCache>::SOMOSCache(OMRPortLibrary* library,
				     char* cacheName,
				     char* ctrlDirName,
				     IDATA numLocks,
				     SOMOSCacheConfig<typename SuperOSCache::config_type>* config,
				     SOMOSCacheConfigOptions* configOptions,
				     UDATA osPageSize,
				     bool callStartup)
  : SuperOSCache(library, cacheName, ctrlDirName, numLocks,
		 (_config = config),
		 configOptions)
{
  if (callStartup) {
     startup(cacheName, ctrlDirName);
  }
}

template <class SuperOSCache>
OSCacheIterator*
SOMOSCache<SuperOSCache>::constructCacheIterator(char* resultBuf) {
  return new (PERSISTENT_NEW) SOMOSCacheIterator<typename SuperOSCache::iterator_type>(this->_cacheLocation, resultBuf);
}

template <>
IDATA SOMOSCache<OSMemoryMappedCache>::acquireHeaderWriteLock() {
   return _config->acquireHeaderWriteLock(this->_portLibrary, false, NULL);
}

template <>
IDATA SOMOSCache<OSMemoryMappedCache>::releaseHeaderWriteLock() {
  return _config->releaseHeaderWriteLock(this->_portLibrary, false, NULL);
}


template <>
IDATA SOMOSCache<OSSharedMemoryCache>::acquireHeaderWriteLock() {
   return _config->acquireHeaderWriteLock(this->_portLibrary, this->_cacheName, NULL);
}

template <>
IDATA SOMOSCache<OSSharedMemoryCache>::releaseHeaderWriteLock() {
  return _config->releaseHeaderWriteLock(this->_portLibrary, NULL);
}

template class SOMOSCache<OSMemoryMappedCache>;
template class SOMOSCache<OSSharedMemoryCache>;

