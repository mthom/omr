/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include <algorithm>
#include "infra/Monitor.hpp"
#include "infra/CriticalSection.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "runtime/DataCache.hpp"

//--------------------- DataCacheManager ----------------

// static field
TR::DataCacheManager* TR::DataCacheManager::_dataCacheManager = NULL;

template <typename T>
TR::DataCacheManager* TR::DataCacheManager::constructManager (
            JitConfig *jitConfig,
            TR::Monitor *monitor,
            uint32_t quantumSize,
            uint32_t minQuanta,
            bool newImplementation
   )
   {
  // PORT_ACCESS_FROM_JITCONFIG(jitConfig);
//   // T* newManager = static_cast<T *>(j9mem_allocate_memory( sizeof(T), J9MEM_CATEGORY_JIT ));
//    if (newManager)
//       {
//       newManager = new (newManager) T (
//          jitConfig,
//          monitor,
//          quantumSize,
//          minQuanta,
//          newImplementation
//          );
//       }
//    return newManager;
   }


//------------------------------- initializeDataCacheManager -----------------
// Allocate a single object of type TR::DataCacheManager
// Static method
// Return value:
//       Pointer to the initialized dataCacheManager or NULL in case of failure
// Note: this method should only be called by one thread during VM bootstrap
//       In case it returns NULL, we should fail the VM
//----------------------------------------------------------------------------
TR::DataCacheManager* TR::DataCacheManager::initialize(JitConfig * jitConfig)
   {

   if (!_dataCacheManager)
      {
      // allocate a dataCacheManager
      TR::Monitor *monitor = TR::Monitor::create("JIT-DataCacheManagerMutex");
      if (!monitor)
         return NULL;
      TR::DataCacheManager *(*managerConstructor)(JitConfig *, TR::Monitor *, uint32_t, uint32_t, bool);
      if (TR::Options::getCmdLineOptions()->getOption(TR_EnableDataCacheStatistics))
         {
         managerConstructor = constructManager<TR::InstrumentedDataCacheManager>;
         }
      else
         {
         managerConstructor = constructManager<TR::DataCacheManager>;
         }

   //Those parameters are flawed
      _dataCacheManager = managerConstructor(
         jitConfig,
         monitor,0,
         !TR::Options::getCmdLineOptions()->getOption(TR_OldDataCacheImplementation),
         false
      );
      }
   return _dataCacheManager;
   }

void
TR::DataCacheManager::destroyManager()
   {
   if (_dataCacheManager)
      {
      JitConfig *jitConfig = _dataCacheManager->_jitConfig; // make a copy before destroying the object
      _dataCacheManager = NULL;
      }
   }


TR::DataCacheManager::DataCacheManager(JitConfig *jitConfig, TR::Monitor *monitor, uint32_t quantumSize, uint32_t minQuanta, bool newImplementation, bool worstFit) :
   _activeDataCacheList(0),
   _almostFullDataCacheList(0),
   _cachesInPool(0),
   _numAllocatedCaches(0),
   _totalSegmentMemoryAllocated(0),
   _flags(0),
   _jitConfig(jitConfig),
   _quantumSize( roundToMultiple<uint32_t>(quantumSize, sizeof(uintptr_t)) ),
   _minQuanta(
      // We need the minimum chunk size (_quantumSize * _minQuanta) to be able to hold our Allocation objects
      // So we need to figure out how many quantas are needed to fit an Allocation object
      // And if the requested minimum is too, to set the run-time minimum to the required number.
      std::max(
         minQuanta,
         (
            roundToMultiple<uint32_t>(
               sizeof(Allocation),
               roundToMultiple<uint32_t>(
                  quantumSize,
                  sizeof(uintptr_t)
               )
            )
            /
            roundToMultiple<uint32_t>(
               quantumSize,
               sizeof(uintptr_t)
            )
         )
      )
   ),
   _newImplementation(newImplementation),
   _worstFit(worstFit),
   _sizeList(),
   _mutex(monitor)
   {
   TR_ASSERT( (_quantumSize % sizeof(UDATA)) == 0, "Chunks need to be aligned with pointer size");
   TR_ASSERT( (_quantumSize * _minQuanta) >= sizeof(Allocation), "Allocation won't fit in free blocks" );
   // Add trace point if we have disabled reclamation
#if defined(DATA_CACHE_DEBUG)
   if (!_newImplementation)
      fprintf(stderr, "Old data cache implementation enabled");
   fprintf(stderr, "Initialized the data cache manager with a quantum size of %d with a minimum of %d quanta per allocation.\n", _quantumSize, _minQuanta);
#endif
   }

