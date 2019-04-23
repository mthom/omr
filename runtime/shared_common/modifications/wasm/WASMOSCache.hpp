#if !defined(WASM_OS_CACHE_HPP_INCLUDED)
#define WASM_OS_CACHE_HPP_INCLUDED

#include "OSCacheRegion.hpp"

#include "WASMOSCacheConfig.hpp"
#include "WASMOSCacheConfigOptions.hpp"
#include "WASMOSCacheHeaderMapping.hpp"

#include "env/TRMemory.hpp"

class WASMCompositeCache;

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

  OSCacheContiguousRegion* headerRegion() {
    return (OSCacheContiguousRegion*) _config->_layout->operator[](HEADER_REGION_ID);
  }

  OSCacheContiguousRegion* dataSectionRegion() {
    return (OSCacheContiguousRegion*) _config->_layout->operator[](DATA_SECTION_REGION_ID);
  }

  UDATA* readerCountFocus() {
    UDATA offset = offsetof(WASMOSCacheHeaderMapping<typename SuperOSCache::header_type>, _readerCount);
    return (UDATA*) headerRegion()->regionStartAddress() + offset;
  }

  UDATA* crcFocus() {
    UDATA offset = offsetof(WASMOSCacheHeaderMapping<typename SuperOSCache::header_type>, _cacheCrc);
    return (UDATA*) headerRegion()->regionStartAddress() + offset;
  }

  U_32 getDataSize() {
    return dataSectionRegion()->regionSize();
  }

  U_32 getTotalSize() {
    return getDataSize() + headerRegion()->regionSize();
  }

  IDATA initCacheName(const char* cacheName) {
    strcpy(this->_cacheName, cacheName);
    return 0;
  }

  // obviously totally naive, but do it anyway.
  bool started() {
    return true;
  }
  
  using SuperOSCache::runExitProcedure;
  using SuperOSCache::errorHandler;
  using SuperOSCache::getPermissionsRegionGranularity;
  
protected:
  WASMOSCacheConfig<typename SuperOSCache::config_type>* _config;
};

#endif
