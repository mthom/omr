#include "WASMOSCacheHeader.hpp"

template <class OSCacheHeader>
void WASMOSCacheHeader<OSCacheHeader>::init() {
  OSCacheHeader::init();

  void* remOffset = OSCacheHeader::regionStartAddress() + OSCacheHeader::regionSize();

  _readerCount = remOffset;
  _cacheCrc = remOffset + sizeof(volatile UDATA*);
}
