/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef OMR_DISPLACEMENT_SITES_INCL
#define OMR_DISPLACEMENT_SITES_INCL

#include <stddef.h>
#include <stdint.h>
// #include "env/KnownObjectTable.hpp"
#include "env/TRMemory.hpp"
#include "infra/TRlist.hpp"
// #include "env/jittypes.h"
// #include "infra/Assert.hpp"
// #include "infra/List.hpp"

namespace TR { class Compilation; }

class TR_DisplacementSite
   {
   public:

   TR_ALLOC(TR_Memory::DisplacementSiteInfo)

   enum DisplacementSize { bits_8, bits_16, bits_32 };

   TR_DisplacementSite(TR::Compilation *comp, uint64_t assumptionID);
   TR_DisplacementSite(uint64_t assumptionID);

   DisplacementSize setDisplacementSize(DisplacementSize size) { return (_dispSize = size); }

   uint64_t getAssumptionID() { return _assumptionID; }
   void addLocation(uint8_t* location);
     
   void setNodeAddress(uintptrj_t nodeAddress) { _nodeAddress = nodeAddress; }
   uintptrj_t getNodeAddress() { return _nodeAddress; }

   TR::list<uint8_t*, TRPersistentMemoryAllocator>& getSites() { return _sites; }
     
   private:
   TR::list<uint8_t*, TRPersistentMemoryAllocator>     _sites;     
   uintptrj_t                                          _nodeAddress;
   uint64_t                                            _assumptionID;
   DisplacementSize                                    _dispSize;
   };

#endif
