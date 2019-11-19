#include <type_traits>

#include "SOMCacheStats.hpp"

template <class OSCache>
void SOMCacheStats<OSCache>::getCacheStats()
{
    auto* cache = dynamic_cast<SOMOSCache<OSCache>*>(this->_cache);

    if (cache == nullptr || !cache->started()) {
       _cacheInfo = std::nullopt;
       return;
    }

    auto* headerMapping = cache->_config->getHeader()->derivedMapping();

    SOMOSCacheInfo cacheInfo;

    cacheInfo._isPersistent = std::is_same_v<OSMemoryMappedCache, OSCache>;
    cacheInfo._cacheSize = headerMapping->_cacheSize;
    cacheInfo._preludeSectionSize = headerMapping->_preludeSectionSize;
    cacheInfo._dataSectionSize = headerMapping->_dataSectionSize;
    cacheInfo._metadataSectionSize = headerMapping->_metadataSectionSize;
    cacheInfo._lastAssumptionID = headerMapping->_lastAssumptionID;
    cacheInfo._vmID = headerMapping->_vmCounter;

    _cacheInfo = cacheInfo;
}

template <class OSCache>
void SOMCacheStats<OSCache>::shutdownCache()
{
    OSCache::stats_type::shutdownCache();
    this->_cache = nullptr;
}

template <class OSCache>
const std::optional<SOMOSCacheInfo>& SOMCacheStats<OSCache>::cacheInfo()
{
    return _cacheInfo;
}

template class SOMCacheStats<OSMemoryMappedCache>;
template class SOMCacheStats<OSSharedMemoryCache>;
