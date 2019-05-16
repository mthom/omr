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

#include "env/TRMemory.hpp"

class WASMCompositeCache {
public:
  TR_ALLOC(TR_Memory::SharedCache)
  
  WASMCompositeCache(WASMOSCache<OSMemoryMappedCache>* osCache, UDATA osPageSize);

  virtual ~WASMCompositeCache() {
    //delete pointers
    _osCache->cleanup();
  }
  
  bool startup(const char* cacheName, const char* ctrlDirName);

  bool storeCodeEntry(const char* methodName, void* codeLocation, U_32 codeLength);

  void *loadCodeEntry(const char *methodName, U_32 &codeLength);


private:
  virtual WASMDataSectionEntryIterator constructEntryIterator(WASMCacheEntry* delimiter);
  
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
  std::map<std::string, void *> _loadedMethods;
};

#endif
