#if !defined(WASM_OS_CACHE_CONFIG_OPTIONS_HPP_INCLUDED)
#define WASM_OS_CACHE_CONFIG_OPTIONS_HPP_INCLUDED

#include "OSCacheConfigOptions.hpp"

#include "omr.h"

class WASMOSCacheConfigOptions: public OSCacheConfigOptions
{
public:
  WASMOSCacheConfigOptions();

  // cache creation/opening options. used mostly in startup routines.
//  enum CreateOptions {
//    OpenForStats,
//    OpenToDestroy,
//    OpenDoNotCreate
//  };
//
//  enum RuntimeOptions {
//    GETCACHEDIR_USE_USERHOME, // relates to OMRSHMEM_GETDIR_USE_USERHOME
//    CACHEDIR_PRESENT // the user specified the cache directory as a runtime flag
//  };
//
//  enum VerboseOptions {
//  };
//
//  enum StartupReason {
//    Stat
//  };

  virtual bool useUserHomeDirectoryForCacheDir() {
    return true;
  }
  
  virtual bool isUserSpecifiedCacheDir() {
    return false;
  }
  // is the cache being opened in read-only mode?
  virtual bool readOnlyOpenMode() {
    return false;
  }

  virtual I_32 fileMode() {
  }
  
  virtual I_32 openMode(); //TODO: this should set _groupPerm = 1 if groupAccessEnabled() is true.
  virtual UDATA groupPermissions(); // returns 1 iff groupAccessEnabled() == true. sometimes we need a UDATA.

  virtual IDATA cacheDirPermissions();
  virtual bool usingNetworkCache();

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

  virtual OSCacheConfigOptions& setOpenReason(StartupReason reason);
  virtual OSCacheConfigOptions& setReadOnlyOpenMode();
  virtual OSCacheConfigOptions& setOpenMode(I_32 openMode);

  // the block size of the cache.
  virtual U_32 cacheSize() = 0;
  virtual OSCacheConfigOptions& setCacheSize(uintptr_t size) = 0;

  virtual U_32 maxCRCSamples();

  // does the cache create a file?
  virtual bool createFile();

  // do we try to open the cache read-only if we failed to open the cache with write permissions?
  virtual bool tryReadOnlyOnOpenFailure();

  // when the cache is corrupt, do we dump its contents?
  virtual bool disableCorruptCacheDumps();

  virtual bool verboseEnabled();
  virtual bool groupAccessEnabled(); // true iff _groupPerm = 1 in the J9 cache.
  // allocate the cache in the user's home directory.
  virtual void useUserHomeDirectoryForCacheLocation();
  // render the options to a bit vector understood by the functions of the OMR port library.
  virtual U_32 renderToFlags();

  virtual UDATA renderCreateOptionsToFlags();
  virtual UDATA renderVerboseOptionsToFlags();

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
