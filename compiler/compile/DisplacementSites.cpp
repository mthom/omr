#include "compile/Compilation.hpp"
#include "compile/DisplacementSites.hpp"

TR_DisplacementSite::TR_DisplacementSite(TR::Compilation *comp, uint32_t assumptionID)
      : _assumptionID(assumptionID), _calleeIndex(0),
	_byteCodeIndex(0), _dispSize(TR_DisplacementSite::bits_32), _location(NULL)
   {
   comp->addDisplacementSite(this);
   }

void TR_DisplacementSite::setDisplacement(uintptrj_t disp) 
   {
   switch(_dispSize)
      {
      case DisplacementSize::bits_8:
	 *_location = disp;
	 break;
      case DisplacementSize::bits_16:
	 *(uint16_t*)_location = disp;
	 break;
      case DisplacementSize::bits_32:
	 *(uint32_t*)_location = disp;
	 break;
      }
   }