TR::DataCacheManager::~DataCacheManager()
   {
   // This list will eventually be integrated directly into the data cache manager.
   freeDataCacheList(_activeDataCacheList);
   freeDataCacheList(_almostFullDataCacheList);

   for (auto i = _sizeList.begin(); i != _sizeList.end();)
      {
      SizeBucket &currentBucket = *i;
      i = _sizeList.remove(i);
      freeMemoryToVM(&currentBucket);
      }

   freeDataCacheList(_cachesInPool);

   }

void
TR::DataCacheManager::freeDataCacheList(TR::DataCache *& head)
   {
   for (TR::DataCache * next = 0; head; head = next)
      {
      next = head->_next;
      OMRMemorySegment *segment = head->_segment;
    //  head->~TR::DataCache();
      }
   }



//------------------------- copyDataCacheAllocation --------------------------
// Copy the contents from one data cache allocation to another
// Static method
// Parameters:
//       dest - Allocation receiving copy
//       src - Allocation used as source
// Return value:
//       void
//----------------------------------------------------------------------------

void TR::DataCacheManager::copyDataCacheAllocation(OMRJITDataCacheHeader *dest, OMRJITDataCacheHeader *src)
   {
   if (dest->size >= src->size)
      {
      dest->type = src->type;
      memmove(dest + 1, src + 1, src->size - sizeof(OMRJITDataCacheHeader));
      }
   }


//-------------------------- allocateNewDataCache ----------------------------
// If allowed, allocate a new dataCache segment and register it with the VM
// Should not be called directly by code outside TR::DataCache class
// Parameters:
//      minimumSize - minimum size to allocate
// Return value:
//      Pointer to the allocated dataCache  or NULL if allocation failed
//      The TR::DataCache structure is filled-in with the exception of the owning
//      vmThread
// Side effects:
//      Will acquire/release the dataCacheManager mutex internally
//      Does not need vmAccess
//----------------------------------------------------------------------------
TR::DataCache* TR::DataCacheManager::allocateNewDataCache(uint32_t minimumSize)
   {
   TR::DataCache *dataCache = NULL;
   //--- try to allocate a new one ---
   // if ( _numAllocatedCaches==0) // make sure we are allowed to allocate
//       {
//       if (_jitConfig->dataCacheList->totalSegmentSize < _jitConfig->dataCacheTotalKB * 1024)
//          {
//          dataCache = (TR::DataCache*)j9mem_allocate_memory(sizeof(TR::DataCache), J9MEM_CATEGORY_JIT);
//          if (dataCache)
//             {
//             // Compute the size for the segment
//             int32_t segSize = std::max(_jitConfig->dataCacheKB * 1024, static_cast<UDATA>(minimumSize));

//             //--- allocate the segment for the dataCache
//             MemorySegment *dataCacheSeg = NULL;
//                {
//                OMR::CriticalSection criticalSection(_mutex);
//                dataCacheSeg = _jitConfig->javaVM->internalVMFunctions->allocateMemorySegmentInList(_jitConfig->javaVM, _jitConfig->dataCacheList, segSize, MEMORY_TYPE_RAM, J9MEM_CATEGORY_JIT_DATA_CACHE);
//                if (dataCacheSeg)
//                   _jitConfig->dataCache = dataCacheSeg; // for maximum compatibility with the old implementation
//                }

//             if (dataCacheSeg)
//                {
//                int32_t allocatedSize = (char*)dataCacheSeg->heapTop - (char*)dataCacheSeg->heapBase;
//                //memset(dataCacheSeg->heapBase, 0, allocatedSize); // 0 initialize the segment
//                dataCache->_segment = dataCacheSeg;
//                dataCache->_next = NULL;
//                dataCache->_status = 0;
//                dataCache->_vmThread = NULL;
//                dataCache->_allocationMark = dataCacheSeg->heapAlloc;
//                _numAllocatedCaches++;
//                _totalSegmentMemoryAllocated += (uint32_t)allocatedSize;
// #ifdef DATA_CACHE_DEBUG
//                fprintf(stderr, "Allocated a new segment %p of size %d for TR::DataCache %p heapAlloc=%p\n",
//                   dataCacheSeg, segSize, dataCache, dataCacheSeg->heapAlloc);
// #endif
//                }
//             else
//                {
//                TR_VerboseLog::write("<JIT: non-fatal error: failed to allocate %d Kb data cache>\n", _jitConfig->dataCacheKB);
//                j9mem_free_memory(dataCache);
//                dataCache = NULL;
//                _jitConfig->runtimeFlags |= J9JIT_DATA_CACHE_FULL;  // prevent future allocations
// #ifdef DATA_CACHE_DEBUG
//                fprintf(stderr, "Segment allocation for dataCache failed\n");
// #endif
//                }
//             }
//          else // cannot allocate even a bit of memory from the JVM
//             {
//             TR_VerboseLog::write("<JIT: non-fatal error: failed to allocate %d bytes for data cache>\n", sizeof(TR::DataCache));
//             _jitConfig->runtimeFlags |= J9JIT_DATA_CACHE_FULL;  // prevent future allocations
// #ifdef DATA_CACHE_DEBUG
//             fprintf(stderr, "Memory allocation for TR::DataCache failed\n");
// #endif
//             }
//          }
//       else // reached the limit on data cache segments
//          {
//          _jitConfig->runtimeFlags |= J9JIT_DATA_CACHE_FULL;  // prevent future allocations
//          }
//       }
   return dataCache;
   }


