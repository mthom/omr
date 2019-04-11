#include "OSCacheConfig.hpp"
#include "OSSharedMemoryCacheConfig.hpp"

void
OSSharedMemoryCacheHeader::init(OMRPortLibrary* library, bool inDefaultControlDir)
{
  OMRPORT_ACCESS_FROM_OMRPORT(library);

  void* headerStart = regionStartAddress();
  UDATA headerSize  = regionSize();
  
  memset(headerStart, 0, headerSize);

  alignToExistingHeader(headerStart);
  
  strncpy(_mapping->_eyecatcher, OMRSH_OSCACHE_SYSV_EYECATCHER, OMRSH_OSCACHE_SYSV_EYECATCHER_LENGTH);

  _mapping->_createTime = omrtime_current_time_millis();
  _mapping->_inDefaultControlDir = inDefaultControlDir;
}

void OSSharedMemoryCacheHeader::alignToExistingHeader(void* headerStart)
{
  _mapping = (OSSharedMemoryCacheHeaderMapping *) headerStart;
}
