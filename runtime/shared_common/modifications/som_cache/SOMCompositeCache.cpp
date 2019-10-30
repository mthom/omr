//#include <sys/mman.h>
#include "SOMCompositeCache.hpp"
#include "SOMOSCacheConfig.hpp"

#include "Universe.h"

#include "omr.h"
#include "omrcfg.h"
#include "omrport.h"
#include "shrnls.h"
#include "ut_omrshr_mods.h"

#include "compiler/env/CompilerEnv.hpp"

#include <cstring>
#include <iostream>

#define WRITEHASH_MASK 0x000FFFFF
#define WRITEHASH_SHIFT 20

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
  , _codeUpdatePtr(_osCache.dataSectionRegion(),
		   (SOMCacheEntry*) _osCache.dataSectionRegion()->regionStartAddress())
  , _metadataUpdatePtr(_osCache.metadataSectionRegion(),
		       (ItemHeader*) _osCache.metadataSectionRegion()->regionStartAddress())
{
   populateTables();

   _osCache.acquireHeaderWriteLock();

   _osCache.incrementVMCounter();
   _metadataUpdatePtr += *_osCache.metadataSectionSizeFieldOffset();

   _osCache.releaseHeaderWriteLock();
}

UDATA SOMCompositeCache::tryResetWriteHash(UDATA hashValue)
{
   if (!_osCache.started()) {
     return false;
   }

   UDATA oldCachedHashValue = *_osCache.writeHashPtr();
   UDATA value = 0;
   UDATA maskedHash = hashValue & WRITEHASH_MASK;

   if (maskedHash == (oldCachedHashValue & WRITEHASH_MASK)) {// || (_lastFailedWHCount > FAILED_WRITEHASH_MAX_COUNT)) {
      setWriteHash(0);
//     _lastFailedWHCount = 0;
//     _lastFailedWriteHash = 0;
//     Trc_SHR_CC_tryResetWriteHash_Exit1(_commonCCInfo->vmID, maskedHash, _theca->writeHash);
      return 1; // we reset the hash.
   }

   return 0;
}

// DOES NOT ASSUME: caller holds the header write lock.
void SOMCompositeCache::setWriteHash(UDATA hashValue)
{
   UDATA oldCachedHashValue = *_osCache.writeHashPtr();
   UDATA value = 0;

   if (hashValue != 0) {
      value = (hashValue & WRITEHASH_MASK) | (_osCache.vmID() << WRITEHASH_SHIFT);
   }

   // unprotectHeaderReadWriteArea(currentThread, false);
   // compareSwapResult =
   VM_AtomicSupport::lockCompareExchange(_osCache.writeHashPtr(),
					 oldCachedHashValue,
					 value);
   // protectHeaderReadWriteArea(currentThread, false);
}

// DOES NOT ASSUME: caller holds the header write lock.
// this supposes peekForWriteHash returned true, and thus..
// we want to use the write hash.
UDATA SOMCompositeCache::testAndSetWriteHash(UDATA hashValue)
{
   if (!_osCache.started()) {
     return false;
   }

   UDATA cachedHashValue = *_osCache.writeHashPtr();
   UDATA maskedHash = hashValue & WRITEHASH_MASK;

   if (hashValue == 0) {
     setWriteHash(hashValue);
   } else if (maskedHash == (cachedHashValue & WRITEHASH_MASK)) {
     // the first WRITEHASH_MASK + 1 bits of the write hash encode
     // the actual hash value. The rest, to the left of that value,
     // encode the ID of the VM that wrote the hash to the cache header.

     // I love you, write hash! :-O
     UDATA vmWroteToCache = cachedHashValue >> WRITEHASH_SHIFT;

     if (_osCache.vmID() != vmWroteToCache) {
        // and, this signifies that another VM is storing the class (or failing that, one
        // with an identical hash) to the cache. Wait for it to finish, update the metadata
        // pointer, and load it.
        return 1;
     }
   }

   return 0; // this means we set the write hash.
}

bool SOMCompositeCache::peekForWriteHash()
{
   if (!_osCache.started()) {
     return false;
   }

   return _osCache.vmID() < _osCache.vmIDCounter();
}