//-------------------------- reserveAvailableDataCache -----------------------
// Try to find a dataCache that can be used. Allocate a new segment if needed
// Parameters:
//       vmThread - calling thread
//       sizeHint - hint about the amount of memory we are going to use
// Return value:
//       Pointer to TR::DataCache that can be used exclusively by this thread
//       or NULL if cannot get any dataCache
// Side efects:
//       Aquires/releases dataCache mutex internally
//----------------------------------------------------------------------------
TR::DataCache* TR::DataCacheManager::reserveAvailableDataCache( uint32_t sizeHint)
   {
   sizeHint = TR::DataCacheManager::alignToMachineWord(sizeHint);
      //TODO define
      TR::DataCache *dataCache = NULL;
      {
      OMR::CriticalSection criticalSection(_mutex);

      //--- go through _activeDataCacheList and find one that has at least sizeHint bytes available
      TR::DataCache *prev = NULL;
      dataCache = _activeDataCacheList;
      while (dataCache)
         {
         if ((uint32_t)(dataCache->_segment->heapTop - dataCache->_segment->heapAlloc) >= sizeHint)
            {
            // Detach this data cache from the active list
            if (!prev)
               _activeDataCacheList = dataCache->_next;
            else
               prev->_next = dataCache->_next;
            dataCache->_next = NULL;
            break;
            }
         else
            {
            prev = dataCache;
            dataCache = dataCache->_next;
            }
         }
      }

   if (!dataCache)
      {
      // no dataCache available in the list; try to allocate a new one
      dataCache = allocateNewDataCache(sizeHint);
      if (dataCache)
         dataCache->_status = TR::DataCache::ACTIVE; // make it look as if it came from the active list
      }
   if (dataCache)
      {
      // TR_ASSERT(dataCache->_vmThread == NULL, "data cache must not be in use by other thread");
      // TR_ASSERT(dataCache->_status == TR::DataCache::ACTIVE, "data cache must be active");
      dataCache->_status = TR::DataCache::RESERVED;
      }
#if defined(DATA_CACHE_DEBUG) && (DATA_CACHE_VERBOSITY_LEVEL >= 3)
      fprintf(stderr, "reserveAvailableDataCache: sizeHint=%u reserved dataCache=%p\n",
              sizeHint, dataCache);
#endif
   return dataCache;
   }


//---------------------------- makeDataCacheAvailable ------------------------
// Put the designated dataCache back into the list of available data caches
// Side efects:
//       Aquires/releases dataCache mutex internally
//----------------------------------------------------------------------------
void TR::DataCacheManager::makeDataCacheAvailable(TR::DataCache *dataCache)
   {
   OMR::CriticalSection criticalSection(_mutex);
   dataCache->_status = TR::DataCache::ACTIVE;
   dataCache->_next = _activeDataCacheList; // put the cache in the list
   _activeDataCacheList = dataCache;
   // TODO: make sure the available list does not grow too much
   // If too many caches almost full, we may need to retire some of them
#if defined(DATA_CACHE_DEBUG) && (DATA_CACHE_VERBOSITY_LEVEL >= 3)
   fprintf(stderr, "makeDataCacheAvailable: dataCache=%p heapAlloc=%p\n", dataCache, dataCache->_segment->heapAlloc);
#endif
   }


//---------------------------- retireDataCache -------------------------------
// This cache is almost full. Place it in the list of almost full caches
//----------------------------------------------------------------------------
void TR::DataCacheManager::retireDataCache(TR::DataCache *dataCache)
{
   OMR::CriticalSection criticalSection(_mutex);
#if defined(DATA_CACHE_DEBUG) && (DATA_CACHE_VERBOSITY_LEVEL >=1)
   J9VMThread *dataCacheThread = dataCache->_vmThread;
#endif
   dataCache->_status = TR::DataCache::ALMOST_FULL;
   dataCache->_next = _almostFullDataCacheList; // put the cache in the list
   _almostFullDataCacheList = dataCache;
#if defined(DATA_CACHE_DEBUG) && (DATA_CACHE_VERBOSITY_LEVEL >=1)
   fprintf(stderr, "retireDataCache %p for thread %p\n", dataCache, dataCacheThread);
#endif
}


