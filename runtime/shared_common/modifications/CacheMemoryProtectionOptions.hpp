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
#if !defined(CACHE_MEMORY_PROTECTION_HPP_INCLUDED)
#define CACHE_MEMORY_PROTECTION_HPP_INCLUDED

#include "omrport.h"

class CacheMemoryProtectionOptions
{
public:
  enum ProtectionOptions {
    Read = 0x1,
    Write = 0x2
  };
  
  virtual UDATA renderToFlags()
  {
    UDATA flags = 0;
    
    if(_options & ProtectionOptions::Read) {
      flags |= OMRPORT_PAGE_PROTECT_READ;
    }

    if(_options & ProtectionOptions::Write) {
      flags |= OMRPORT_PAGE_PROTECT_WRITE;
    }

    return flags;
  }
  
protected:
  UDATA _options;
};

#endif
