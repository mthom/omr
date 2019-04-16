#include "WASMOSCache.hpp"
#include "WASMOSCacheConfig.hpp"

template <class SuperOSCache>
WASMOSCache<SuperOSCache>::WASMOSCache(OMRPortLibrary* library,
				       const char* cacheName,
				       const char* ctrlDirName,
				       IDATA numLocks,
				       WASMOSCacheConfigOptions* configOptions,
				       UDATA osPageSize)
  : SuperOSCache(library, cacheName, ctrlDirName, numLocks,
		 new WASMOSCacheConfig<typename SuperOSCache::config_type>(numLocks, osPageSize),
		 configOptions)
{}
