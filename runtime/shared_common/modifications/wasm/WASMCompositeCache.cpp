#include "WASMCompositeCache.hpp"
#include "WASMOSCacheConfig.hpp"

WASMCompositeCache::WASMCompositeCache(WASMOSCache<OSMemoryMappedCache>* osCache, UDATA osPageSize)
  : _osCache(osCache)
  , _readerCount(_osCache->headerRegion(), _osCache->readerCountFocus())
  , _crcChecker(_osCache->headerRegion(), _osCache->crcFocus(), MAX_CRC_SAMPLES)
  , _codeUpdatePtr(_osCache->dataSectionRegion(), (U_8*) _osCache->dataSectionRegion()->regionStartAddress())
{}

bool WASMCompositeCache::startup(const char* cacheName, const char* ctrlDirName)
{    
  if(!_osCache->startup(cacheName, ctrlDirName))
    return false;  
}
