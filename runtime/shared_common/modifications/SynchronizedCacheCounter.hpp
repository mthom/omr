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
#if !defined(SYNCHRONIZED_CACHE_COUNTER_HPP_INCLUDED)
#define SYNCHRONIZED_CACHE_COUNTER_HPP_INCLUDED

#include "OSCacheImpl.hpp"
#include "OSCacheRegion.hpp"

#include "omr.h"
#include "shrnls.h"

// contains a 'focus', a pointer to a OSCacheRegion object, and a pointer to a
// volatile UDATA field within the object. The constructor asserts that the
// field is contained fully inside the memory spanned by the object.
template <typename T>
struct OSCacheRegionFocus {
  OSCacheRegionFocus(OSCacheRegion* region, T* counter)
    : _region(region)
    , _counter(counter)
  {
    Trc_SHR_Assert_True(region != NULL
			&& region + region->regionSize() >= counter + sizeof(T)
			&& region <= counter);
  }
  
  OSCacheRegion* _region;
  T* _counter;
};

class SynchronizedCacheCounter
{
public:
  SynchronizedCacheCounter(OSCacheRegion* region, UDATA* counter)
    : _regionFocus(region, counter)
  {}

  virtual bool incrementCount(OSCacheImpl* osCache);
  virtual bool decrementCount(OSCacheImpl* osCache);
  
protected:
  OSCacheRegionFocus<UDATA> _regionFocus;
};

#endif
