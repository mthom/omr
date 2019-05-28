#ifndef TR_SYMBOLVALIDATIONMANAGER_INCL
#define TR_SYMBOLVALIDATIONMANAGER_INCL

#include "runtime/OMRSymbolValidationManager.hpp"

namespace TR
{

class OMR_EXTENSIBLE SymbolValidationManager : public OMR::SymbolValidationManagerConnector
   {
   public:
   SymbolValidationManager(TR::Region &region, TR_ResolvedMethod *compilee) : OMR::SymbolValidationManagerConnector(region, compilee) {}
   };
  
}

#endif