//----------------------------- allocateDataCacheSpace -----------------------
// Allocates some space in the current dataCache.
// This method can be called only after a thread has reserved this dataCache for
// exclusive use.
// Parameters:
//      size - how much space to allocate in the current dataCache
//      (NOTE: internally the method may allocate more than 'size' to allow for
//              natural alignment)
// Return value:
//      Pointer to the allocated space or NULL if the segment does not contain
//      enough available space
//----------------------------------------------------------------------------
uint8_t* TR::DataCache::allocateDataCacheSpace(int32_t size)
   {
   uint8_t *base = 0;
   size = TR::DataCacheManager::alignToMachineWord(size);
   // dataCache is manipulated exclussively by a single thread
   if (_segment->heapAlloc + size <= _segment->heapTop)
      {
      base = _segment->heapAlloc;
      _segment->heapAlloc += size; // buy the space
#if defined(DATA_CACHE_DEBUG) && (DATA_CACHE_VERBOSITY_LEVEL >= 3)
      fprintf(stderr, "allocateDataCacheSpace: size=%d dataCache=%p after allocation heapAlloc=%p base=%p\n",
              size, this, _segment->heapAlloc, base);
#endif
      }
   return base;
   }



//----------------------------- allocateDataCacheSpace -----------------------
// Allocates some space in one of the available data caches.
// This method DOES NOT allocates a "record", nor does it fill the fields of
// OMRJITDataCacheHeader. It simply allocates space in a dataCache.
// For more functionality look at allocateDataCacheRecord() which internally
// calls this routine.
// This method should be used when successive allocations don't need to
// contiguous (this is not the case for AOT compilations)
// For more flexibility one can use the sequence:
//    TR::DataCache* dataCache = reserveAvailableDataCache()
//    dataCache->allocateDataCacheSpace(size);
//    makeDataCacheAvailable(dataCache);
//
// Parameters:
//      size - desired size
//      (NOTE: internally  the method may allocate more than 'size' to allow for
//             natural alignment)
// Ret value:
//      pointer to allocated space
// Side efects:
//       Aquires/releases dataCache mutex internally
//-----------------------------------------------------------------------------
uint8_t* TR::DataCacheManager::allocateDataCacheSpace(uint32_t size)
{
   OMR::CriticalSection criticalSection(_mutex);
   uint8_t *retValue = NULL;
   size = TR::DataCacheManager::alignToMachineWord(size);

   //--- go through _activeDataCacheList and find one that has at least sizeHint bytes available
   TR::DataCache *dataCache = _activeDataCacheList;
   while (dataCache)
      {
      int32_t bytesAvailable = dataCache->_segment->heapTop - dataCache->_segment->heapAlloc;
      if ((uint32_t)bytesAvailable >= size)
         break;
      // This dataCache is too small; retire it
      _activeDataCacheList = dataCache->_next;
      retireDataCache(dataCache);
      dataCache = _activeDataCacheList;
      }
    if (!dataCache)
      {
      // no dataCache available in the list; try to allocate a new one
      if ((dataCache = allocateNewDataCache(size)))
         {
         dataCache->_status = TR::DataCache::ACTIVE; // make it look as if it came from the active list
         // Insert new cache in the list
         dataCache->_next = _activeDataCacheList;
         _activeDataCacheList = dataCache;
         }
      }
   if (dataCache)
      {
      TR_ASSERT(dataCache->_status == TR::DataCache::ACTIVE, "data cache must be active");
      retValue = dataCache->allocateDataCacheSpace(size);
      }
   return retValue;
}


//--------------------------- fillDataCacheHeader ---------------------------
// Helper routine that fills the OMRJITDataCacheHeader of an allocation record
// Normally, this should not be called directly. It is used in
// allocateDataCacheRecord routines from TR::DataCacheManager and J9VMBase
// Parameters:
//    hdr - pointer to OMRJITDataCacheHeader that needs to be initialized
//    allocationType - type of allocation record (e.g. J9_JIT_DCE_EXCEPTION_INFO)
//    size - total number of bytes allocated for this record
// Return value: none
// Side effects: some fields from jitConfig will be updated
//----------------------------------------------------------------------------
void TR::DataCacheManager::fillDataCacheHeader(OMRJITDataCacheHeader *hdr, uint32_t allocationType, uint32_t size)
{
   // Complete the header
   hdr->size = size;
   hdr->type = allocationType;
}


