#if !defined(WASM_OSCACHE_HEADER_MAPPING_HPP_INCLUDED)
#define WASM_OSCACHE_HEADER_MAPPING_HPP_INCLUDED

#include "CacheHeaderMappingImpl.hpp"
#include "OSMemoryMappedCacheHeaderMapping.hpp"
#include "OSSharedMemoryCacheHeaderMapping.hpp"

template <class OSCacheHeader>
struct WASMOSCacheHeaderMapping: CacheHeaderMapping<OSCacheHeader>
{
  U_32 _cacheSize;
  typename CacheHeaderMapping<OSCacheHeader>::mapping_type _mapping;
  volatile UDATA _readerCount;
  UDATA _cacheCrc;
  U_32 _dataSectionSize; // the size of the data section.
};

template <class OSCacheHeader>
class WASMOSCacheHeaderMappingImpl: public CacheHeaderMappingImpl<OSCacheHeader>
{
public:
  virtual typename CacheHeaderMapping<OSCacheHeader>::mapping_type* baseMapping() {
    return &static_cast<WASMOSCacheHeaderMapping<OSCacheHeader>*>(this->_mapping)->_mapping;
  } 
};

#endif