void SOMCompositeCache::populateTables()
{
   enterReadMutex(_dataSectionReaderCount, DATA_SECTION_LOCK_ID);

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

   exitReadMutex(_dataSectionReaderCount);
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
   updateMetadataPtr();
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

// does an elaborate dance to increment the metadata section reader
// count, blocking if the cache is locked.
void SOMCompositeCache::enterReadMutex(SynchronizedCacheCounter& reader, UDATA lockID)
{
   reader.incrementCount(_osCache);

   if (isCacheLocked(lockID)) {
      reader.decrementCount(_osCache);

      // this will prevent another thread or VM from locking the cache
      // once the locking thread or VM unlocks it, allowing us to
      // delay the pending lock by incrementing the reader count.
      auto rc = _osCache.acquireLock(lockID);

      if (rc == 0) {
	 reader.incrementCount(_osCache);
	 rc = _osCache.releaseLock(lockID);
      }

      if (rc != 0) {
	 std::cerr << "acquire/release data section lock failed with rc " << rc << "\n";
      }
   }
}

void SOMCompositeCache::exitReadMutex(SynchronizedCacheCounter& reader)
{
   if (!_osCache.started()) {
      return;
   }

   reader.decrementCount(_osCache);
}

// the cache lock only applies to reading/writing code from the data
// section area. The metadata section is governed by the write hash.
bool SOMCompositeCache::isCacheLocked(UDATA lockID)
{
   return *_osCache.isLockedOffset(lockID);
}

// ASSUMES: the write lock under lockID is held. Should probably add an assert here.
void SOMCompositeCache::lockCache(SynchronizedCacheCounter& reader, UDATA lockID)
{
   auto* isLocked = _osCache.isLockedOffset(lockID);
   *isLocked = static_cast<U_32>(true);

   while (reader.count() > 0) {
       omrthread_sleep(5); // wait 5 milliseconds.
   }
}

void SOMCompositeCache::unlockCache(UDATA lockID)
{
   auto* isLocked = _osCache.isLockedOffset(lockID);
   *isLocked = static_cast<U_32>(false);
}

// assumes the read mutex for the metadata section is held.
void SOMCompositeCache::updateMetadataPtr()
{
   if (omrthread_monitor_init(&_refreshMutex, 0) != 0) {
      _refreshMutex = NULL;
      return; // that'ssss right, we simply fail.
   }
   
   auto startAddress = _osCache.metadataSectionRegion()->regionStartAddress();

   ObjectDeserializer deserialize(TR::Compiler->aotAdapter.getReverseLookupMap());
   SOMCacheMetadataEntryIterator it{_osCache.metadataSectionRegion()};
   
   it.fastForward(_metadataUpdatePtr.focus());

   GetUniverse()->ProcessLoadedClasses(deserialize, it);
   _metadataUpdatePtr = *it;

   omrthread_monitor_destroy(_refreshMutex);
   _refreshMutex = NULL;
}

void SOMCompositeCache::copyMetadata(const char* clazz, void* data, size_t size)
{
   UDATA hashValue = hash<const char*>{}(clazz);
   
   if (hashValue == *_osCache.writeHashPtr()) {
      enterReadMutex(_metadataSectionReaderCount, METADATA_SECTION_LOCK_ID);
      updateMetadataPtr();
      exitReadMutex(_metadataSectionReaderCount);
   } else {
      tryResetWriteHash(hashValue);

      _osCache.acquireLock(METADATA_SECTION_LOCK_ID);
      
      lockCache(_metadataSectionReaderCount, METADATA_SECTION_LOCK_ID);
      updateMetadataPtr();      

      memcpy(_metadataUpdatePtr, data, size);
      _metadataUpdatePtr += size;

      _osCache.acquireHeaderWriteLock();      
      *_osCache.metadataSectionSizeFieldOffset() += size; //TODO: again! use fetch_add.
      _osCache.releaseHeaderWriteLock();

      setWriteHash(0);
      unlockCache(METADATA_SECTION_LOCK_ID);
      
      _osCache.releaseLock(METADATA_SECTION_LOCK_ID);
   }     
}

void SOMCompositeCache::copyPreludeBuffer(void* data, size_t size)
{
   _osCache.acquireHeaderWriteLock();

   const auto* preludeRegion = _osCache.preludeSectionRegion();
   auto preludeSectionSize = *_osCache.preludeSectionSizeFieldOffset();

   if (preludeSectionSize == 0) {
      TR_ASSERT(size <= preludeRegion->regionSize(),
		"copyPreludeBuffer: size of prelude metadata cannot exceed prelude region size");

      memcpy(preludeRegion->regionStartAddress(), data, size);
      *_osCache.preludeSectionSizeFieldOffset() = size;
   }

   _osCache.releaseHeaderWriteLock();
}

// find space for, and stores, a code entry. if it fails at any point,
// simply return 0.
bool SOMCompositeCache::storeEntry(const char* methodName, void* data, U_32 allocSize)
{
   _osCache.acquireLock(DATA_SECTION_LOCK_ID);
   lockCache(_dataSectionReaderCount, DATA_SECTION_LOCK_ID);

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

   unlockCache(DATA_SECTION_LOCK_ID);   
   _osCache.releaseLock(DATA_SECTION_LOCK_ID);

   return true;
}

//TODO: should copy to the code cache (not scc) when code cache becomes available
void *SOMCompositeCache::loadEntry(const char *methodName)
{
   _osCache.acquireLock(DATA_SECTION_LOCK_ID);
   SOMCacheEntry *entry = _codeEntries[methodName];
   _osCache.releaseLock(DATA_SECTION_LOCK_ID);

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
