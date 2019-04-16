#if !defined(WASM_OS_CACHE_HPP_INCLUDED)
#define WASM_OS_CACHE_HPP_INCLUDED

#include "WASMOSCacheConfigOptions.hpp"

#include "env/TRMemory.hpp"

// this is meant to augment an existing header, like
// OSMemoryMappedCacheHeader, or OSSharedMemoryCacheHeader.  also,
// it's a region of the cache! but then, the OSCacheHeader might be
// too, hence the virtual base class of OSCacheRegion.  OSCacheRegion
// doesn't currently contain state, so the virtual designator isn't
// necessary *now*, but who knows, that might change.
template <class SuperOSCache>
class WASMOSCache: public SuperOSCache
{
public:
  TR_ALLOC(TR_Memory::SharedCache)
  
  WASMOSCache(OMRPortLibrary* library,
	      const char* cacheName,
	      const char* ctrlDirName,
	      IDATA numLocks,
	      WASMOSCacheConfigOptions* configOptions,
	      UDATA osPageSize = 0);

  using SuperOSCache::startup;
};

#endif
