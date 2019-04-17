#include "WASMCompositeCache.hpp"
#include "WASMOSCacheConfig.hpp"

WASMCompositeCache::WASMCompositeCache(WASMOSCache<OSMemoryMappedCache>* osCache, UDATA osPageSize)
  : _osCache(osCache)
  , _readerCount(SynchronizedCacheCounter(_osCache->headerRegion(),
					  _osCache->readerCountFocus()))
  , _crcChecker(CacheCRCChecker(_osCache->headerRegion(),
				_osCache->crcFocus(),
				MAX_CRC_SAMPLES))
  , _codeUpdatePtr(OSCacheBumpRegionFocus<WASMCacheEntry>(_osCache->dataSectionRegion(),
							  (WASMCacheEntry*) _osCache->dataSectionRegion()->regionStartAddress()))
{}

WASMDataSectionEntryIterator
WASMCompositeCache::constructEntryIterator()
{
  OSCacheBumpRegionFocus<WASMCacheEntry> focus(_osCache->dataSectionRegion(),
					       (WASMCacheEntry*) _osCache->dataSectionRegion()->regionStartAddress());

  OSCacheBumpRegionFocus<WASMCacheEntry> limit(_osCache->dataSectionRegion(), _codeUpdatePtr);

  return WASMDataSectionEntryIterator(focus, limit);
}

bool WASMCompositeCache::startup(const char* cacheName, const char* ctrlDirName)
{
  if(!_osCache->startup(cacheName, ctrlDirName))
    return false;
}

UDATA WASMCompositeCache::dataSectionFreeSpace() const
{
  WASMCacheEntry* startAddress = (WASMCacheEntry*) _osCache->dataSectionRegion()->regionStartAddress();
  UDATA dataSectionSize = _osCache->dataSectionRegion()->regionSize();

  return dataSectionSize - (UDATA) (_codeUpdatePtr - startAddress);
}

// find space for, and stores, a code entry. if it fails at any point,
// simply return 0.
bool WASMCompositeCache::storeCodeEntry(const char* methodName, void* codeLocation, U_32 codeLength)
{
  UDATA allocSize = sizeof(WASMCacheEntry) + codeLength;
  UDATA freeSpace = dataSectionFreeSpace();

  if(freeSpace < allocSize) {
    return false;
  }

  WASMCacheEntry entry(methodName, codeLength);

  memcpy(_codeUpdatePtr++, &entry, sizeof(WASMCacheEntry));
  memcpy(_codeUpdatePtr, codeLocation, codeLength);

  _codeUpdatePtr += codeLength;

  return true;
}
