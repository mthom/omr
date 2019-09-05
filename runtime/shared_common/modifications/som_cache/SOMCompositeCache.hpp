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

  bool startup(const char* cacheName, const char* ctrlDirName);

  bool storeEntry(const char* methodName, void* codeLocation, U_32 codeLength);

  bool createdNewCache();

  UDATA baseSharedCacheAddress();

  void *loadEntry(const char *methodName);

  void copyMetadataBuffer(void* data, size_t size);

  void storeCallAddressToHeaders(void *calleeMethod, size_t methodNameTemplateOffset, void *calleeCodeCacheAddress);

  virtual SOMCacheMetadataEntryIterator constructMetadataSectionEntryIterator();

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
  OSCacheRegionRetreatingBumpFocus<SOMCacheMetadataItemHeader> _metadataUpdatePtr;

  std::map<std::string, SOMCacheEntry*> _codeEntries;
  std::map<std::string, void*> _loadedMethods;
};

#endif
