#if !defined(WASM_OS_CACHE_CONFIG_HPP_INCLUDED)
#define WASM_OS_CACHE_CONFIG_HPP_INCLUDED

#include "WASMOSCacheLayout.hpp"

#include "env/TRMemory.hpp"

#define HEADER_REGION_ID 0
#define DATA_SECTION_REGION_ID 1

template <class>
class WASMOSCache;

// this is meant to augment an existing header, like
// OSMemoryMappedCacheHeader, or OSSharedMemoryCacheHeader.  also,
// it's a region of the cache! but then, the OSCacheHeader might be
// too, hence the virtual base class of OSCacheRegion.  OSCacheRegion
// doesn't currently contain state, so the virtual designator isn't
// necessary *now*, but who knows, that might change.
template <class OSCacheConfigImpl>
class WASMOSCacheConfig: public OSCacheConfigImpl
{
public:
  TR_ALLOC(TR_Memory::SharedCacheConfig)

  typedef typename OSCacheConfigImpl::header_type header_type;
  typedef typename OSCacheConfigImpl::cache_type cache_type;

  WASMOSCacheConfig(U_32 numLocks, WASMOSCacheConfigOptions* configOptions, UDATA osPageSize)
    : OSCacheConfigImpl(numLocks)
    , _layout(new (PERSISTENT_NEW) WASMOSCacheLayout<header_type>(osPageSize, osPageSize > 0))
  {
    WASMOSCacheHeader<header_type>* header = dynamic_cast<WASMOSCacheHeader<header_type>*>(_layout->operator[](HEADER_REGION_ID));
    header->setConfigOptions(configOptions);
  }

  void* getDataSectionLocation() override;
  U_32 getDataSectionSize() override;
  U_32* getDataLengthFieldLocation() override;
  U_64* getInitCompleteLocation() override;
  bool setCacheInitComplete() override;
  void nullifyRegions() override;
  U_32 getCacheSize() override;
  U_32* getCacheSizeFieldLocation() override;
  void detachRegions() override;
  bool isCacheHeaderValid() override;
  
  virtual void serializeCacheLayout(OSCache* osCache, void* blockAddress, U_32 cacheSize);
  virtual void initializeCacheLayout(OSCache* osCache, void* blockAddress, U_32 cacheSize);    
  
  using OSCacheConfigImpl::releaseLock;
  
private:
  friend class WASMOSCache<cache_type>;
  
  WASMOSCacheLayout<header_type>* _layout;
};

#endif
