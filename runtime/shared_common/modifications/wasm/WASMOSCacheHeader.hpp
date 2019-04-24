#if !defined(WASM_OSCACHE_HEADER_HPP_INCLUDED)
#define WASM_OSCACHE_HEADER_HPP_INCLUDED

#include "OSCacheContiguousRegion.hpp"
#include "OSMemoryMappedCacheConfig.hpp"
#include "OSMemoryMappedCacheHeader.hpp"
#include "OSSharedMemoryCacheHeader.hpp"
#include "WASMOSCacheConfigOptions.hpp"
#include "WASMOSCacheHeaderMapping.hpp"

#include "env/TRMemory.hpp"

#include "omr.h"

template <class>
class WASMOSCacheConfig;

template <class>
class WASMOSCacheLayout;

template <class>
class WASMOSCacheHeader;

template <>
class WASMOSCacheHeader<OSMemoryMappedCacheHeader>: public OSMemoryMappedCacheHeader,
						    virtual public OSCacheContiguousRegion
{
public:
  TR_ALLOC(TR_Memory::SharedCacheRegion)

  WASMOSCacheHeader(WASMOSCacheLayout<OSMemoryMappedCacheHeader>* layout,
		    int regionID, bool pageBoundaryAligned)
    : OSMemoryMappedCacheHeader(5, new WASMOSCacheHeaderMappingImpl<OSMemoryMappedCacheHeader>())
    , OSCacheContiguousRegion((OSCacheLayout*) layout, regionID, pageBoundaryAligned)
  {}  
  
  void refresh(OMRPortLibrary* library) override;
  void create(OMRPortLibrary* library) override;

  using OSMemoryMappedCacheHeader::regionStartAddress;

  UDATA regionSize() const override {
    return OSMemoryMappedCacheHeader::regionSize() + sizeof(volatile UDATA) + sizeof(UDATA);
  }

protected:
  friend class WASMOSCacheConfig<OSMemoryMappedCacheConfig>;  
  friend class WASMOSCacheLayout<OSMemoryMappedCacheHeader>;
  
  WASMOSCacheHeaderMapping<OSMemoryMappedCacheHeader>* derivedMapping();
  void setConfigOptions(WASMOSCacheConfigOptions* configOptions) {
    _configOptions = configOptions;
  }
  
  WASMOSCacheConfigOptions* _configOptions;
};

template <>
class WASMOSCacheHeader<OSSharedMemoryCacheHeader>: public OSSharedMemoryCacheHeader,
						    virtual public OSCacheContiguousRegion
{
public:
  TR_ALLOC(TR_Memory::SharedCacheRegion)

  WASMOSCacheHeader(WASMOSCacheLayout<OSSharedMemoryCacheHeader>* layout,
		    int regionID, bool pageBoundaryAligned)
    : OSSharedMemoryCacheHeader(new WASMOSCacheHeaderMappingImpl<OSSharedMemoryCacheHeader>())
    , OSCacheContiguousRegion((OSCacheLayout*) layout, regionID, pageBoundaryAligned)
  {}

  void refresh(OMRPortLibrary* library, bool inDefaultControlDir) override;
  void create(OMRPortLibrary* library, bool inDefaultControlDir) override;

  using OSSharedMemoryCacheHeader::regionStartAddress;

  UDATA regionSize() const override {
    return OSSharedMemoryCacheHeader::regionSize() + sizeof(volatile UDATA) + sizeof(UDATA);
  }

protected:
  friend class WASMOSCacheLayout<OSSharedMemoryCacheHeader>;
  friend class WASMOSCacheConfig<OSSharedMemoryCacheHeader>;
  
  WASMOSCacheHeaderMapping<OSSharedMemoryCacheHeader>* derivedMapping();
  void setConfigOptions(WASMOSCacheConfigOptions* configOptions) {
    _configOptions = configOptions;
  }
  
  WASMOSCacheConfigOptions* _configOptions;
};

#endif
