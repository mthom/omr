#include "WASMOSCacheHeader.hpp"

template <class OSCacheHeader>
void WASMOSCacheHeader<OSCacheHeader>::init()
{
  OSCacheHeader::init();

  UDATA* remOffset = (UDATA*) regionStartAddress() + regionSize();

  _readerCount = remOffset;
  _cacheCrc = remOffset + sizeof(volatile UDATA*);

  *_readerCount = 0;
  *_cacheCrc = 0;
}

template <class OSCacheHeader>
UDATA WASMOSCacheHeader<OSCacheHeader>::regionSize()
{
  return OSCacheHeader::regionSize() + sizeof(volatile UDATA) + sizeof(UDATA);
}

