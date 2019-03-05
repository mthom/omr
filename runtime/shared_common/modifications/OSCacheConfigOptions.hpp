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

class OSCacheConfigOptions
{
public:
  enum CreateOptions {
  };

  enum RuntimeOptions {
    GETCACHEDIR_USE_USERHOME, // relates to OMRSHMEM_GETDIR_USE_USERHOME
    CACHEDIR_PRESENT // the user specified the cache directory as a runtime flag
  };

  enum VerboseOptions {
  };

  enum StartupReason {
  };

  virtual bool isUserSpecifiedCacheDir();
  // is the cache being opened in read-only mode?
  virtual bool readOnlyOpenMode();
  
  virtual I_32 fileMode();
  virtual I_32 openMode();
  
  // are we opening the cache in order to destroy?
  virtual bool openToDestroyExistingCache();
  virtual bool openToDestroyExpiredCache();
  virtual bool openToStatExistingCache();
  
  // does the cache create a file?
  virtual bool createFile();

  // do we try to open the cache read-only if we failed to open the cache with write permissions?
  virtual bool tryReadOnlyOnOpenFailure();

  virtual bool verboseEnabled();
  virtual bool groupAccessEnabled();
  // allocate the cache in the user's home directory.
  virtual void useUserHomeDirectoryForCacheLocation();
  // render the object's options to a bit vector understood by the functions of the OMR port library.
  virtual U_32 renderToFlags();

  // flags obviated so far:
  /* appendBaseDir (a variable inside getCacheDir)
     appendBaseDir = (NULL == ctrlDirName) || (OMRPORT_SHR_CACHE_TYPE_NONPERSISTENT == cacheType) || (OMRPORT_SHR_CACHE_TYPE_SNAPSHOT == cacheType);

     flags |= OMRSHMEM_GETDIR_USE_USERHOME; <-- covered by useUserHomeDirectoryForCacheLocation().
   */
protected:
  // TODO: move _openMode here. in the persistent version.
  
  CreateOptions _createOptions;
  RuntimeOptions _runtimeOptions;
  VerboseOptions _verboseOptions;
  StartupReason _startupReason;
};

#endif
