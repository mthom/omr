//#include <sys/mman.h>
#include "WASMCompositeCache.hpp"
#include "WASMOSCacheConfig.hpp"

static OMRPortLibrary* initializePortLibrary()
{
  static OMRPortLibrary library;
  
  omrthread_init_library();
  omrthread_t self;
  omrthread_attach_ex(&self, J9THREAD_ATTR_DEFAULT);

  omrport_init_library(&library, sizeof(OMRPortLibrary));
  omrthread_detach(self);

  return &library;
}

WASMCompositeCache::WASMCompositeCache(const char* cacheName, const char* cachePath)
  : _osCache(new (PERSISTENT_NEW) WASMOSCache<OSMemoryMappedCache>(initializePortLibrary(),
								   cacheName,
								   cachePath,
								   5,
								   new (PERSISTENT_NEW) WASMOSCacheConfigOptions()))
  , _readerCount(SynchronizedCacheCounter(_osCache->headerRegion(),
					  _osCache->readerCountFocus()))
  , _crcChecker(CacheCRCChecker(_osCache->headerRegion(),
				_osCache->crcFocus(),
				MAX_CRC_SAMPLES))
  , _codeUpdatePtr(OSCacheBumpRegionFocus<WASMCacheEntry>(_osCache->dataSectionRegion(),
							  (WASMCacheEntry*) _osCache->dataSectionRegion()->regionStartAddress()))
  , _relocationData(NULL)
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
      
      _codeUpdatePtr += descriptor.entry->codeLength;
      _codeUpdatePtr += sizeof(WASMCacheEntry);
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

UDATA WASMCompositeCache::baseSharedCacheAddress(){
  return reinterpret_cast<UDATA>(_osCache->headerRegion()->regionStartAddress());
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

  // now write the relocation record to the cache.
  uint32_t relocationRecordSize = sizeof(uint32_t);
  uint32_t tempNull = 0;
  if(_relocationData) {
    relocationRecordSize = static_cast<uint32_t>(*_relocationData);
  } else {
    _relocationData = reinterpret_cast<uint8_t*>(&tempNull);
  }
  
  memcpy(_codeUpdatePtr, _relocationData, relocationRecordSize);
  _relocationData = nullptr;
  _codeUpdatePtr+=relocationRecordSize;
  
  return true;
}

//TODO: should copy to the code cache (not scc) when code cache becomes available
void *WASMCompositeCache::loadCodeEntry(const char *methodName, U_32 &codeLength, uint8_t *&relocationHeader) {
//if(!_loadedMethods[methodName]){
    WASMCacheEntry *entry = _codeEntries[methodName];
    if(entry) {
      uint8_t *bytePointer = reinterpret_cast<uint8_t *>(entry);
      relocationHeader = bytePointer+sizeof(WASMCacheEntry)+entry->codeLength;
      codeLength = entry->codeLength;
      entry++;
    }
    return entry;
//  void * methodArea =  mmap(NULL,
//            codeLength,
//            PROT_READ | PROT_WRITE | PROT_EXEC,
//            MAP_ANONYMOUS | MAP_PRIVATE,
//            0,
//            0);
//  _loadedMethods[methodName] = methodArea;
//  memcpy(methodArea,entry,codeLength);
//}
//return _loadedMethods[methodName];
}

void WASMCompositeCache::storeCallAddressToHeaders(void *calleeMethod,size_t methodNameTemplateOffset,void *calleeCodeCacheAddress){
    WASMCacheEntry *callee = reinterpret_cast<WASMCacheEntry *>(calleeMethod);
    callee--;
    UDATA relativeCalleeNameOffset = reinterpret_cast<UDATA>(&callee->methodName) - this->baseSharedCacheAddress();
    for(auto entry:_codeEntries){
      U_32 codeLength = entry.second->codeLength;
      char *methodName = reinterpret_cast<char *>(entry.second)+sizeof(WASMCacheEntry)+codeLength;
      if(*methodName){
	methodName = methodName+methodNameTemplateOffset;
	if(*reinterpret_cast<UDATA *>(methodName)==relativeCalleeNameOffset){
//	  *methodName = reinterpret_cast<UDATA>(calleeCodeCacheAddress);
	  memcpy(methodName,&calleeCodeCacheAddress,sizeof(UDATA));
	}
      }
    }
}

bool WASMCompositeCache::checkTime(uint64_t moduleTime) {
    return _osCache->checkTime(moduleTime);
}