//------------------------------- allocateDataCacheRecord --------------------
// Allocate some space (including header) into the data cache and fill in the
// header of the record
// The data cache does not have to be reserved
// size may be rounded for alignment
// There is a similar methos in J9VMBase. That one is supposed to be called
// from within a compilation. This method is supposed to be called outside a
// compilation and when we don't need contiguous allocation.
//----------------------------------------------------------------------------
uint8_t* TR::DataCacheManager::allocateDataCacheRecord(uint32_t numBytes, uint32_t allocationType,
                                                      uint32_t *allocatedSizePtr)
   {
   uint8_t* retValue = NULL;
   // need to allocate space for header too and do some alignment
   uint32_t size = numBytes + sizeof(OMRJITDataCacheHeader);

   if (_newImplementation)
      {
      OMR::CriticalSection criticalSection(_mutex);
      size = alignAllocation(size);
      TR_ASSERT(size == TR::DataCacheManager::alignToMachineWord(size), "Byte multiples should always align to machine words");
      Allocation *alloc = getFromPool(size);
      if (!alloc)
         {
         TR::DataCache *newDataCache = allocateNewDataCache(size);
         if (newDataCache)
            {
            alloc = convertDataCacheToAllocation(newDataCache);
            }
         }
      if (alloc)
         {
         if (alloc->size() >= size + (_quantumSize * _minQuanta) )
            {
            Allocation *remainder = alloc->split(size);
#if defined(DATA_CACHE_DEBUG) && (DATA_CACHE_VERBOSITY_LEVEL >= 3)
            fprintf(stderr, "Allocation split, returning unused allocation to the pool\n", remainder, remainder->size());
#endif
            addToPool(remainder);
            }
         allocationHook(alloc->size(), numBytes);
         alloc->prepareForUse();
         fillDataCacheHeader(static_cast<OMRJITDataCacheHeader *>(static_cast<void *>(alloc)), allocationType, alloc->size());
         retValue = static_cast<uint8_t *>(alloc->getBuffer());
         if (allocatedSizePtr)
            {
            *allocatedSizePtr = alloc->size() - sizeof(OMRJITDataCacheHeader);
            }
         TR_ASSERT(retValue == static_cast<uint8_t *>( static_cast<void *>(alloc) ) + sizeof(OMRJITDataCacheHeader),"Pointer to the buffer should point to address immediately following the data cache header.");
         }
#if defined(DATA_CACHE_DEBUG)
         printStatistics();
#endif
      }
   else
      {
      size = TR::DataCacheManager::alignToMachineWord(size);
      uint8_t  *ptr = allocateDataCacheSpace(size);
      if (ptr) // I managed to allocate some space
         {
         // Complete the header
         fillDataCacheHeader((OMRJITDataCacheHeader*)ptr, allocationType, size);
         if (allocatedSizePtr)
            *allocatedSizePtr = size - sizeof(OMRJITDataCacheHeader); // communicate back the allocated size
         // Return the location after the header
         retValue = (ptr + sizeof(OMRJITDataCacheHeader));
         }
      }
#if defined(DATA_CACHE_DEBUG)
   fprintf(stderr, "allocateDataCacheRecord: startAddress=%p numBytes=%u allocationType=%u size=%u retValue=%p\n",
         retValue - sizeof(OMRJITDataCacheHeader), numBytes, allocationType, size, retValue);
#endif
   return retValue;
   }



//----------------------------- freeDataCacheRecord -----------------------------
// This should be called when a data cache allocation is no longer required and
// should be returned to the pool.  NOTE: This should be called with the same
// pointer offset as was given in allocateDataCacheRecord (IE just after the data
// cache header)
// Parameters:
//    record - pointer to Data cache allocation that needs to be reclaimed
// Return value: none
// Side effects: If the record is actually still in use, bad things will happen
//-------------------------------------------------------------------------------
void TR::DataCacheManager::freeDataCacheRecord(void *record)
   {
   if (_newImplementation)
      {
      OMRJITDataCacheHeader *hdr = static_cast<OMRJITDataCacheHeader *>( static_cast<void *>( static_cast<uint8_t *>(record) - sizeof (OMRJITDataCacheHeader) ) );
      uint32_t size = hdr->size;
      Allocation *alloc = new (hdr) Allocation (size);
      if ( TR::Options::getCmdLineOptions()->getOption(TR_PaintDataCacheOnFree) )
         {
         uint8_t *start = static_cast<uint8_t *>( static_cast<void *>(alloc) ) + sizeof(Allocation);
         for (size_t i = 0; i < alloc->size() - sizeof(Allocation); ++i)
            switch (i % 4)
               {
               case 0:
                  start[i] = 0xDA;
                  break;
               case 1:
                  start[i] = 0x7A;
                  break;
               case 2:
                  start[i] = 0xCA;
                  break;
               case 3:
                  start[i] = 0xCE;
                  break;
               }
         }

      if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableDataCacheReclamation))
         {
         OMR::CriticalSection criticalSection(_mutex);
#if defined(DATA_CACHE_DEBUG)
         fprintf(stderr, "Reaping data cache record at %p, data start = %p\n", static_cast<uint8_t *>(record) - sizeof(OMRJITDataCacheHeader), record);
         fprintf(stderr, "Returning freed allocation to pool\n");
#endif
         addToPool(alloc);
         freeHook(alloc->size());
#if defined(DATA_CACHE_DEBUG)
         printStatistics();
#endif
         }
      }
   }


