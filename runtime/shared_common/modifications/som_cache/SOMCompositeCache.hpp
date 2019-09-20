#if !defined(SOM_COMPOSITE_CACHE_HPP_INCLUDED)
#define SOM_COMPOSITE_CACHE_HPP_INCLUDED

#include <map>
#include <string>

#include "CacheCRCChecker.hpp"
#include "OSCacheRegionBumpFocus.hpp"
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

  bool storeEntry(const char* methodName, void* data, U_32 allocSize);

  bool createdNewCache();

  UDATA baseSharedCacheAddress();

  void *loadEntry(const char *methodName);

  bool isCacheLocked();
  void lockCache();
  void unlockCache();

  void copyPreludeBuffer(void* data, size_t size);
  void copyMetadata(void* data, size_t size);

  void storeCallAddressToHeaders(void *calleeMethod, size_t methodNameTemplateOffset, void *calleeCodeCacheAddress);

//void storeAssumptionID(UDATA);
//UDATA assumptionID();

//void storeCard(UDATA);
//UDATA card();

  virtual SOMCacheMetadataEntryIterator constructMetadataSectionEntryIterator();
  virtual SOMCacheMetadataEntryIterator constructPreludeSectionEntryIterator();

  U_64 lastAssumptionID();
  void setLastAssumptionID(U_64 assumptionID);

private:
  virtual SOMDataSectionEntryIterator constructDataSectionEntryIterator(SOMCacheEntry* delimiter);

  UDATA dataSectionFreeSpace();
  void populateTables();
  void updateMetadataPtr();

  void enterReadMutex();
  void exitReadMutex();
  
  SOMOSCacheConfigOptions _configOptions;
  SOMOSCacheConfig<typename OSMemoryMappedCache::config_type> _config;
  SOMOSCache<OSMemoryMappedCache> _osCache;

  SynchronizedCacheCounter _dataSectionReaderCount;
  CacheCRCChecker _crcChecker;
  OSCacheRegionBumpFocus<SOMCacheEntry> _codeUpdatePtr;
  OSCacheRegionBumpFocus<SOMCacheMetadataItemHeader> _metadataUpdatePtr;

  std::map<std::string, SOMCacheEntry*> _codeEntries;
  std::map<std::string, void*> _loadedMethods;

  uint8_t* _relocationData;
};

#endif
