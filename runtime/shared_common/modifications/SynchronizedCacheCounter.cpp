#include "SynchronizedCacheCounter.hpp"

bool SynchronizedCacheCounter::incrementCount(OSCacheImpl& osCache)
{  
  UDATA oldNum, value;

  if (!osCache.started() || osCache.runningReadOnly()) {
    Trc_SHR_Assert_ShouldNeverHappen();
    return false;
  }

  oldNum = *_regionFocus._field; // _theca->readerCount;
  // Trc_SHR_CC_incReaderCount_Entry(oldNum);

  value = 0;
  
  // unprotectHeaderReadWriteArea(currentThread, false);
  //_regionFocus._region->unprotect(); // this toggles memory protection off.
  osCache.setRegionPermissions(_regionFocus._region);
  
  do {
    value = oldNum + 1;
    oldNum = VM_AtomicSupport::lockCompareExchange(_regionFocus._field, oldNum, value);
  } while ((UDATA)value != (oldNum + 1));
  
  //  protectHeaderReadWriteArea(currentThread, false);
  // _regionFocus._region->protect(); // this toggles it back on.
  osCache.setRegionPermissions(_regionFocus._region);
  
  //  Trc_SHR_CC_incReaderCount_Exit(_theca->readerCount);
  return true;
}

bool SynchronizedCacheCounter::decrementCount(OSCacheImpl& osCache)
{
  UDATA oldNum, value;

  if (!osCache.started() || osCache.runningReadOnly()) {
    Trc_SHR_Assert_ShouldNeverHappen();
    return false;
  }

  oldNum = *_regionFocus._field; // _theca->readerCount;
  //Trc_SHR_CC_decReaderCount_Entry(oldNum);

  value = 0;
  // unprotectHeaderReadWriteArea(currentThread, false);
  // _regionFocus._region->unprotect();
  
  do {
    if (0 == oldNum) {
      /* This can happen if _theca->readerCount is whacked to 0 by doLockCache() */
      // OMRPORT_ACCESS_FROM_PORT(_portlib);
      //CC_ERR_TRACE(J9NLS_SHRC_CC_NEGATIVE_READER_COUNT);
      break;
    }
    
    value = oldNum - 1;
    oldNum = VM_AtomicSupport::lockCompareExchange(_regionFocus._field, oldNum, value);
  } while ((UDATA)value != (oldNum - 1));

  //_regionFocus._region->protect(); //protectHeaderReadWriteArea(currentThread, false);

  //Trc_SHR_CC_decReaderCount_Exit(_theca->readerCount);

  return true;
}
