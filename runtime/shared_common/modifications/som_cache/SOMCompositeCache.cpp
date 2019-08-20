//#include <sys/mman.h>
#include "SOMCompositeCache.hpp"
#include "SOMOSCacheConfig.hpp"

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
  , _readerCount(SynchronizedCacheCounter(_osCache.headerRegion(),
					  _osCache.readerCountFocus()))
  , _crcChecker(CacheCRCChecker(_osCache.headerRegion(),
				_osCache.crcFocus(),
				MAX_CRC_SAMPLES))
  , _codeUpdatePtr(OSCacheBumpRegionFocus<SOMCacheEntry>(_osCache.dataSectionRegion(),
							 (SOMCacheEntry*) _osCache.dataSectionRegion()->regionStartAddress()))
  , _relocationData(NULL)
{
  populateTables();
}

void SOMCompositeCache::populateTables()
{
  SOMCacheEntry* dataSectionEnd = (SOMCacheEntry*) _osCache.dataSectionRegion()->regionEnd();
  SOMDataSectionEntryIterator iterator = constructEntryIterator(dataSectionEnd - sizeof(SOMCacheEntry));

  while(true) {
    SOMCacheEntryDescriptor descriptor = iterator.next();

    if(descriptor) {
      _codeEntries[descriptor.entry->methodName] = descriptor.entry;

      _codeUpdatePtr += descriptor.entry->codeLength;
      _codeUpdatePtr += sizeof(SOMCacheEntry);
      _codeUpdatePtr += descriptor.relocationRecordSize;
    } else {
      break;
    }
  }
}

// limit should reflect the true limit of the cache at
// initialization. Read in the cache size, modify accordingly.  By an
// argument, whose default should be _codeUpdatePtr.
SOMDataSectionEntryIterator
SOMCompositeCache::constructEntryIterator(SOMCacheEntry* delimiter)
{
  OSCacheRegion* dataSectionRegion = _osCache.dataSectionRegion();
  SOMCacheEntry* dataSectionStartAddress = (SOMCacheEntry*) dataSectionRegion->regionStartAddress();

  OSCacheBumpRegionFocus<SOMCacheEntry> focus(dataSectionRegion, dataSectionStartAddress);
  OSCacheBumpRegionFocus<SOMCacheEntry> limit(dataSectionRegion, delimiter);

  return SOMDataSectionEntryIterator(focus, limit);
}

UDATA SOMCompositeCache::baseSharedCacheAddress(){
  return reinterpret_cast<UDATA>(_osCache.headerRegion()->regionStartAddress());
}

bool SOMCompositeCache::startup(const char* cacheName, const char* ctrlDirName)
{
  if(!_osCache.startup(cacheName, ctrlDirName))
    return false;
}

UDATA SOMCompositeCache::dataSectionFreeSpace()
{
  SOMCacheEntry* startAddress = (SOMCacheEntry*) _osCache.dataSectionRegion()->regionStartAddress();
  UDATA dataSectionSize = _osCache.dataSectionRegion()->regionSize();

  return dataSectionSize - (UDATA) (_codeUpdatePtr - startAddress);
}

// find space for, and stores, a code entry. if it fails at any point,
// simply return 0.
bool SOMCompositeCache::storeEntry(const char* methodName, void* data, U_32 allocSize)
{
  //UDATA allocSize = sizeof(SOMCacheEntry) + codeLength;
  UDATA freeSpace = dataSectionFreeSpace();

  if(freeSpace < allocSize) {
    return false;
  }

  // yes, there's an extraneous string copy done here, buuuht, that is fine for now.
  SOMCacheEntry entry(methodName, allocSize);
  SOMCacheEntry* entryLocation = _codeUpdatePtr++;

  memcpy(entryLocation, &entry, sizeof(SOMCacheEntry));
  //memcpy(_codeUpdatePtr, codeLocation, codeLength);

  //_codeUpdatePtr += codeLength;

  _codeEntries[methodName] = entryLocation;

  memcpy(_codeUpdatePtr,data,allocSize);
  _codeUpdatePtr += allocSize;

  // now write the relocation record to the cache.
  /*size_t relocationRecordSize = static_cast<size_t>(*_relocationData);
  
  memcpy(_codeUpdatePtr, _relocationData, relocationRecordSize);
  _codeUpdatePtr+=relocationRecordSize;*/
  
  return true;
}

//TODO: should copy to the code cache (not scc) when code cache becomes available
void *SOMCompositeCache::loadEntry(const char *methodName) {
//if(!_loadedMethods[methodName]){
    SOMCacheEntry *entry = _codeEntries[methodName];

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
    void *rawData = NULL;
    if (entry)
      rawData = (void*) (entry+1);
    return rawData;
}

void SOMCompositeCache::storeCallAddressToHeaders(void *calleeMethod,size_t methodNameTemplateOffset,void *calleeCodeCacheAddress){
    SOMCacheEntry *callee = reinterpret_cast<SOMCacheEntry *>(calleeMethod);
    callee--;
    UDATA relativeCalleeNameOffset = reinterpret_cast<UDATA>(&callee->methodName) - this->baseSharedCacheAddress();
    for(auto entry:_codeEntries){
      U_32 codeLength = entry.second->codeLength;
      char *methodName = reinterpret_cast<char *>(entry.second)+sizeof(SOMCacheEntry)+codeLength;
      if(*methodName){
	methodName = methodName+methodNameTemplateOffset;
	if(*reinterpret_cast<UDATA *>(methodName)==relativeCalleeNameOffset){
//	  *methodName = reinterpret_cast<UDATA>(calleeCodeCacheAddress);
	  memcpy(methodName,&calleeCodeCacheAddress,sizeof(UDATA));
	}
      }
    }
}
