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

#if !defined(COMPOSITE_CACHE_HPP_INCLUDED)
#define COMPOSITE_CACHE_HPP_INCLUDED

#include "OSCacheImpl.hpp"

#define CC_STARTUP_OK 0
#define CC_STARTUP_FAILED -1
#define CC_STARTUP_CORRUPT -2
#define CC_STARTUP_RESET -3
#define CC_STARTUP_SOFT_RESET -4
#define CC_STARTUP_NO_CACHELETS -5
#define CC_STARTUP_NO_CACHE -6

/* How many bytes to sample from across the cache for calculating the CRC */
/* Need a value that has negligible impact on performance */
#define OMRSHR_CRC_MAX_SAMPLES 100000

class CompositeCache
{
public:
  CompositeCache(OSCacheImpl* oscache);

  virtual IDATA startup();
  
protected:
  OSCacheImpl* _oscache;
};

#endif

