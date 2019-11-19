//#include <sys/mman.h>
#include "WASMCompositeCache.hpp"
#include "WASMOSCacheConfig.hpp"
#include "runtime/Runtime.hpp"
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
      _codeUpdatePtr += descriptor.relocationRecordSize;
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

// typedef struct AOTMethodHeader 
//    {
//       // at compile time, the constructor runs with four arguments, 
//       // relocationsSize, compiledCodeSize, compiledCodeStart and relocationsStart
//       // at loadtime we don't know anything, so we run a copy constructor with no 
//    public:
//       AOTMethodHeader(uint8_t* compiledCodeStart, uint32_t compiledCodeSize, uint8_t* relocationsStart, uint32_t relocationsSize):
//          compiledCodeStart(compiledCodeStart),
//          compiledCodeSize(compiledCodeSize),
//          relocationsStart(relocationsStart),
//          relocationsSize(relocationsSize)
//          {};
//       AOTMethodHeader(const AOTMethodHeader &original){ 
//          compiledCodeSize  = original.compiledCodeSize;
//          relocationsSize   = original.relocationsSize;
//          relocationsStart  = relocationsSize ? (uint8_t*) &original+sizeof(AOTMethodHeader)+compiledCodeSize : 0;
//          compiledCodeStart = (uint8_t*) &original+sizeof(AOTMethodHeader);
         
//          };
//       uint8_t* compiledCodeStart;
//       uint32_t compiledCodeSize;
//       uint8_t* relocationsStart;
//       uint32_t relocationsSize;
//       // uintptrj_t  exceptionTableStart;
//       // // Here, compiledDataStart is a pointer to any data persisted along with the
//       // // compiled code. offset to RelocationsTable points to Relocations, should
//       // // be equal 
//       // uintptrj_t compiledDataStart;
//       // uintptrj_t compiledDataSize;

   
//    } AOTMethodHeader;
// find space for, and stores, a code entry. if it fails at any point,
// simply return 0.
bool WASMCompositeCache::storeEntry(const char* elementName, void* data, uint32_t allocSize)
{
  UDATA freeSpace = dataSectionFreeSpace();

  if(freeSpace < allocSize) {
    return false;
  }

  // yes, there's an extraneous string copy done here, buuuht, that is fine for now.
  WASMCacheEntry entry(elementName, allocSize);
  WASMCacheEntry* entryLocation = _codeUpdatePtr++;

  memcpy(entryLocation, &entry, sizeof(WASMCacheEntry));
  // memcpy(_codeUpdatePtr, codeLocation, codeLength);
  // _codeUpdatePtr += codeLength;

  _codeEntries[elementName] = entryLocation;

  // // now write the relocation record to the cache.
  memcpy(_codeUpdatePtr,data,allocSize);
   _codeUpdatePtr += allocSize;

  // // memcpy(_codeUpdatePtr,hdr->compiledCodeStart,hdr->compiledCodeSize);
  // // _codeUpdatePtr += hdr->compiledCodeSize;
  // // memcpy(_codeUpdatePtr,hdr->relocationsStart,hdr->relocationsSize);
  // // _codeUpdatePtr += hdr->relocationsSize;
    // memcpy(_codeUpdatePtr,_relocationData,relocationRecordSize);
  // memcpy(_codeUpdatePtr, _relocationData, relocationRecordSize);
  // _relocationData = nullptr;
  // _codeUpdatePtr+=relocationRecordSize;
  
  return true;
}

//TODO: should copy to the code cache (not scc) when code cache becomes available
void* WASMCompositeCache::loadEntry(const char *elementName) {
//if(!_loadedMethods[methodName]){
    WASMCacheEntry *entry = _codeEntries[elementName];
    // if(entry) {
    //   uint8_t *bytePointer = reinterpret_cast<uint8_t *>(entry);
    //   relocationHeader = bytePointer+sizeof(WASMCacheEntry)+entry->codeLength;
    //   codeLength = entry->codeLength;
    //   entry++;
    // }
    void *rawData = NULL;
    if (entry)
      rawData = (void*) (entry+1);
    return rawData;
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