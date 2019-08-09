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
#include "runtime/Runtime.hpp"
#include "env/TRMemory.hpp"

class WASMCompositeCache {
public:
  TR_ALLOC(TR_Memory::SharedCache)
  
  WASMCompositeCache(const char* cacheName, const char* cachePath);

  virtual ~WASMCompositeCache() {
    //delete pointers
    _osCache->cleanup();
  }

  void setRelocationData(uint8_t* relocationData) {
      _relocationData = relocationData;
  }
  
  bool startup(const char* cacheName, const char* ctrlDirName);

  bool storeEntry(const char* elementName, void* data, uint32_t size);

  UDATA baseSharedCacheAddress();

 WASMCacheEntry *loadEntry(const char *elementName);

  void storeCallAddressToHeaders(void *calleeMethod,size_t methodNameTemplateOffset,void *calleeCodeCacheAddress);

  bool checkTime(uint64_t moduleTime);
  
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

  uint8_t* _relocationData;
};

#endif
