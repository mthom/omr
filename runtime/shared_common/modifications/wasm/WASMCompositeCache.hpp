#if !defined(WASM_COMPOSITE_CACHE_HPP_INCLUDED)
#define WASM_COMPOSITE_CACHE_HPP_INCLUDED

#include "CacheCRCChecker.hpp"
#include "OSCacheImpl.hpp"
#include "SynchronizedCacheCounter.hpp"
#include "OSCacheRegionBumpFocus.hpp"

class WASMCompositeCache {
public:
  WASMCompositeCache(OSCacheImpl* osCache);

  bool startup();

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
  OSCacheImpl* _osCache;

  SynchronizedCacheCounter _readerCount;
  CacheCRCChecker _crcChecker;
  OSCacheRegionBumpFocus<U_8*> _codeUpdatePtr;
};

#endif
