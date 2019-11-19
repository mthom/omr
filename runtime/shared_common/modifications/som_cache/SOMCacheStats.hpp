#if !defined(SOM_CACHE_STATS_HPP_INCLUDED)
#define SOM_CACHE_STATS_HPP_INCLUDED

#include <optional>

#include "OSMemoryMappedCacheStats.hpp"
#include "OSSharedMemoryCacheStats.hpp"

#include "SOMOSCache.hpp"

struct SOMOSCacheInfo {
    U_32 _cacheSize;
    bool _isPersistent;
    U_32 _preludeSectionSize;
    U_32 _dataSectionSize;
    U_32 _metadataSectionSize;
    U_64 _lastAssumptionID;    
    U_32 _vmID;
};

template <class OSCache>
class SOMCacheStats: public OSCache::stats_type
{
public:
    SOMCacheStats(SOMOSCache<OSCache>* cache)
      : OSCache::stats_type(cache)
    {}

    virtual void getCacheStats();
    virtual void shutdownCache();

    const std::optional<SOMOSCacheInfo>& cacheInfo();
  
private:
    std::optional<SOMOSCacheInfo> _cacheInfo;
};

#endif
