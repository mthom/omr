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


#if !defined(OSCACHE_HPP_INCLUDED)
#define OSCACHE_HPP_INCLUDED

#include "OSCacheRegion.hpp"

#include "omr.h"
#include "sharedconsts.h"

// this was formerly a struct with just two fields, but I can see no
// reason it should not be a class -- it's not ever used in a C
// exclusive context. Also, the block of code that populates its
// fields is repeated verbatim throughout the SCC, so why shouldn't it
// be a member function?
struct LastErrorInfo
{
  I_32 _lastErrorCode;
  const char* _lastErrorMsg;

  LastErrorInfo()
    : _lastErrorCode(0)
    , _lastErrorMsg(NULL)
  {}

  void populate() {
    lastErrorCode = omrerror_last_error_number();
    lastErrorMsg = omrerror_last_error_message();
  }
};

class OSCache {
public:
  // get the data and total sizes of the cache.
  virtual U_32 getDataSize() = 0;
  virtual U_32 getTotalSize() = 0;

  // moduleName, whaaa?? I'm not sure.
  virtual void errorHandler(U_32 moduleName, U_32 id, LastErrorInfo *lastErrorInfo) = 0;

  // this as well.
  virtual SH_CacheAccess isCacheAccessible(void) const { return J9SH_CACHE_ACCESS_ALLOWED; }

  // this belongs to the OSCache hierarchy at the highest level. That's clear, I think.
  virtual void getCorruptionContext(IDATA *corruptionCode, UDATA *corruptValue);
  virtual void setCorruptionContext(IDATA corruptionCode, UDATA corruptValue);

  // this is a factory method that produces visitor objects.
  // these visitors codify how to serialize a region.

  // But! We have different serializers for different region
  // types. So, I'm not sure this is the best place for this. Granted,
  // the OSCache should decide how to serialize a region, yes. But is
  // it going to serialize all regions identically? Obviously not.

  // actually, it's fine. when it visits, it can specialize its
  // argument on the various OSCacheRegion types.
  virtual OSCacheRegionSerializer* constructSerializer() = 0;

  // This is tricky. Initially I thought this function should be
  // inside the OSCacheLayout, which doesn't only describe the
  // sequencing of regions within the cache, but also indicates
  // whether the regions are aligned along page boundaries.  But,
  // there's also a literal 'start' to the cache, as the address of a
  // block of memory. OSCacheLayout knows nothing about this
  // block.. it simply specifies how to lay everything out. So, it's
  // really the OSCache that knows, as the owner and consolidator of
  // that information. It should be able to bring that all to the fore
  // in computing this.
  virtual void * getOSCacheStart();

  // cleanup resources used by the cache. I'm half-convinced this should occupy a
  // Destruction policy object.
  virtual void cleanup(void) = 0;
  // deletes the shared class. contains an option to suppress verbose trace messages.
  virtual IDATA destroy(bool suppressVerbose, bool isReset = false) = 0;
  // reports on the contents of.. error codes and the corruption context.
  virtual IDATA getError() = 0;
  // calls routines to finalize/shutdown the cache when OMR is exiting.
  virtual void runExitCode(void) = 0;
  // the region object has an internal value, "flags", that specify
  // permissions settings, and a few other things.
  virtual IDATA setRegionPermissions(OMRPortLibrary* portLibrary, OSCacheRegion* region) = 0;
  // returns the size of the smallest region on which permissions can be set. A return value of 0
  // means the setting of permissions is unsupported.
  virtual UDATA getPermissionsRegionGranularity(OMRPortLibrary* portLibrary, OSCacheRegion* region) = 0;

protected:
  IDATA _corruptionCode;
  UDATA _corruptValue;

  //  it's best if caches embed their own configuration objects.
  //  OSCacheConfig* _config;
  OMRPortLibrary* _portLibrary;
  OSCacheConfigOptions _configOptions;
};

#endif