//----------------------------- computeDataCacheEfficiency -------------------
// Returns a percentage which represents the occupancy of the data caches in
// the system. If data cache reclamation is implemented, then we need to
// revisit this method
//----------------------------------------------------------------------------
double TR::DataCacheManager::computeDataCacheEfficiency()
   {
   OMR::CriticalSection criticalSection(_mutex);
   uint32_t availableSpaceActive = 0;
   uint32_t availableSpaceRetired = 0;
   int32_t numSeenCaches = 0;
   // Traverses all known dataCaches
   TR::DataCache *dataCache;
   for(dataCache = _activeDataCacheList; dataCache; dataCache = dataCache->_next)
      {
      availableSpaceActive += dataCache->_segment->heapTop - dataCache->_segment->heapAlloc;
      numSeenCaches++;
      }
   for(dataCache = _almostFullDataCacheList; dataCache; dataCache = dataCache->_next)
      {
      availableSpaceRetired += dataCache->_segment->heapTop - dataCache->_segment->heapAlloc;
      numSeenCaches++;
      }
   if (numSeenCaches != _numAllocatedCaches)
      fprintf(stderr, "Possible leak: numSeenCaches=%d numAllocatedCaches=%d\n", numSeenCaches, _numAllocatedCaches);
   return 100.0 * (_totalSegmentMemoryAllocated - availableSpaceActive - availableSpaceRetired) / _totalSegmentMemoryAllocated;
   }



void *
TR::DataCacheManager::allocateMemoryFromVM(size_t size)
   {
   // void *alloc = j9mem_allocate_memory(size, J9MEM_CATEGORY_JIT);
   // if (!alloc)
   //    {
   //    // out of memory
   //    }
   // return alloc;
   }

void
TR::DataCacheManager::freeMemoryToVM(void *ptr)
   {
   // PORT_ACCESS_FROM_JITCONFIG(_jitConfig);
   // j9mem_free_memory(ptr);
   }


void
TR::DataCacheManager::addToPool(TR::DataCacheManager::Allocation *alloc)
   {
   TR_ASSERT(alloc, "Attempting to add a NULL allocation to pool.");
#if defined(DATA_CACHE_DEBUG) && (DATA_CACHE_VERBOSITY_LEVEL >= 3)
   fprintf(stderr, "Adding allocation to pool.  Start = %p, Size = %u\n", alloc, alloc->size());
#endif
   InPlaceList<SizeBucket>::Iterator it = _sizeList.begin();
   while (it != _sizeList.end() && it->size() < alloc->size() )
      {
      ++it;
      }
   if ( it != _sizeList.end() && it->size() == alloc->size() )
      {
      it->push(alloc);
      insertHook(alloc->size());
      }
   else
      {
      void *vmAlloc = allocateMemoryFromVM( sizeof(SizeBucket) );
      if (vmAlloc)
         {
         SizeBucket *sb = new ( vmAlloc ) SizeBucket(alloc);
         _sizeList.insert(it, *sb);
         insertHook(alloc->size());
         }
      else
         {
         // Add trace point for leaked allocation
         }
      }
   }


