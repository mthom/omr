#include "SynchronizedCacheCounter.hpp"

bool SynchronizedCacheCounter::incrementCount(OSCacheImpl* osCache)
{  
  UDATA oldNum, value;

  if (!osCache->started() || osCache->runningReadOnly()) {
    Trc_SHR_Assert_ShouldNeverHappen();
    return false;
  }

  oldNum = *_regionFocus._counter; // _theca->readerCount;
  // Trc_SHR_CC_incReaderCount_Entry(oldNum);

  value = 0;
  
  // unprotectHeaderReadWriteArea(currentThread, false);
  _regionFocus._region->unprotect();
  
  do {
    value = oldNum + 1;
    oldNum = VM_AtomicSupport::lockCompareExchange(_regionFocus._counter, oldNum, value);
  } while ((UDATA)value != (oldNum + 1));
  
  //  protectHeaderReadWriteArea(currentThread, false);
  _regionFocus._region->protect();
  
  //  Trc_SHR_CC_incReaderCount_Exit(_theca->readerCount);
  return true;
}

bool SynchronizedCacheCounter::decrementCount()
{
  UDATA oldNum, value;

  if (!osCache->started() || osCache->runningReadOnly()) {
    Trc_SHR_Assert_ShouldNeverHappen();
    return false;
  }

  oldNum = *_regionFocus._counter; // _theca->readerCount;
  //Trc_SHR_CC_decReaderCount_Entry(oldNum);

  value = 0;
  // unprotectHeaderReadWriteArea(currentThread, false);
  _regionFocus._region->unprotect();
  
  do {
    if (0 == oldNum) {
      /* This can happen if _theca->readerCount is whacked to 0 by doLockCache() */
      // OMRPORT_ACCESS_FROM_PORT(_portlib);
      //CC_ERR_TRACE(J9NLS_SHRC_CC_NEGATIVE_READER_COUNT);
      break;
    }
    
    value = oldNum - 1;
    oldNum = VM_AtomicSupport::lockCompareExchange(_regionFocus._counter, oldNum, value);
  } while ((UDATA)value != (oldNum - 1));

  _regionFocus._region->protect(); //protectHeaderReadWriteArea(currentThread, false);

  //Trc_SHR_CC_decReaderCount_Exit(_theca->readerCount);

  return true;
}
