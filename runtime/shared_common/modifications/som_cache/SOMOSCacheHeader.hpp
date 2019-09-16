#if !defined(SOM_OSCACHE_HEADER_HPP_INCLUDED)
#define SOM_OSCACHE_HEADER_HPP_INCLUDED

#include "OSCacheContiguousRegion.hpp"
#include "OSMemoryMappedCacheConfig.hpp"
#include "OSMemoryMappedCacheHeader.hpp"
#include "OSSharedMemoryCacheConfig.hpp"
#include "OSSharedMemoryCacheHeader.hpp"
#include "SOMOSCacheConfigOptions.hpp"
#include "SOMOSCacheHeaderMapping.hpp"

#include "env/TRMemory.hpp"

#include "omr.h"

template <class>
class SOMOSCacheConfig;

template <class>
class SOMOSCacheLayout;

template <class>
class SOMOSCacheHeader;

template <>
class SOMOSCacheHeader<OSMemoryMappedCacheHeader>:
  virtual public OSCacheContiguousRegion,
          public OSMemoryMappedCacheHeader
{
public:
  TR_ALLOC(TR_Memory::SharedCacheRegion)

  SOMOSCacheHeader(int regionID, bool pageBoundaryAligned)
    : OSCacheContiguousRegion(nullptr, regionID, pageBoundaryAligned),
      OSMemoryMappedCacheHeader(5, new SOMOSCacheHeaderMappingImpl<OSMemoryMappedCacheHeader>())
  {}

  void refresh(OMRPortLibrary* library) override;
  void create(OMRPortLibrary* library) override;

  using OSMemoryMappedCacheHeader::regionStartAddress;

  UDATA regionSize() const override {
    return sizeof(SOMOSCacheHeaderMapping<OSMemoryMappedCacheHeader>);
  }

protected:
  friend class SOMOSCacheConfig<OSMemoryMappedCacheConfig>;
  friend class SOMOSCacheLayout<OSMemoryMappedCacheHeader>;

  SOMOSCacheHeaderMapping<OSMemoryMappedCacheHeader>* derivedMapping();

  void setConfigOptions(SOMOSCacheConfigOptions* configOptions) {
	_configOptions = configOptions;
  }

  SOMOSCacheConfigOptions* _configOptions;
};

template <>
class SOMOSCacheHeader<OSSharedMemoryCacheHeader>:
  virtual public OSCacheContiguousRegion,
          public OSSharedMemoryCacheHeader
{
public:
  TR_ALLOC(TR_Memory::SharedCacheRegion)

  SOMOSCacheHeader(int regionID, bool pageBoundaryAligned)
    : OSSharedMemoryCacheHeader(new SOMOSCacheHeaderMappingImpl<OSSharedMemoryCacheHeader>())
    , OSCacheContiguousRegion(nullptr, regionID, pageBoundaryAligned)
  {}

  void refresh(OMRPortLibrary* library, bool inDefaultControlDir) override;
  void create(OMRPortLibrary* library, bool inDefaultControlDir) override;

  using OSSharedMemoryCacheHeader::regionStartAddress;

  UDATA regionSize() const override {
    return sizeof(SOMOSCacheHeaderMapping<OSSharedMemoryCacheHeader>);
  }

protected:
  friend class SOMOSCacheConfig<OSSharedMemoryCacheConfig>;
  friend class SOMOSCacheLayout<OSSharedMemoryCacheHeader>;

  SOMOSCacheHeaderMapping<OSSharedMemoryCacheHeader>* derivedMapping();

  void setConfigOptions(SOMOSCacheConfigOptions* configOptions) {
	_configOptions = configOptions;
  }

  SOMOSCacheConfigOptions* _configOptions;
};

#endif
