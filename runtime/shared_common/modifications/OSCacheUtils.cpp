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
#include "omrcfg.h"
#include "omrargscan.h"
#include "omrport.h"
#include "pool_api.h"
#include "shrnls.h"
#include "omrsharedhelper.h" // see omrsharedhelper.c for
                             // cleanSharedMemorySegments, which is
                             // called from omrshmem_createDir to
                             // "clean the memory segments." More
                             // comments in the source.
#include "ut_omrshr.h"

#include "OSCacheConfigOptions.hpp"
#include "OSCacheUtils.hpp"

namespace OSCacheUtils
{
// writes the name of the cache file directory/control directory to
// buffer.  if ctrlDirName == NULL, it winds up being the /tmp
// directory, or if group access is disabled, the user's home
// directory.

// this was originally called getCacheDir -- I renamed it to do away with
// the ambiguity of 'getCacheDir', ie. we are 'getting' the directory? what?
IDATA
getCacheDirName(OMRPortLibrary* portLibrary, const char* ctrlDirName, char* buffer, UDATA bufferSize, OSCacheConfigOptions* configOptions)
{
  OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
  IDATA rc;

  //Trc_SHR_OSC_getCacheDir_Entry();

  /* Cache directory used is the j9shmem dir, regardless of whether we're using j9shmem or j9mmap */

  // appendBaseDir is left up to the ConfigOptions object now.
//  appendBaseDir = (NULL == ctrlDirName) || (OMRPORT_SHR_CACHE_TYPE_NONPERSISTENT == cacheType) || (OMRPORT_SHR_CACHE_TYPE_SNAPSHOT == cacheType);
//  if (appendBaseDir) {
//    flags |= OMRSHMEM_GETDIR_APPEND_BASEDIR;
//  }
  if ((NULL == ctrlDirName) && !configOptions->groupAccessEnabled())
  {
		/* omrshmem_getDir() always tries the CSIDL_LOCAL_APPDATA directory (C:\Documents and Settings\username\Local Settings\Application Data)
		 * first on Windows if ctrlDirName is NULL, regardless of whether OMRSHMEM_GETDIR_USE_USERHOME is set or not. So OMRSHMEM_GETDIR_USE_USERHOME is effective on UNIX only.
		 */

    configOptions->useUserHomeDirectoryForCacheDir();
    //flags |= OMRSHMEM_GETDIR_USE_USERHOME;
  }

  rc = omrshmem_getDir(ctrlDirName, configOption->renderToFlags(), buffer, bufferSize);
  if (rc == -1) {
    // //  Trc_SHR_OSC_getCacheDir_omrshmem_getDir_failed(ctrlDirName);
    return -1;
  }

  //Trc_SHR_OSC_getCacheDir_Exit();
  return 0;
}

/**
 * Create the directory to use for the cache file or control file(s)
 *
 * @param [in] portLibrary  A portLibrary
 * @param[in] cacheDirName The name of the cache directory
 * @param[in] cacheDirPermissions The permissions of the created cache directory
 * @param[in] cleanMemorySegments, set TRUE to call cleanSharedMemorySegments. It will clean sysv memory segments.
 * 				It should be set to TRUE if the ctrlDirName is NULL. This stops shared memory
 *                              from being leaked when wiped from /tmp, the default sysv location if ctrlDirName
 *                              is NULL.
 *
 * Returns -1 for error, >=0 for success
 */
IDATA
createCacheDir(OMRPortLibrary* portLibrary, char* cacheDirName, UDATA cacheDirPermissions, bool cleanMemorySegments)
{
  OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
  IDATA rc;

  //Trc_SHR_OSC_createCacheDir_Entry(cacheDirName, cleanMemorySegments);

  rc = omrshmem_createDir(cacheDirName, cacheDirPermissions, cleanMemorySegments);

  //Trc_SHR_OSC_createCacheDir_Exit();
  return rc;
}

/* Returns the full path of a cache based on the current cacheDir value */
IDATA
getCachePathName(OMRPortLibrary* portLibrary, const char* cacheDirName, char* buffer, UDATA bufferSize)//, const char* cacheNameWithVGen)
{
  OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

  //Trc_SHR_OSC_getCachePathName_Entry(cacheNameWithVGen);

  omrstr_printf(buffer, bufferSize, "%s", cacheDirName);//, cacheNameWithVGen);

  //Trc_SHR_OSC_getCachePathName_Exit();

  return 0;
}

UDATA
statCache(OMRPortLibrary* portLibrary, const char* cacheDirName, const char* cacheName, bool displayNotFoundMsg)
{
  OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
  char fullPath[OMRSH_MAXPATH];

  Trc_SHR_OSC_statCache_Entry(cacheName); // cacheNameWithVGen);

  omrstr_printf(fullPath, OMRSH_MAXPATH, "%s%s", cacheDirName, cacheName); // cacheNameWithVGen);
  if (omrfile_attr(fullPath) == EsIsFile) {
    Trc_SHR_OSC_statCache_cacheFound();
    return 1;
  }

  if (displayNotFoundMsg) {
    omrnls_printf(OMRNLS_ERROR, OMRNLS_SHRC_OSCACHE_NOT_EXIST);
  }

  Trc_SHR_OSC_statCache_cacheNotFound();
  return 0;
}

}
#endif
