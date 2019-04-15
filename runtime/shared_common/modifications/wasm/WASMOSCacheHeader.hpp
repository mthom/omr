#if !defined(WASM_OS_CACHE_HEADER_HPP_INCLUDED)
#define WASM_OS_CACHE_HEADER_HPP_INCLUDED

#include "OSCacheContiguousRegion.hpp"
#include "OSMemoryMappedCacheHeader.hpp"
#include "OSSharedMemoryCacheHeader.hpp"
#include "WASMOSCacheConfigOptions.hpp"
#include "WASMOSCacheHeaderMapping.hpp"

#include "env/TRMemory.hpp"

template <class OSCacheHeader>
class WASMOSCacheHeader;

template <>
class WASMOSCacheHeader<OSMemoryMappedCacheHeader>: public OSMemoryMappedCacheHeader,
						    virtual public OSCacheContiguousRegion
{
public:
  TR_ALLOC(TR_Memory::SharedCacheRegion)

  WASMOSCacheHeader(WASMOSCacheLayout<OSMemoryMappedCacheHeader>* layout, int regionID,
		    bool pageBoundaryAligned);

  void refresh(OMRPortLibrary* library) override;
  void create(OMRPortLibrary* library) override;

  using OSSharedMemoryCacheHeader::regionStartAddress;
  
  UDATA regionSize() const override;

protected:
  WASMOSCacheConfigOptions* _configOptions;
};

template <>
class WASMOSCacheHeader<OSSharedMemoryCacheHeader>: public OSSharedMemoryCacheHeader,
						    virtual public OSCacheContiguousRegion
{
public:
  TR_ALLOC(TR_Memory::SharedCacheRegion)

  WASMOSCacheHeader(WASMOSCacheLayout<OSSharedMemoryCacheHeader>* layout, int regionID,
		    bool pageBoundaryAligned);
  
  void refresh(OMRPortLibrary* library, bool inDefaultControlDir) override;
  void create(OMRPortLibrary* library, bool inDefaultControlDir) override;

  using OSSharedMemoryCacheHeader::regionStartAddress;
  
  UDATA regionSize() const override;

protected:
  WASMOSCacheConfigOptions* _configOptions;  
};

#endif
