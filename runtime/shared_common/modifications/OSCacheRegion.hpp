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


#if !defined(OSCACHE_REGION_HPP_INCLUDED)
#define OSCACHE_REGION_HPP_INCLUDED

#include "omr.h"

class OSCacheLayout;

class OSCacheRegion {
public:
  // the start address of the region.
  virtual UDATA getRegionStartAddress() = 0;
  // calculates the size of the region.
  virtual UDATA getRegionSize() = 0;

  // render the Region's settings as flags that can be used
  // by the underlying ocmponents of the OMRPortLibrary, as anticipated
  // by the OSCacheRegion subclass.
  virtual IDATA renderToFlags() = 0;

  // check the cache region for corruption using whatever means are
  // available.  I'm not sure this method should belong to the
  // OSCacheRegion class.  OSCacheConfig may be a more appropriate choice.
  virtual IDATA checkValidity() = 0;

protected:  
  virtual void serialize(OSCacheRegionSerializer* serializer) = 0;
  
  // this all hinges on ENABLE_MPROTECT being set. The runtime flags. Of course,
  // we don't want that flag to be built in. Each region should have the choice of being
  // protected. Probably, these functions should exist downstream from OSCacheRegion.
  // We could introduce the class OSCachePageProtectedRegion, perhaps?

  // OK, the notion of page protection really doesn't apply at the OSCache
  // level. Only later, when you move up to the composite cache layer, where regions
  // are subdivided into further regions, does it make sense.

  // the commented-out arguments are now internal to region objects.
private:
  friend class OSClassLayout;
  // the regionStart is a *relative* value denoting the beginning of
  // the cache allocation from the start of the cache block in memory,
  // wherever it winds up. we allow for circumstances where the two
  // may not coincide.
};

#endif
