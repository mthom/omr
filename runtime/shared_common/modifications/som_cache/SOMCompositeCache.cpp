//#include <sys/mman.h>
#include "SOMCompositeCache.hpp"
#include "SOMOSCacheConfig.hpp"

#include <cstring>

using ItemHeader = SOMCacheMetadataItemHeader;

static OMRPortLibrary* initializePortLibrary()
{
  static OMRPortLibrary library;

  omrthread_init_library();
  static omrthread_t self;
  omrthread_attach_ex(&self, J9THREAD_ATTR_DEFAULT);

  omrport_init_library(&library, sizeof(OMRPortLibrary));
  //omrthread_detach(self);

  return &library;
}

SOMCompositeCache::SOMCompositeCache(const char* cacheName, const char* cachePath)
  : _configOptions(300 * 1024 * 1024)
  , _config(5, &_configOptions, 0)
  , _osCache(initializePortLibrary(), cacheName, cachePath, 5, &_config, &_configOptions, 0)
  , _dataSectionReaderCount(_osCache.headerRegion(), _osCache.dataSectionReaderCountFocus())
  , _metadataSectionReaderCount(_osCache.headerRegion(), _osCache.metadataSectionReaderCountFocus())
  , _crcChecker(_osCache.headerRegion(), _osCache.crcFocus(), MAX_CRC_SAMPLES)
  , _codeUpdatePtr(_osCache.dataSectionRegion(), (SOMCacheEntry*) _osCache.dataSectionRegion()->regionStartAddress())
  , _metadataUpdatePtr(_osCache.metadataSectionRegion(),
		      (ItemHeader*) _osCache.metadataSectionRegion()->regionStartAddress())
{
   //TODO: claim the header lock here and release when done.
   populateTables();

   _osCache.acquireHeaderWriteLock();
   _metadataUpdatePtr += *_osCache.metadataSectionSizeFieldOffset();
   _osCache.releaseHeaderWriteLock();
}

void SOMCompositeCache::populateTables()
{
   _dataSectionReaderCount.incrementCount(_osCache);

   SOMCacheEntry* dataSectionEnd = (SOMCacheEntry*) _osCache.dataSectionRegion()->regionEnd();
   SOMDataSectionEntryIterator iterator = constructDataSectionEntryIterator(dataSectionEnd - sizeof(SOMCacheEntry));

   while(true) {
     SOMCacheEntryDescriptor descriptor = iterator.next();

     if (descriptor) {
        _codeEntries[descriptor.entry->methodName] = descriptor.entry;

	_codeUpdatePtr += descriptor.entry->codeLength;
	_codeUpdatePtr += sizeof(SOMCacheEntry);
	_codeUpdatePtr += descriptor.relocationRecordSize;
     } else {
        break;
     }
   }

   _dataSectionReaderCount.decrementCount(_osCache);
}

bool
SOMCompositeCache::createdNewCache()
{
   return _osCache._initContext->creatingNewCache();
}

SOMCacheMetadataEntryIterator
SOMCompositeCache::constructPreludeSectionEntryIterator()
{
   return {_osCache.preludeSectionRegion()};
}

SOMCacheMetadataEntryIterator
SOMCompositeCache::constructMetadataSectionEntryIterator()
{
   _osCache.acquireHeaderWriteLock();
   updateMetadataPtr();
   _osCache.releaseHeaderWriteLock();

   return {_osCache.metadataSectionRegion(), _metadataUpdatePtr};
}

// limit should reflect the true limit of the cache at
// initialization. Read in the cache size, modify accordingly.  By an
// argument, whose default should be _codeUpdatePtr.
SOMDataSectionEntryIterator
SOMCompositeCache::constructDataSectionEntryIterator(SOMCacheEntry* delimiter)
{
   OSCacheContiguousRegion* dataSectionRegion = _osCache.dataSectionRegion();
   SOMCacheEntry* dataSectionStartAddress = (SOMCacheEntry*) dataSectionRegion->regionStartAddress();

   OSCacheRegionBumpFocus<SOMCacheEntry> focus(dataSectionRegion, dataSectionStartAddress);
   OSCacheRegionBumpFocus<SOMCacheEntry> limit(dataSectionRegion, delimiter);

   return SOMDataSectionEntryIterator(focus, limit);
}

UDATA SOMCompositeCache::baseSharedCacheAddress()
{
   return reinterpret_cast<UDATA>(_osCache.headerRegion()->regionStartAddress());
}

bool SOMCompositeCache::startup(const char* cacheName, const char* ctrlDirName)
{
   return _osCache.startup(cacheName, ctrlDirName);
}

