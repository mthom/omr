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
  volatile UDATA _dataSectionReaderCount;
  volatile UDATA _metadataSectionReaderCount;
  UDATA _cacheInitComplete;
  UDATA _cacheCrc;
  U_32 _preludeSectionSize;
  U_32 _dataSectionSize;
  U_32 _metadataSectionSize;
  U_64 _lastAssumptionID;
  U_32 _isLocked[5]; // offsets for the data section and metadata sections.
  U_32 _vmCounter; // the number of connected VMs.
  UDATA _writeHash;

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
