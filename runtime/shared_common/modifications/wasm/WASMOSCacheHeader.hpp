#if !defined(WASM_OSCACHE_HEADER_HPP_INCLUDED)
#define WASM_OSCACHE_HEADER_HPP_INCLUDED

#include "OSCacheContiguousRegion.hpp"
#include "OSMemoryMappedCacheHeader.hpp"
#include "OSSharedMemoryCacheHeader.hpp"
#include "WASMOSCacheConfigOptions.hpp"
#include "WASMOSCacheHeaderMapping.hpp"
#include "WASMOSCacheLayout.hpp"

#include "env/TRMemory.hpp"

#include "omr.h"

template <class OSCacheHeader>
class WASMOSCacheHeader;

template <>
class WASMOSCacheHeader<OSMemoryMappedCacheHeader>: public OSMemoryMappedCacheHeader,
						    virtual public OSCacheContiguousRegion
{
public:
  TR_ALLOC(TR_Memory::SharedCacheRegion)

  WASMOSCacheHeader(WASMOSCacheLayout<OSMemoryMappedCacheHeader>* layout,
		    WASMOSCacheConfigOptions* configOptions,
		    int regionID, bool pageBoundaryAligned)
    : OSMemoryMappedCacheHeader(5, new WASMOSCacheHeaderMappingImpl<OSMemoryMappedCacheHeader>())
    , OSCacheContiguousRegion(layout, regionID, pageBoundaryAligned)
    , _configOptions(configOptions)
  {}

  void refresh(OMRPortLibrary* library) override;
  void create(OMRPortLibrary* library) override;

  using OSMemoryMappedCacheHeader::regionStartAddress;

  UDATA regionSize() const override {
    return OSMemoryMappedCacheHeader::regionSize() + sizeof(volatile UDATA) + sizeof(UDATA);
  }

protected:
  WASMOSCacheHeaderMapping<OSMemoryMappedCacheHeader>* derivedMapping();
  
  WASMOSCacheConfigOptions* _configOptions;
};

template <>
class WASMOSCacheHeader<OSSharedMemoryCacheHeader>: public OSSharedMemoryCacheHeader,
						    virtual public OSCacheContiguousRegion
{
public:
  TR_ALLOC(TR_Memory::SharedCacheRegion)

  WASMOSCacheHeader(WASMOSCacheLayout<OSSharedMemoryCacheHeader>* layout,
		    WASMOSCacheConfigOptions* configOptions,		    
		    int regionID, bool pageBoundaryAligned)
    : OSSharedMemoryCacheHeader(new WASMOSCacheHeaderMappingImpl<OSSharedMemoryCacheHeader>())
    , OSCacheContiguousRegion(layout, regionID, pageBoundaryAligned)
    , _configOptions(configOptions)
  {}
  
  void refresh(OMRPortLibrary* library, bool inDefaultControlDir) override;
  void create(OMRPortLibrary* library, bool inDefaultControlDir) override;

  using OSSharedMemoryCacheHeader::regionStartAddress;  

  UDATA regionSize() const override {
    return OSSharedMemoryCacheHeader::regionSize() + sizeof(volatile UDATA) + sizeof(UDATA);
  }

protected:
  WASMOSCacheHeaderMapping<OSSharedMemoryCacheHeader>* derivedMapping();
  
  WASMOSCacheConfigOptions* _configOptions;  
};

#endif
