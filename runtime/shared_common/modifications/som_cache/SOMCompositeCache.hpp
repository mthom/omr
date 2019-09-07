#if !defined(SOM_COMPOSITE_CACHE_HPP_INCLUDED)
#define SOM_COMPOSITE_CACHE_HPP_INCLUDED

#include <map>
#include <string>

#include "CacheCRCChecker.hpp"
#include "OSCacheRegionBumpFocus.hpp"
#include "OSCacheRegionRetreatingBumpFocus.hpp"
#include "SynchronizedCacheCounter.hpp"

#include "OSMemoryMappedCache.hpp"
#include "OSSharedMemoryCache.hpp"

#include "OSCacheImpl.hpp"

#include "SOMDataSectionEntryIterator.hpp"
#include "SOMMetadataSectionEntryIterator.hpp"
#include "SOMOSCache.hpp"

#include "env/TRMemory.hpp"

class SOMCompositeCache {
public:
  TR_ALLOC(TR_Memory::SharedCache)

  explicit SOMCompositeCache(const char* cacheName = "som_shared_cache",
			     const char* cachePath = "/tmp");

  void cleanup() {
    _osCache.cleanup();
  }
  
  void setRelocationData(uint8_t* relocationData) {
    _relocationData = relocationData;
  }

  bool startup(const char* cacheName, const char* ctrlDirName);

  bool storeEntry(const char* methodName, void* codeLocation, U_32 codeLength);

  bool createdNewCache();

  UDATA baseSharedCacheAddress();

  void *loadEntry(const char *methodName);

  void copyPreludeBuffer(void* data, size_t size);

  void storeCallAddressToHeaders(void *calleeMethod, size_t methodNameTemplateOffset, void *calleeCodeCacheAddress);

  virtual SOMCacheMetadataEntryIterator constructMetadataSectionEntryIterator();
  virtual SOMCacheMetadataEntryIterator constructPreludeSectionEntryIterator();

private:
  virtual SOMDataSectionEntryIterator constructDataSectionEntryIterator(SOMCacheEntry* delimiter);

  UDATA dataSectionFreeSpace();
  void populateTables();

  SOMOSCacheConfigOptions _configOptions;
  SOMOSCacheConfig<typename OSMemoryMappedCache::config_type> _config;
  SOMOSCache<OSMemoryMappedCache> _osCache;

  SynchronizedCacheCounter _readerCount;
  CacheCRCChecker _crcChecker;
  OSCacheRegionBumpFocus<SOMCacheEntry> _codeUpdatePtr;
  OSCacheRegionBumpFocus<SOMCacheMetadataItemHeader> _preludeUpdatePtr;

  std::map<std::string, SOMCacheEntry*> _codeEntries;
  std::map<std::string, void*> _loadedMethods;

  uint8_t* _relocationData;
};

#endif
