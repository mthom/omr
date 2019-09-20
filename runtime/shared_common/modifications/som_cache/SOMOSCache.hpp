#if !defined(SOM_OS_CACHE_HPP_INCLUDED)
#define SOM_OS_CACHE_HPP_INCLUDED

#include "OSCacheIterator.hpp"
#include "OSCacheRegion.hpp"

#include "SOMMetadataSectionEntryIterator.hpp"
#include "SOMOSCacheConfig.hpp"
#include "SOMOSCacheConfigOptions.hpp"
#include "SOMOSCacheHeaderMapping.hpp"

#include "env/TRMemory.hpp"

template <class SuperOSCache>
class SOMOSCache: public SuperOSCache
{
  friend class SOMCompositeCache;
public:
  TR_ALLOC(TR_Memory::SharedCache)

  SOMOSCache(OMRPortLibrary* library,
	     const char* cacheName,
	     const char* ctrlDirName,
	     IDATA numLocks,
	     SOMOSCacheConfig<typename SuperOSCache::config_type>* config,
	     SOMOSCacheConfigOptions* configOptions,
	     UDATA osPageSize = 0);

  virtual ~SOMOSCache() {
     this->cleanup();
  }

  using SuperOSCache::cleanup;
  
  bool startup(const char* cacheName, const char* ctrlDirName) override {
     return (_started = SuperOSCache::startup(cacheName, ctrlDirName));      
  }

  IDATA acquireLock(UDATA lockID) {
     return _config->acquireLock(this->_portLibrary, lockID, NULL);
  }

  IDATA releaseLock(UDATA lockID) {
     return _config->releaseLock(this->_portLibrary, lockID);
  }

  IDATA acquireHeaderWriteLock();
  IDATA releaseHeaderWriteLock();

  OSCacheContiguousRegion* headerRegion() {
    return (OSCacheContiguousRegion*) _config->_layout[HEADER_REGION_ID];
  }

  OSCacheContiguousRegion* dataSectionRegion() {
    return (OSCacheContiguousRegion*) _config->_layout[DATA_SECTION_REGION_ID];
  }

  OSCacheContiguousRegion* metadataSectionRegion() {
    return (OSCacheContiguousRegion*) _config->_layout[METADATA_REGION_ID];
  }

  OSCacheContiguousRegion* preludeSectionRegion() {
    return (OSCacheContiguousRegion*) _config->_layout[PRELUDE_REGION_ID];
  }

  bool started() override {
    return _started;
  }
  
  UDATA* dataSectionReaderCountFocus() {
    UDATA offset = offsetof(SOMOSCacheHeaderMapping<typename SuperOSCache::header_type>, _dataSectionReaderCount);
    return (UDATA*) ((uint8_t*) headerRegion()->regionStartAddress() + offset);
  }

  UDATA* crcFocus() {
    UDATA offset = offsetof(SOMOSCacheHeaderMapping<typename SuperOSCache::header_type>, _cacheCrc);
    return (UDATA*) ((uint8_t*) headerRegion()->regionStartAddress() + offset);
  }

  U_32* preludeSectionSizeFieldOffset() {
    UDATA offset = offsetof(SOMOSCacheHeaderMapping<typename SuperOSCache::header_type>, _preludeSectionSize);
    return (U_32*) ((uint8_t*) headerRegion()->regionStartAddress() + offset);
  }

  U_32* metadataSectionSizeFieldOffset() {
    UDATA offset = offsetof(SOMOSCacheHeaderMapping<typename SuperOSCache::header_type>, _metadataSectionSize);
    return (U_32*) ((uint8_t*) headerRegion()->regionStartAddress() + offset);
  }

  U_64* lastAssumptionIDOffset() {
    UDATA offset = offsetof(SOMOSCacheHeaderMapping<typename SuperOSCache::header_type>, _lastAssumptionID);
    return (U_64*) ((uint8_t*) headerRegion()->regionStartAddress() + offset);
  }

  U_32* isLockedOffset() {
    UDATA offset = offsetof(SOMOSCacheHeaderMapping<typename SuperOSCache::header_type>, _isLocked);
    return (U_32*) ((uint8_t*) headerRegion()->regionStartAddress() + offset);
  }
  
  U_32 getDataSize() override {
    return _config->getDataSectionSize();
  }

  U_32 getTotalSize() override {
    return _config->getCacheSize();
  }

  OSCacheIterator* constructCacheIterator(char* resultBuf) override;
  
  IDATA setRegionPermissions(OSCacheRegion*) override {
      return 0;
  }

  using SuperOSCache::runExitProcedure;
  using SuperOSCache::errorHandler;
  using SuperOSCache::getPermissionsRegionGranularity;
  
protected:
  IDATA initCacheName(const char* cacheName) override {
    OMRPORT_ACCESS_FROM_OMRPORT(this->_portLibrary);
    
    if (!(this->_cacheName = (char*) omrmem_allocate_memory(OMRSH_MAXPATH, OMRMEM_CATEGORY_CLASSES))) {
      return -1;
    }
    
    strcpy(this->_cacheName, cacheName);
    return 0;
  }

  SOMOSCacheConfig<typename SuperOSCache::config_type>* _config;
  bool _started;
};

#endif