UDATA SOMCompositeCache::dataSectionFreeSpace()
{
   SOMCacheEntry* startAddress = (SOMCacheEntry*) _osCache.dataSectionRegion()->regionStartAddress();
   UDATA dataSectionSize = _osCache.dataSectionRegion()->regionSize();

   return dataSectionSize - (UDATA) (_codeUpdatePtr - startAddress);
}

void SOMCompositeCache::copyPreludeBuffer(void* data, size_t size)
{
   _osCache.acquireHeaderWriteLock();

   const auto* preludeRegion = _osCache.preludeSectionRegion();
   auto preludeSectionSize = *_osCache.preludeSectionSizeFieldOffset();

   if (preludeSectionSize == 0) {
      _osCache.acquireLock(PRELUDE_SECTION_LOCK_ID);

      TR_ASSERT(size <= preludeRegion->regionSize(),
		"copyPreludeBuffer: size of prelude metadata cannot exceed prelude region size");
      
      memcpy(preludeRegion->regionStartAddress(), data, size);
      *_osCache.preludeSectionSizeFieldOffset() = size;
      
      _osCache.releaseLock(PRELUDE_SECTION_LOCK_ID);
   }
   
   _osCache.releaseHeaderWriteLock();
}

// ASSUMES: the header write lock is held.
void SOMCompositeCache::updateMetadataPtr()
{
   auto oldSize = *_osCache.metadataSectionSizeFieldOffset();
   auto startAddress = _osCache.metadataSectionRegion()->regionStartAddress();

   _metadataUpdatePtr = reinterpret_cast<SOMCacheMetadataItemHeader*>(static_cast<uint8_t*>(startAddress) + oldSize);
}

void SOMCompositeCache::copyMetadata(void* data, size_t size)
{
   _osCache.acquireHeaderWriteLock();
   _osCache.acquireLock(METADATA_SECTION_LOCK_ID);

   updateMetadataPtr();

   memcpy(_metadataUpdatePtr, data, size);
   _metadataUpdatePtr += size;

   *_osCache.metadataSectionSizeFieldOffset() += size;

   _osCache.releaseLock(METADATA_SECTION_LOCK_ID);
   _osCache.releaseHeaderWriteLock();
}

// find space for, and stores, a code entry. if it fails at any point,
// simply return 0.
bool SOMCompositeCache::storeEntry(const char* methodName, void* data, U_32 allocSize)
{
  UDATA freeSpace = dataSectionFreeSpace();

  if (freeSpace < allocSize + sizeof(SOMCacheEntry)) {
     return false;
  }

  // yes, there's an extraneous string copy done here, buuuht, that is fine for now.
  SOMCacheEntry entry(methodName, allocSize);
  SOMCacheEntry* entryLocation = _codeUpdatePtr++;

  memcpy(entryLocation, &entry, sizeof(SOMCacheEntry));
  _codeEntries[methodName] = entryLocation;

  memcpy(_codeUpdatePtr, data, allocSize);
  _codeUpdatePtr += allocSize;

  return true;
}

//TODO: should copy to the code cache (not scc) when code cache becomes available
void *SOMCompositeCache::loadEntry(const char *methodName)
{
    SOMCacheEntry *entry = _codeEntries[methodName];

    void *rawData = NULL;
    if (entry) rawData = (void*) (entry+1);
    return rawData;
}

U_64 SOMCompositeCache::lastAssumptionID()
{
    _osCache.acquireHeaderWriteLock();
    auto offset = *_osCache.lastAssumptionIDOffset();
    _osCache.releaseHeaderWriteLock();

    return offset;
}

void SOMCompositeCache::setLastAssumptionID(U_64 assumptionID)
{
    _osCache.acquireHeaderWriteLock();
    *_osCache.lastAssumptionIDOffset() = assumptionID;
    _osCache.releaseHeaderWriteLock();
}

void SOMCompositeCache::storeCallAddressToHeaders(void *calleeMethod, size_t methodNameTemplateOffset, void *calleeCodeCacheAddress)
{
    SOMCacheEntry *callee = reinterpret_cast<SOMCacheEntry *>(calleeMethod);
    callee--;

    UDATA relativeCalleeNameOffset = reinterpret_cast<UDATA>(&callee->methodName) - this->baseSharedCacheAddress();

    for (auto entry : _codeEntries)
    {
       U_32 codeLength = entry.second->codeLength;
       char *methodName = reinterpret_cast<char *>(entry.second) + sizeof(SOMCacheEntry)+codeLength;

       if (*methodName)
       {
	  methodName = methodName+methodNameTemplateOffset;

	  if (*reinterpret_cast<UDATA *>(methodName)==relativeCalleeNameOffset)
	  {
//	    *methodName = reinterpret_cast<UDATA>(calleeCodeCacheAddress);
	     memcpy(methodName, &calleeCodeCacheAddress, sizeof(UDATA));
	  }
       }
    }
}