TR::DataCacheManager::Allocation *
TR::DataCacheManager::getFromPool(uint32_t size)
   {
#if defined(DATA_CACHE_DEBUG) && (DATA_CACHE_VERBOSITY_LEVEL >= 3)
   fprintf(stderr, "Attempting to retrieve an allocation of size %u\n", size);
#endif
   Allocation *ret = 0;
   InPlaceList<SizeBucket>::Iterator it = _sizeList.begin();
   while (it != _sizeList.end() && it->size() < size)
      {
      ++it;
      }
   if (it != _sizeList.end())
      {
      if (_worstFit)
         {
         if (it->size() != size)
            {
            it = _sizeList.end();
            --it;
            }
         }
      ret = it->pop();
      if (it->isEmpty())
         {
            SizeBucket *sb = &(*it);
            _sizeList.remove(it);
            freeMemoryToVM(sb);
         }
      }
   if (ret)
      removeHook(ret->size());
#if defined(DATA_CACHE_DEBUG) && (DATA_CACHE_VERBOSITY_LEVEL >= 3)
   if (ret)
      {
      fprintf(stderr, "Retrieving allocation from pool.  Start = %p, Size = %u\n", ret, ret->size());
      }
   else
      {
      fprintf(stderr, "Unable to retrieve an allocation >= %u\n", size);
      }
#endif
   return ret;
   }

void
TR::DataCacheManager::convertDataCachesToAllocations()
   {
   if (_newImplementation)
      {
      OMR::CriticalSection criticalSection(_mutex);
      TR::DataCache *currentDataCache = _activeDataCacheList;
      while (currentDataCache != NULL)
         {
         TR::DataCache *nextDataCache = currentDataCache->_next;
         Allocation *allocation = convertDataCacheToAllocation(currentDataCache);
         if (allocation)
            {
            addToPool(allocation);
            }
         currentDataCache = nextDataCache;
         }
      _activeDataCacheList = NULL;
      }
   }

TR::DataCacheManager::Allocation*
TR::DataCacheManager::convertDataCacheToAllocation(TR::DataCache *dataCache)
   {
   Allocation *returnValue = NULL;
   TR_ASSERT(dataCache, "Attempting to convert a NULL data cache.");
   uint32_t dataCacheSize = dataCache->remainingSpace();
   if (dataCacheSize >= (_quantumSize * _minQuanta))
      {
      returnValue =  new (dataCache->allocateDataCacheSpace(dataCacheSize)) Allocation(dataCacheSize);
      dataCache->_next = _cachesInPool;
      _cachesInPool = dataCache;
      growHook(returnValue->size());
      }
   else
      {
      retireDataCache(dataCache);
      }
   return returnValue;
   }


TR::DataCacheManager::Allocation *
TR::DataCacheManager::Allocation::split(uint32_t requiredSize)
   {
   TR_ASSERT(this->_header.size >= requiredSize + sizeof(Allocation), "Should only split allocations when actual size is bigger than required size plus enough space to fit another allocation.");
   Allocation *ret = new ((uint8_t *)this + requiredSize) Allocation( _header.size - requiredSize);
   this->_header.size = requiredSize;
   return ret;
   }


TR::DataCacheManager::SizeBucket::~SizeBucket()
   {
   // Need to implement this if we want to blow away the memory pool all at once.
   // Here we would make sure that each allocation in the size bucket gets deleted.
   // Alternatively, remove allocations one by one and delete them.
   }


void TR::DataCacheManager::SizeBucket::push(Allocation *alloc)
   {
      _allocations.push_front(*alloc);
   }


TR::DataCacheManager::Allocation *TR::DataCacheManager::SizeBucket::pop()
   {
   InPlaceList<Allocation>::Iterator it = _allocations.begin();
   TR_ASSERT(it != _allocations.end(), "There should always be an allocation to remove");
   Allocation &ret = *it;
   it = _allocations.remove(it);
   return &ret;
   }


void
TR::DataCacheManager::growHook( UDATA allocationSize )
   {
   }

void
TR::DataCacheManager::allocationHook( UDATA allocationSize, UDATA requestedSize )
   {
   }

void
TR::DataCacheManager::freeHook( UDATA allocationSize )
   {
   }

void TR::DataCacheManager::insertHook( UDATA allocationSize )
   {
   }

void TR::DataCacheManager::removeHook( UDATA allocationSize )
   {
   }

void
TR::DataCacheManager::printStatistics()
   {
   }

void
TR::DataCacheManager::SizeBucket::print()
   {
   fprintf(stderr, "\tSizeBucket of size %u with Allocations:\n", _size);
   for (InPlaceList<Allocation>::Iterator it = _allocations.begin(); it != _allocations.end(); ++it)
      {
      it->print();
      }
   }

UDATA
TR::DataCacheManager::SizeBucket::calculateBucketSize()
   {
   UDATA bucketSize = 0;
   for (InPlaceList<Allocation>::Iterator it = _allocations.begin(); it != _allocations.end(); ++it)
      {
      bucketSize += it->size();
      }
   return bucketSize;
   }

