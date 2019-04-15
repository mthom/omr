
#include "OSMemoryMappedCache.hpp"
#include "OSSharedMemoryCache.hpp"

#include "WASMOSCache.hpp"

template <> class WASMOSCache<OSMemoryMappedCache>;
template <> class WASMOSCache<OSSharedMemoryCache>;
