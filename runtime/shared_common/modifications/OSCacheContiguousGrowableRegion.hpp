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
#if !defined(OSCACHE_CONTIGUOUS_GROWABLE_REGION_HPP_INCLUDED)
#define OSCACHE_CONTIGUOUS_GROWABLE_REGION_HPP_INCLUDED

#include "OSCacheContiguousRegion.hpp"

/* A region that occupies a contiguous block of memory, that can be
 * grown, free space permitting.
 */
class OSCacheContiguousGrowableRegion: public OSCacheContiguousRegion
{
public:
  enum Direction { Forward, Backward };

  OSCacheContiguousGrowableRegion(OSCacheLayout* layout, int regionID, void* regionStart,
				  UDATA regionSize, bool pageBoundaryAligned);
  
protected:
  Direction _growthDirection;
};

#endif