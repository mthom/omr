#if !defined(SOM_OSCACHE_HEADER_MAPPING_HPP_INCLUDED)
#define SOM_OSCACHE_HEADER_MAPPING_HPP_INCLUDED

#include "CacheHeaderMappingImpl.hpp"
#include "OSMemoryMappedCacheHeaderMapping.hpp"
#include "OSSharedMemoryCacheHeaderMapping.hpp"

template <class OSCacheHeader>
struct SOMOSCacheHeaderMapping: CacheHeaderMapping<OSCacheHeader>
{
  typename CacheHeaderMapping<OSCacheHeader>::mapping_type _mapping;  
  U_32 _cacheSize;
  volatile UDATA _readerCount;
  UDATA _cacheInitComplete;
  UDATA _cacheCrc;
  U_32 _dataSectionSize; // the size of the data section.
  UDATA _assumptionID;
  UDATA _card;
  U_32 _metadataUpdateOffset;
  U_32 _metadataSectionSize;
  U_64 _lastAssumptionID;

  UDATA size() const {
    return sizeof(SOMOSCacheHeaderMapping<OSCacheHeader>);
  }
};

template <class OSCacheHeader>
class SOMOSCacheHeaderMappingImpl: public CacheHeaderMappingImpl<OSCacheHeader>
{
public:
  virtual typename CacheHeaderMapping<OSCacheHeader>::mapping_type* baseMapping() {
    return &static_cast<SOMOSCacheHeaderMapping<OSCacheHeader>*>(this->_mapping)->_mapping;
  }
};

#endif
