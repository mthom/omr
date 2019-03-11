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


#if !defined(OSCACHE_ITERATOR_HPP_INCLUDED)
#define OSCACHE_ITERATOR_HPP_INCLUDED

#include "omr.h"
#include "omrport.h"
#include "ut_omrshr.h"

class OSCacheIterator
{
public:
  // the interface this class replaces:
  
  // static UDATA findfirst(OMRPortLibrary *portLibrary, char *cacheDir, char *resultbuf, UDATA cacheType);
  // static I_32 findnext(OMRPortLibrary *portLibrary, UDATA findHandle, char *resultbuf, UDATA cacheType);

  // this design assumes the missing arguments will be available as fields of the OSCacheIterator subclasses.
  
  virtual UDATA findFirst(OMRPortLibrary *portLibrary) = 0;
  virtual I_32 findNext(OMRPortLibrary *portLibrary, UDATA findHandle) = 0;
  virtual void findClose(OMRPortLibrary *portLibrary, UDATA findHandle) = 0;
};

#endif
