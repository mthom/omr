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
{
  populateTables();
}

void WASMCompositeCache::populateTables()
{
  WASMCacheEntry* dataSectionEnd = (WASMCacheEntry*) _osCache->dataSectionRegion()->regionEnd();
  WASMDataSectionEntryIterator iterator = constructEntryIterator(dataSectionEnd - sizeof(WASMCacheEntry));

  while(true) {
    WASMCacheEntryDescriptor descriptor = iterator.next();

    if(descriptor) {
      _codeEntries[descriptor.entry->methodName] = descriptor.entry;
    } else {
      break;
    }
  }
}

// limit should reflect the true limit of the cache at
// initialization. Read in the cache size, modify accordingly.  By an
// argument, whose default should be _codeUpdatePtr.
WASMDataSectionEntryIterator
WASMCompositeCache::constructEntryIterator(WASMCacheEntry* delimiter)
{
  OSCacheRegion* dataSectionRegion = _osCache->dataSectionRegion();
  WASMCacheEntry* dataSectionStartAddress = (WASMCacheEntry*) dataSectionRegion->regionStartAddress();

  OSCacheBumpRegionFocus<WASMCacheEntry> focus(dataSectionRegion, dataSectionStartAddress);
  OSCacheBumpRegionFocus<WASMCacheEntry> limit(dataSectionRegion, delimiter);

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

  // yes, there's an extraneous string copy done here, buuuht, that is fine for now.
  WASMCacheEntry entry(methodName, codeLength);
  WASMCacheEntry* entryLocation = _codeUpdatePtr++;

  memcpy(entryLocation, &entry, sizeof(WASMCacheEntry));
  memcpy(_codeUpdatePtr, codeLocation, codeLength);

  _codeUpdatePtr += codeLength;

  _codeEntries[methodName] = entryLocation;

  return true;
}
