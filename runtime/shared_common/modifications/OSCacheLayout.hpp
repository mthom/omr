/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#if !defined(OSCACHE_LAYOUT_HPP_INCLUDED)
#define OSCACHE_LAYOUT_HPP_INCLUDED

#include "omr.h"
#include "OSCache.hpp"
#include "OSCacheRegion.hpp"
#include "OSCacheRegionSerializer.hpp"

// for TR::vector.
// #include "infra/vector.hpp"
#include <vector>

class OSCacheLayout
{
public:
  OSCacheLayout(UDATA osPageSize)
    : _osPageSize(osPageSize)
  {}

  void serialize(OSCache* osCache) {
    OSCacheRegionSerializer* serializer = osCache->constructSerializer();

    for(int i = 0; i < _regions.size(); ++i) {
      _regions[i]->serialize(serializer);
    }
  }

  virtual void initialize(OSCache* osCache, void* blockAddress, uintptr_t size) {
    init(blockAddress, size);

    OSCacheRegionInitializer* initializer = osCache->constructInitializer();

    for(int i = 0; i < _regions.size(); ++i) {
      _regions[i]->initialize(initializer);
    }
  }
  
  virtual void alignRegionsToPageBoundaries();

  /* If a region changes size, the owning cache layout is notified. */
  virtual bool notifyRegionSizeAdjustment(OSCacheRegion&) = 0;

  virtual OSCacheRegion* operator[](uint64_t i) {
    return _regions[i];
  }
  
protected:
  // initialize the region.
  virtual void init(void* blockAddress, uintptr_t size) = 0;

  virtual void addRegion(OSCacheRegion* region) {
    _regions.push_back(region);
  }

  UDATA _osPageSize;
  //TODO: eventually convert to TR::vector.
  std::vector<OSCacheRegion*> _regions;  
};

#endif
