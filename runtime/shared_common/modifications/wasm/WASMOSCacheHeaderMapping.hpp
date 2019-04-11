#if !defined(WASM_OS_CACHE_HEADER_MAPPING_HPP_INCLUDED)
#define WASM_OS_CACHE_HEADER_MAPPING_HPP_INCLUDED

#include "CacheHeaderImpl.hpp"
#include "OSCacheContiguousRegion.hpp"

template <class OSCacheHeader>
struct WASMOSCacheHeaderMapping: CacheHeaderMapping<OSCacheHeader>
{
public:
  U_32 _cacheSize;
  typename HeaderMapping<OSCacheHeader>::mapping_type _mapping;
  volatile UDATA _readerCount;
  UDATA _cacheCrc;
  U_32 _dataSectionSize; // the size of the data section.
};

template <class OSCacheHeader>
class WASMOSCacheHeaderMappingImpl: public CacheHeaderMappingImpl<OSCacheHeader>
{
public:
  virtual typename HeaderMapping<OSCacheHeader>::mapping_type* baseMapping() {
    return &static_cast<WASMOSCacheHeaderMapping<OSCacheHeader>*>(_mapping)->_mapping;
  }
};

#endif
