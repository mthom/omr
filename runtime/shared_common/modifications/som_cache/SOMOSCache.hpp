#if !defined(SOM_OS_CACHE_HPP_INCLUDED)
#define SOM_OS_CACHE_HPP_INCLUDED

#include "OSCacheIterator.hpp"
#include "OSCacheRegion.hpp"
#include "OSCacheRegionRetreatingBumpFocus.hpp"

#include "SOMMetadataSectionEntryIterator.hpp"
#include "SOMOSCacheConfig.hpp"
#include "SOMOSCacheConfigOptions.hpp"
#include "SOMOSCacheHeaderMapping.hpp"

#include "env/TRMemory.hpp"

class SOMCompositeCache;

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

  using SuperOSCache::startup;
  using SuperOSCache::cleanup;
  
  virtual ~SOMOSCache() {
    this->cleanup();
  }
  
  OSCacheContiguousRegion* headerRegion() {
    return (OSCacheContiguousRegion*) _config->_layout[HEADER_REGION_ID];
  }

  OSCacheContiguousRegion* dataSectionRegion() {
    return (OSCacheContiguousRegion*) _config->_layout[DATA_SECTION_REGION_ID];
  }

  OSCacheContiguousRegion* metadataSectionRegion() {
    return (OSCacheContiguousRegion*) _config->_layout[METADATA_REGION_ID];
  }
  
  UDATA* readerCountFocus() {
    UDATA offset = offsetof(SOMOSCacheHeaderMapping<typename SuperOSCache::header_type>, _readerCount);
    return (UDATA*) ((uint8_t*) headerRegion()->regionStartAddress() + offset);
  }

  UDATA* crcFocus() {
    UDATA offset = offsetof(SOMOSCacheHeaderMapping<typename SuperOSCache::header_type>, _cacheCrc);
    return (UDATA*) ((uint8_t*) headerRegion()->regionStartAddress() + offset);
  }

  UDATA* metadataOffset() {
    UDATA offset = offsetof(SOMOSCacheHeaderMapping<typename SuperOSCache::header_type>, _metadataUpdateOffset);
    return (UDATA*) ((uint8_t*) headerRegion()->regionStartAddress() + offset);
  }

  OSCacheRegionRetreatingBumpFocus<SOMCacheMetadataItemHeader> metadataUpdateFocus()
  {
    auto headerAddress = (uint8_t*) metadataSectionRegion()->regionEnd() - *metadataOffset();
    return {metadataSectionRegion(), (SOMCacheMetadataItemHeader*) headerAddress};
  }
  
  U_32 getDataSize() override {
    return _config->getDataSectionSize();
  }

  U_32 getTotalSize() override {
    return _config->getCacheSize();
  }
  
  // obviously totally naive, but do it anyway.
  bool started() override {
    return true;
  }

  OSCacheIterator* constructCacheIterator(char* resultBuf) override;
  
  using SuperOSCache::runExitProcedure;
  using SuperOSCache::errorHandler;
  using SuperOSCache::getPermissionsRegionGranularity;
  
protected:
  IDATA initCacheName(const char* cacheName) override {
    OMRPORT_ACCESS_FROM_OMRPORT(this->_portLibrary);
    
    if (!(this->_cacheName = (char*)omrmem_allocate_memory(OMRSH_MAXPATH, OMRMEM_CATEGORY_CLASSES))) {
      return -1;
    }
    
    strcpy(this->_cacheName, cacheName);
    return 0;
  }

  SOMOSCacheConfig<typename SuperOSCache::config_type>* _config;
};

#endif
