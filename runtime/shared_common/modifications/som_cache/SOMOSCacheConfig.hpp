#if !defined(SOM_OS_CACHE_CONFIG_HPP_INCLUDED)
#define SOM_OS_CACHE_CONFIG_HPP_INCLUDED

#include "SOMOSCacheLayout.hpp"

#include "env/TRMemory.hpp"

#define HEADER_REGION_ID 0
#define DATA_SECTION_REGION_ID 1
#define METADATA_REGION_ID 2
#define PRELUDE_REGION_ID 3

#define PRELUDE_SECTION_LOCK_ID 0
#define DATA_SECTION_LOCK_ID 1
#define METADATA_SECTION_LOCK_ID 2

template <class>
class SOMOSCache;

template <class>
class SOMCacheStats;

template <class OSCacheConfigImpl>
class SOMOSCacheConfig: public OSCacheConfigImpl
{
public:
  TR_ALLOC(TR_Memory::SharedCacheConfig)

  typedef typename OSCacheConfigImpl::header_type header_type;
  typedef typename OSCacheConfigImpl::cache_type cache_type;

  SOMOSCacheConfig(U_32 numLocks, SOMOSCacheConfigOptions* configOptions, UDATA osPageSize);
  
  void* getDataSectionLocation() override;
  U_32 getDataSectionSize() override;
  U_32* getDataLengthFieldLocation() override;
  U_64* getInitCompleteLocation() override;
  bool setCacheInitComplete() override;
  void nullifyRegions() override;
  U_32 getCacheSize() override;
  U_32* getCacheSizeFieldLocation() override;
//U_64* getAssumptionIDFieldLocation();
//  U_64 getAssumptionID();
//U_64* getCardFieldLocation();
//  U_64 getCard();
  void detachRegions() override;
  
  virtual void serializeCacheLayout(OSCache* osCache, void* blockAddress, U_32 cacheSize) override;
  virtual void initializeCacheLayout(OSCache* osCache, void* blockAddress, U_32 cacheSize) override;
  
  SOMOSCacheHeader<header_type>* getHeader();

  using OSCacheConfigImpl::releaseLock;
  using OSCacheConfigImpl::acquireHeaderWriteLock;
  using OSCacheConfigImpl::releaseHeaderWriteLock;
  
private:
  friend class SOMOSCache<cache_type>;
  friend class SOMCacheStats<cache_type>;

  SOMOSCacheLayout<header_type> _layout;
};

#endif