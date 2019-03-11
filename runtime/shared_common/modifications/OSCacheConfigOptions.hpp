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
#if !defined(OS_CACHE_CONFIG_OPTIONS_HPP_INCLUDED)
#define OS_CACHE_CONFIG_OPTIONS_HPP_INCLUDED

#include "omr.h"

class OSCacheConfigOptions
{
public:
  OSCacheConfigOptions(I_32 _openMode)
    : _openMode(_openMode)
  {}
  
  // cache creation/opening options. used mostly in startup routines.
  enum CreateOptions {
    OpenForStats,
    OpenToDestroy,
    OpenDoNotCreate
  };

  enum RuntimeOptions {
    GETCACHEDIR_USE_USERHOME, // relates to OMRSHMEM_GETDIR_USE_USERHOME
    CACHEDIR_PRESENT // the user specified the cache directory as a runtime flag
  };

  enum VerboseOptions {
  };

  enum StartupReason {
    Stat
  };

  virtual bool useUserHomeDirectoryForCacheDir();
  virtual bool isUserSpecifiedCacheDir();
  // is the cache being opened in read-only mode?
  virtual bool readOnlyOpenMode();
  
  virtual I_32 fileMode();
  virtual I_32 openMode();
  virtual IDATA cacheDirPermissions();

  // TODO: the restore check only applies to the shared memory cache,
  // so we should probably create an OSSharedMemoryCacheConfigOptions
  // subclass, and put it there.. then OSSharedMemoryCache will own a
  // reference to an OSSharedMemoryCacheConfigOptions object.
  virtual bool restoreCheckEnabled();

  // TODO: same as restoreCheckEnabled.
  virtual bool restoreEnabled();

  virtual bool openButDoNotCreate();
  // are we opening the cache in order to destroy?
  virtual bool openToDestroyExistingCache();
  virtual bool openToDestroyExpiredCache();
  virtual bool openToStatExistingCache();

  // reasons for stats. Should have a StatReason enum.
  virtual bool statToDestroy(); // like SHR_STATS_REASON_DESTROY
  virtual bool statExpired(); // like SHR_STATS_REASON_EXPIRE
  virtual bool statIterate(); // like SHR_STATS_REASON_ITERATE
  virtual bool statList(); // like SHR_STATS_REASON_LIST

  virtual void setOpenReason(StartupReason reason);
  virtual void setReadOnlyOpenMode();
  
  // does the cache create a file?
  virtual bool createFile();

  // do we try to open the cache read-only if we failed to open the cache with write permissions?
  virtual bool tryReadOnlyOnOpenFailure();

  // when the cache is corrupt, do we dump its contents?
  virtual bool disableCorruptCacheDumps();
  
  virtual bool verboseEnabled();
  virtual bool groupAccessEnabled();
  // allocate the cache in the user's home directory.
  virtual void useUserHomeDirectoryForCacheLocation();
  // render the options to a bit vector understood by the functions of the OMR port library.
  virtual U_32 renderToFlags();

  virtual UDATA renderCreateOptionsToFlags();

  virtual void setOpenMode(I_32 openMode);
  // flags obviated so far:
  /* appendBaseDir (a variable inside getCacheDir)
     appendBaseDir = (NULL == ctrlDirName) || (OMRPORT_SHR_CACHE_TYPE_NONPERSISTENT == cacheType) || (OMRPORT_SHR_CACHE_TYPE_SNAPSHOT == cacheType);

     flags |= OMRSHMEM_GETDIR_USE_USERHOME; <-- covered by useUserHomeDirectoryForCacheLocation().
   */
protected:
  I_32 _openMode;
  
  CreateOptions _createOptions;
  RuntimeOptions _runtimeOptions;
  VerboseOptions _verboseOptions;
  StartupReason _startupReason;
};

#endif
