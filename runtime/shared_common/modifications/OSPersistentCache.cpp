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

#include "OSCacheUtils.hpp"
#include "OSPersistentCache.hpp"

// formerly OSCache::commonStartup. We only kept the directory init
// part, not the cache name initialization logic. We leave
// initCacheName as a pure virtual function.
IDATA OSPersistentCache::initCacheDirName(const char* ctrlDirName, UDATA cacheDirPermissions, I_32 openMode)
{
  OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

  // UDATA cacheNameLen=0, cachePathNameLen=0, versionStrLen=0;
  char fullPathName[OMRSH_MAXPATH];

  // Trc_SHR_OSC_commonStartup_Entry();
  _openMode = openMode;

  //TODO: do we need this now? Yes? No? A: No. It's specified in the ConfigOptions object. Toggling
  //the flag is no longer the responsibility of this class.
  // _isUserSpecifiedCacheDir = (OMR_ARE_ALL_BITS_SET(_runtimeFlags, OMRSHR_RUNTIMEFLAG_CACHEDIR_PRESENT));
  if (!(_cacheLocation = (char*)omrmem_allocate_memory(OMRSH_MAXPATH, OMRMEM_CATEGORY_CLASSES))) {
    //Trc_SHR_OSC_commonStartup_nomem_cacheDirName();
    //OSC_ERR_TRACE(J9NLS_SHRC_OSCACHE_ALLOC_FAILED);
    return -1;
  }

  IDATA rc = OSCacheUtils::getCacheDirName(OMRPORTLIB, ctrlDirName, _cacheDirName, OMRSH_MAXPATH);//, versionData->cacheType);
  if (rc == -1) {
//    Trc_SHR_OSC_commonStartup_getCacheDir_fail();
//    OSC_ERR_TRACE(J9NLS_SHRC_OSCACHE_GETCACHEDIR_FAILED);
    return -1;
  }

  rc = OSCacheUtils::createCacheDir(OMRPORTLIB, _cacheDirName, cacheDirPermissions, ctrlDirName == NULL);
  if (rc == -1) {
//  Trc_SHR_OSC_commonStartup_createCacheDir_fail();
    /* remove trailing '/' */
    _cacheDirName[strlen(_cacheDirName)-1] = '\0';
//  OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_CREATECACHEDIR_FAILED_V2, _cacheDirName);
    return -1;
  }

  /* In the original commonStartup, there are here segments of code
     that should be provided by overloading subclasses, or handled by
     checks to the configuration object. Also, stuff with generation
     and version labels, and buildIDs. How the cachePathName is
     populated using that information. */
}

/* This is an unhelpful name that describes an obscure feature: the
   ability to disclaim regions of memory from page
   protection. Specifically, if an area of memory is no longer needed
   by a program, the program can use the disclaim64 routine to declare
   it as such to the AIX OS. Hence the flag. The omrmmap_dont_need
   routine wraps it inside port/unix/omrmmap.c. If OMR is running on
   Linux or OS X (which is vastly more likely), the call
   madvise((void*)roundedStart, roundedLength, MADV_DONTNEED) is used
   instead (it does the same thing, basically, but because it's an
   "advisement", it shrinks from making hard guarantees). madvise is
   documented here:

   http://man7.org/linux/man-pages/man2/madvise.2.html
*/

//TODO: determine whether this will instead deal with OSCacheRegion's.
void
OSPersistentCache::dontNeedMetadata(const void* startAddress, size_t length)
{
/* AIX does not allow memory to be disclaimed for memory mapped files */
#if !defined(AIXPPC)
  /* why does it need the specific VM?!? It only appears to use it to get at the 
     portLibrary. But the _portLibrary just points to a table of C functions. I can't
     imagine how those addresses might be dependent on the thread or VM objects.
     This seems totally unnecessary to me, since we have _portLibrary at the ready
     in this class. So, I've removed the VM parameter. */
  //OMRPORT_ACCESS_FROM_VMC(currentThread->_vm);
    OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
    omrmmap_dont_need(startAddress, length);
#endif
}
