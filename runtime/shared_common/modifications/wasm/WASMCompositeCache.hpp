#if !defined(WASM_COMPOSITE_CACHE_HPP_INCLUDED)
#define WASM_COMPOSITE_CACHE_HPP_INCLUDED

#include "CacheCRCChecker.hpp"
#include "OSCacheImpl.hpp"
#include "OSCacheBumpRegionFocus.hpp"
#include "SynchronizedCacheCounter.hpp"

class WASMCacheEntry;

template <class OSCacheType>
class WASMCompositeCache {
public:
  WASMCompositeCache(OSCacheType* osCache, UDATA osPageSize);

  bool startup(const char* cacheName, const char* ctrlDirName);

  // allocate space for an entry! What kind of entry, I dunno..  code
  // or relocation data! Possibly validation data in the future. Who
  // the hell knows. Probably the WASMCacheEntry class should contain
  // factory methods for building WASMCacheAllocator objects, based on
  // their own contents.
  bool allocate(WASMCacheEntry* entry);

private:
  // not nullable once set, but we do eventually want to destroy it,
  // and when we do, perhaps the pointer should become NULL? or not?
  // the reference may not continue after its OSCache is destroyed.
  OSCacheType* _osCache;

  SynchronizedCacheCounter _readerCount;
  CacheCRCChecker _crcChecker;
  OSCacheBumpRegionFocus<U_8> _codeUpdatePtr;
};

#endif
