#if !defined(WASM_COMPOSITE_CACHE_HPP_INCLUDED)
#define WASM_COMPOSITE_CACHE_HPP_INCLUDED

#include "CacheCRCChecker.hpp"
#include "OSCacheBumpRegionFocus.hpp"
#include "SynchronizedCacheCounter.hpp"

#include "OSMemoryMappedCache.hpp"

#include "OSCacheImpl.hpp"

#include "WASMCacheEntry.hpp"
#include "WASMDataSectionEntryIterator.hpp"
#include "WASMOSCache.hpp"

class WASMCompositeCache {
public:
  WASMCompositeCache(WASMOSCache<OSMemoryMappedCache>* osCache, UDATA osPageSize);

  bool startup(const char* cacheName, const char* ctrlDirName);

private:
  virtual WASMDataSectionEntryIterator constructEntryIterator();

  UDATA dataSectionFreeSpace() const; 
  
  // allocate space for an entry! What kind of entry, I dunno..  code
  // or relocation data! Possibly validation data in the future. Who
  // the hell knows. Probably the WASMCacheEntry class should contain
  // factory methods for building WASMCacheAllocator objects, based on
  // their own contents.
  bool storeCodeEntry(const char* methodName, void* codeLocation, U_32 codeLength);
  
  // not nullable once set, but we do eventually want to destroy it,
  // and when we do, perhaps the pointer should become NULL? or not?
  // the reference may not continue after its OSCache is destroyed.
  WASMOSCache<OSMemoryMappedCache>* _osCache;
 
  SynchronizedCacheCounter _readerCount;
  CacheCRCChecker _crcChecker;
  OSCacheBumpRegionFocus<WASMCacheEntry> _codeUpdatePtr;
};

#endif