void
TR::DataCacheManager::Allocation::print()
   {
   fprintf(stderr, "\t\tAllocation at %p of size %u\n", this, _header.size);
   }

TR::InstrumentedDataCacheManager::InstrumentedDataCacheManager(JitConfig *jitConfig, TR::Monitor *monitor, uint32_t quantumSize, uint32_t minQuanta, bool newImplementation, bool worstFit) :
   TR::DataCacheManager(jitConfig, monitor, quantumSize, minQuanta, newImplementation, worstFit),
   _jitSpace(0),
   _freeSpace(0),
   _usedSpace(0),
   _totalWaste(0),
   _numberOfAllocations(0),
   _numberOfCurrentAllocations(0),
   _bytesAllocated(0),
   _maxConcurrentWasteEstimate(0),
   _squares(0),
   _bytesInPool(0)

   {
   }


void
TR::InstrumentedDataCacheManager::growHook(UDATA bytesAdded)
   {
   _jitSpace += bytesAdded;
   _freeSpace += bytesAdded;
   }

void
TR::InstrumentedDataCacheManager::allocationHook(UDATA allocationSize, UDATA requestedSize)
   {
   _usedSpace += allocationSize;
   _freeSpace -= allocationSize;
   ++_numberOfAllocations;
   _totalWaste += allocationSize - ( sizeof(OMRJITDataCacheHeader) + requestedSize );
   ++_numberOfCurrentAllocations;
   _bytesAllocated += allocationSize;
   double averageWaste = static_cast<double>(_totalWaste) / static_cast<double>(_numberOfAllocations);
   double currentWaste = averageWaste * _numberOfCurrentAllocations;
   _maxConcurrentWasteEstimate = std::max( _maxConcurrentWasteEstimate, currentWaste);
   _squares += ( static_cast<double>(allocationSize) * static_cast<double>(allocationSize) );

   }

void
TR::InstrumentedDataCacheManager::freeHook(UDATA allocationSize)
   {
   _freeSpace += allocationSize;
   _usedSpace -= allocationSize;
   --_numberOfCurrentAllocations;
   }

void
TR::InstrumentedDataCacheManager::insertHook(UDATA allocationSize)
   {
   _bytesInPool += allocationSize;
   }

void
TR::InstrumentedDataCacheManager::removeHook(UDATA allocationSize)
   {
   _bytesInPool -= allocationSize;
   }

void
TR::InstrumentedDataCacheManager::printStatistics()
   {
   OMR::CriticalSection criticalSection(_mutex);
   convertDataCachesToAllocations();
   double averageWaste = static_cast<double>(_totalWaste) / static_cast<double>(_numberOfAllocations);
   double currentWaste = averageWaste * _numberOfCurrentAllocations;
   _maxConcurrentWasteEstimate = std::max( _maxConcurrentWasteEstimate, currentWaste);
   fprintf(stderr, "=== Data cache statistics ===\n");
   fprintf(stderr, "Total data cache bytes in use = %zu\n", _totalSegmentMemoryAllocated);
   fprintf(stderr, "Bytes converted for regluar JIT use = %zu\n", _jitSpace);
   fprintf(stderr, "Average allocation size = %f\n", static_cast<double>(_bytesAllocated) / static_cast<double>(_numberOfAllocations) );
   fprintf(stderr, "Standard Deviation of allocation size = %f\n", sqrt(_squares) );
   fprintf(stderr, "Average waste per allocation = %f\n", averageWaste );
   fprintf(stderr, "Estimated current waste = %f\n", currentWaste);
   fprintf(stderr, "Estimated maximum waste = %f\n", _maxConcurrentWasteEstimate);
   fprintf(stderr, "Loss = %zu\n",  _freeSpace - _bytesInPool);
   fprintf(stderr, "Loss Error = %zu\n", _bytesInPool - calculatePoolSize());
   fprintf(stderr, "Free Space = %zu\n",  _freeSpace);
   fprintf(stderr, "Bytes in pool = %zu\n", _bytesInPool);
   printPoolContents();
   fflush(stderr);
   }

void
TR::InstrumentedDataCacheManager::printPoolContents()
   {
   fprintf(stderr, "Printing pool contents:\n");
   for (InPlaceList<SizeBucket>::Iterator it = _sizeList.begin(); it != _sizeList.end(); ++it)
      {
      it->print();
      }
   }

UDATA
TR::InstrumentedDataCacheManager::calculatePoolSize()
   {
   UDATA poolSize = 0;
   for (InPlaceList<SizeBucket>::Iterator it = _sizeList.begin(); it != _sizeList.end(); ++it)
      {
      poolSize += it->calculateBucketSize();
      }
   return poolSize;
   }

