#if !defined(WASM_COMPOSITE_CACHE_HPP_INCLUDED)
#define WASM_COMPOSITE_CACHE_HPP_INCLUDED

#include <map>
#include <string>

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

  bool storeCodeEntry(const char* methodName, void* codeLocation, U_32 codeLength);

private:
  virtual WASMDataSectionEntryIterator constructEntryIterator();

  UDATA dataSectionFreeSpace() const;
  void populateTables();

  // not nullable once set, but we do eventually want to destroy it,
  // and when we do, perhaps the pointer should become NULL? or not?
  // the reference may not continue after its OSCache is destroyed.
  WASMOSCache<OSMemoryMappedCache>* _osCache;

  SynchronizedCacheCounter _readerCount;
  CacheCRCChecker _crcChecker;
  OSCacheBumpRegionFocus<WASMCacheEntry> _codeUpdatePtr;

  std::map<std::string, WASMCacheEntry*> _codeEntries;
};

#endif
