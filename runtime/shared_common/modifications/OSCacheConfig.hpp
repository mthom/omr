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

#if !defined(OSCACHE_CONFIG_HPP_INCLUDED)
#define OSCACHE_CONFIG_HPP_INCLUDED

#include "omr.h"
#include "sharedconsts.h"

#include "OSCacheAttachingContext.hpp"
#include "OSCacheCreatingContext.hpp"

// why is OSCacheLayout a template parameter?? Because Layout classes
// are typically "bottom level", meaning that they contain information
// regarding the architecture of the class that's accessed
// frequently. This is vastly preferable to peppering the cache code
// with dynamic_cast's wherever it needs to know about the layout
// IMHO.
template <class OSCacheLayout>
class OSCacheConfig
{
public:
  virtual IDATA getWriteLockID() = 0;
  virtual IDATA getReadWriteLockID() = 0;

  // sometimes the lock IDs are keyed against regions, sometimes not.
  virtual IDATA acquireLock(UDATA lockID, LastErrorInfo* lastErrorInfo = NULL) = 0;
  virtual IDATA releaseLock(UDATA lockID) = 0;  
protected:
  OSCacheLayout* _layout;
};

#endif
