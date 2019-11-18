#include "OSCache.hpp"
#include "OSMemoryMappedCacheConfig.hpp"
#include "OSSharedMemoryCacheConfig.hpp"

#include "SOMOSCache.hpp"
#include "SOMOSCacheConfig.hpp"
#include "SOMOSCacheHeader.hpp"
#include "SOMOSCacheHeaderMapping.hpp"
#include "SOMOSCacheLayout.hpp"

template <class OSCacheConfigImpl>
SOMOSCacheConfig<OSCacheConfigImpl>::SOMOSCacheConfig(U_32 numLocks, SOMOSCacheConfigOptions* configOptions, UDATA osPageSize)
  : OSCacheConfigImpl(numLocks)
  , _layout(osPageSize, osPageSize > 0)
{
   this->_header = _layout.getHeader();
   _layout.getHeader()->setConfigOptions(configOptions);    
}

template <class OSCacheConfigImpl>
SOMOSCacheHeader<typename OSCacheConfigImpl::header_type>*
SOMOSCacheConfig<OSCacheConfigImpl>::getHeader()
{
   return _layout.getHeader();
}

template <class OSCacheConfigImpl>
void SOMOSCacheConfig<OSCacheConfigImpl>::serializeCacheLayout(OSCache* osCache, void* blockAddress, U_32 size)
{
   _layout.init(blockAddress, size);

   OSCacheRegionSerializer* serializer = osCache->constructSerializer();

   for (int i = 0; i < _layout.numberOfRegions(); ++i) {
      _layout[i]->serialize(serializer);
   }

   this->_mapping = this->_header->baseMapping();
   delete serializer;
}

template <class OSCacheConfigImpl>
void SOMOSCacheConfig<OSCacheConfigImpl>::initializeCacheLayout(OSCache* osCache, void* blockAddress, U_32 size)
{
   _layout.init(blockAddress, size);

   OSCacheRegionInitializer* initializer = osCache->constructInitializer();

   for(int i = 0; i < _layout.numberOfRegions(); ++i) {
      _layout[i]->initialize(initializer);
   }

   this->_mapping = this->_header->baseMapping();
   delete initializer;
}

template <class OSCacheConfigImpl>
void SOMOSCacheConfig<OSCacheConfigImpl>::nullifyRegions()
{
   memset(_layout[HEADER_REGION_ID]->regionStartAddress(), 0,
	  _layout[HEADER_REGION_ID]->regionSize());
   memset(_layout[DATA_SECTION_REGION_ID]->regionStartAddress(), 0,
	  _layout[DATA_SECTION_REGION_ID]->regionSize());
   memset(_layout[METADATA_REGION_ID]->regionStartAddress(), 0,
	  _layout[METADATA_REGION_ID]->regionSize());
}

template <class OSCacheConfigImpl>
U_32 SOMOSCacheConfig<OSCacheConfigImpl>::getDataSectionSize() {
   return _layout[DATA_SECTION_REGION_ID]->regionSize();
}

template <class OSCacheConfigImpl>
void* SOMOSCacheConfig<OSCacheConfigImpl>::getDataSectionLocation() {
   return _layout[DATA_SECTION_REGION_ID]->regionStartAddress();
}

template <class OSCacheConfigImpl>
U_32* SOMOSCacheConfig<OSCacheConfigImpl>::getDataLengthFieldLocation() {
   U_32 offset = offsetof(SOMOSCacheHeaderMapping<header_type>, _dataSectionSize);
   return ((U_32*) _layout[HEADER_REGION_ID]->regionStartAddress() + offset);
}

template <class OSCacheConfigImpl>
U_64* SOMOSCacheConfig<OSCacheConfigImpl>::getInitCompleteLocation() {
   U_64 offset = offsetof(SOMOSCacheHeaderMapping<header_type>, _cacheInitComplete);
   return ((U_64*) _layout[HEADER_REGION_ID]->regionStartAddress() + offset);
}

template <class OSCacheConfigImpl>
bool SOMOSCacheConfig<OSCacheConfigImpl>::setCacheInitComplete() {
   //  SOMOSCacheHeader<header_type>* header = dynamic_cast<SOMOSCacheHeader<header_type>*>(_layout[HEADER_REGION_ID]);
   _layout.getHeader()->derivedMapping()->_cacheInitComplete = 1;
   return true;
}

template <class OSCacheConfigImpl>
U_32 SOMOSCacheConfig<OSCacheConfigImpl>::getCacheSize() {
   return getDataSectionSize() + _layout[HEADER_REGION_ID]->regionSize();
}

template <class OSCacheConfigImpl>
U_32* SOMOSCacheConfig<OSCacheConfigImpl>::getCacheSizeFieldLocation() {
   U_32 offset = offsetof(SOMOSCacheHeaderMapping<header_type>, _cacheSize);
   return (U_32*) _layout[HEADER_REGION_ID]->regionStartAddress() + offset;
}

template <class OSCacheConfigImpl>
void SOMOSCacheConfig<OSCacheConfigImpl>::detachRegions() {
   _layout[HEADER_REGION_ID]->adjustRegionStartAndSize(NULL, 0);
   _layout[DATA_SECTION_REGION_ID]->adjustRegionStartAndSize(NULL, 0);
   
   _layout.clearRegions();
}

template class SOMOSCacheConfig<OSMemoryMappedCacheConfig>;
template class SOMOSCacheConfig<OSSharedMemoryCacheConfig>;
