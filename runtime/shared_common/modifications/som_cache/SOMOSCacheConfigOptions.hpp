#if !defined(SOM_OS_CACHE_CONFIG_OPTIONS_HPP_INCLUDED)
#define SOM_OS_CACHE_CONFIG_OPTIONS_HPP_INCLUDED

#include "OSCacheConfigOptions.hpp"

#include "env/TRMemory.hpp"

#include "omr.h"
#include "sharedconsts.h" // for the J9SH_CACHE_FILE_MODE_USERDIR_WITH_GROUPACCESS, etc.

#define MAX_CRC_SAMPLES 1024

class SOMOSCacheConfigOptions: public OSCacheConfigOptions
{
public:
  TR_ALLOC(TR_Memory::SharedCacheConfig)
  
  SOMOSCacheConfigOptions(UDATA cacheSize)
    : OSCacheConfigOptions(J9OSCACHE_OPEN_MODE_GROUPACCESS)
    , _cacheSize(cacheSize)
  {}

  virtual ~SOMOSCacheConfigOptions() {}

  bool useUserHomeDirectoryForCacheDir() override {
    return true;
  }

  bool isUserSpecifiedCacheDir() override {
    return false;
  }
  
  // is the cache being opened in read-only mode?
  bool readOnlyOpenMode() override {
    return false;
  }

  I_32 fileMode() override {
    I_32 perm = 0;

    Trc_SHR_OSC_Mmap_getFileMode_Entry();

    if (isUserSpecifiedCacheDir()) {
      if (openMode() & J9OSCACHE_OPEN_MODE_GROUPACCESS) {
	perm = J9SH_CACHE_FILE_MODE_USERDIR_WITH_GROUPACCESS;
      } else {
	perm = J9SH_CACHE_FILE_MODE_USERDIR_WITHOUT_GROUPACCESS;
      }
    } else {
      if (groupAccessEnabled()) { //openMode() & J9OSCACHE_OPEN_MODE_GROUPACCESS) {
	perm = J9SH_CACHE_FILE_MODE_DEFAULTDIR_WITH_GROUPACCESS;
      } else {
	perm = J9SH_CACHE_FILE_MODE_DEFAULTDIR_WITHOUT_GROUPACCESS;
      }
    }

    Trc_SHR_OSC_Mmap_getFileMode_Exit(openMode(), perm);
    return perm;
  }

  bool semaphoreCheckEnabled() override {
    return false;
  }
  
  // let's say this is 8MB for now? I dunno.
  U_32 dataSectionSize() {
    return 8 * 1024 * 1024;
  }

  // the open mode flags are defined in sharedconsts.h
  I_32 openMode() override {
    return _openMode;
  }

  // returns 1 iff groupAccessEnabled() == true. sometimes we need a UDATA.
  UDATA groupPermissions() override {
    return 1; // yes, group permissions are enabled.
  }

  IDATA cacheDirPermissions() override {
    return OMRSH_DIRPERM_DEFAULT;
  }
  
  bool usingNetworkCache() override {
    return true;
  }

  // TODO: the restore check only applies to the shared memory cache,
  // so we should probably create an OSSharedMemoryCacheConfigOptions
  // subclass, and put it there.. then OSSharedMemoryCache will own a
  // reference to an OSSharedMemoryCacheConfigOptions object.
  bool restoreCheckEnabled() override {
    return false;
  }

  bool openButDoNotCreate() override {
    return false;
  }
  
  // are we opening the cache in order to destroy?
  bool openToDestroyExistingCache() override {
    return false;
  }
  
  bool openToDestroyExpiredCache() override {
    return false;
  }
  
  bool openToStatExistingCache() override {
    return _startupReason == StartupReason::Stat;
  }

  // reasons for stats. Should have a StatReason enum.
  bool statToDestroy() override {
    return _statReason == StatReason::Destroy; // like SHR_STATS_REASON_DESTROY
  }

  bool statExpired() override {
    return _statReason == StatReason::Expired; // like SHR_STATS_REASON_EXPIRE
  }
  
  bool statIterate() override {
    return _statReason == StatReason::Iterate; // like SHR_STATS_REASON_ITERATE
  }
  
  bool statList() override {
    return _statReason == StatReason::List; // like SHR_STATS_REASON_LIST
  }

  OSCacheConfigOptions& setOpenReason(StartupReason reason) override {
    _startupReason = reason;
    return *this;
  }
  
  OSCacheConfigOptions& setReadOnlyOpenMode() override { return *this; }
  OSCacheConfigOptions& setOpenMode(I_32) override { return *this; }
  
  OSCacheConfigOptions& setStatReason(StatReason reason) {
    _statReason = reason;
    return *this;
  }
  
  // the block size of the cache.
  U_32 cacheSize() override {
    return _cacheSize;
  }
  
  OSCacheConfigOptions& setCacheSize(U_32 size) override {
    _cacheSize = size;
    return *this;
  }

  U_32 maxCRCSamples() override {
    return MAX_CRC_SAMPLES;
  }

  // does the cache create a file?
  bool createFile() override {
    return true;
  }

  // do we try to open the cache read-only if we failed to open the cache with write permissions?
  bool tryReadOnlyOnOpenFailure() override {
    return false;
  }

  // when the cache is corrupt, do we dump its contents?
  bool disableCorruptCacheDumps() override {
    return true;
  }

  bool verboseEnabled() override {
    return false;
  }

  void resetVerboseOptionsFromFlags(UDATA) override {}
  
  bool groupAccessEnabled() override {
    return true; // true iff _groupPerm = 1 in the J9 cache.
  }
  
  // allocate the cache in the user's home directory.
  // we already do this, so this function does nothing.
  void useUserHomeDirectoryForCacheLocation() override {}
  
  // render the options to a bit vector understood by the functions of the OMR port library.
  // the only place this is used currently is inside OSCacheUtils.cpp, exactly once, to
  // open a shared memory block. So, the contents of this member function are derived from
  // OSCache.cpp:getCacheDir.
  U_32 renderToFlags() override {
    U_32 flags = OMRSHMEM_GETDIR_APPEND_BASEDIR;

    if(!groupAccessEnabled()) {
      flags |= OMRSHMEM_GETDIR_USE_USERHOME;
    }

    return flags;
  }

  UDATA renderCreateOptionsToFlags() override {
    return 0;
  }
  
  UDATA renderVerboseOptionsToFlags() override {
    return 0;
  }

protected:
  U_32 _cacheSize;
  StatReason _statReason;
};

#endif
