#include "compile/Compilation.hpp"
#include "compile/DisplacementSites.hpp"

TR_DisplacementSite::TR_DisplacementSite(TR::Compilation *comp, uint64_t assumptionID)
  : _sites(getTypedAllocator<uint8_t*, TRPersistentMemoryAllocator>(TRPersistentMemoryAllocator(comp->trPersistentMemory()))),
    _assumptionID(assumptionID), _nodeAddress(0),
    _dispSize(TR_DisplacementSite::bits_32)
   {
   comp->addDisplacementSite(this);
   }

void TR_DisplacementSite::addLocation(uint8_t* location)
   {
   _sites.push_front(location);
   }
