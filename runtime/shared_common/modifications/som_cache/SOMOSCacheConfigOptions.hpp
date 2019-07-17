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

  bool useUserHomeDirectoryForCacheDir() {
    return true;
  }

  bool isUserSpecifiedCacheDir() {
    return false;
  }
  // is the cache being opened in read-only mode?
  bool readOnlyOpenMode() {
    return false;
  }

  I_32 fileMode() {
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

  bool semaphoreCheckEnabled() {
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
  
  bool openToStatExistingCache() {
    return false;
  }

  // reasons for stats. Should have a StatReason enum.
  bool statToDestroy() {
    return false; // like SHR_STATS_REASON_DESTROY
  }

  bool statExpired() {
    return false; // like SHR_STATS_REASON_EXPIRE
  }
  
  bool statIterate() {
    return false; // like SHR_STATS_REASON_ITERATE
  }
  
  bool statList() {
    return false; // like SHR_STATS_REASON_LIST
  }

  OSCacheConfigOptions& setOpenReason(StartupReason) { return *this; }
  OSCacheConfigOptions& setReadOnlyOpenMode() { return *this; }
  OSCacheConfigOptions& setOpenMode(I_32) { return *this; }

  // the block size of the cache.
  U_32 cacheSize() {
    return _cacheSize;
  }
  
  OSCacheConfigOptions& setCacheSize(U_32 size) {
    _cacheSize = size;
    return *this;
  }

  U_32 maxCRCSamples() {
    return MAX_CRC_SAMPLES;
  }

  // does the cache create a file?
  bool createFile() {
    return true;
  }

  // do we try to open the cache read-only if we failed to open the cache with write permissions?
  bool tryReadOnlyOnOpenFailure() {
    return false;
  }

  // when the cache is corrupt, do we dump its contents?
  bool disableCorruptCacheDumps() {
    return true;
  }

  bool verboseEnabled() {
    return false;
  }

  void resetVerboseOptionsFromFlags(UDATA) {}
  
  bool groupAccessEnabled() override {
    return true; // true iff _groupPerm = 1 in the J9 cache.
  }
  
  // allocate the cache in the user's home directory.
  // we already do this, so this function does nothing.
  void useUserHomeDirectoryForCacheLocation() {}
  
  // render the options to a bit vector understood by the functions of the OMR port library.
  // the only place this is used currently is inside OSCacheUtils.cpp, exactly once, to
  // open a shared memory block. So, the contents of this member function are derived from
  // OSCache.cpp:getCacheDir.
  U_32 renderToFlags() {
    U_32 flags = OMRSHMEM_GETDIR_APPEND_BASEDIR;

    if(!groupAccessEnabled()) {
      flags |= OMRSHMEM_GETDIR_USE_USERHOME;
    }

    return flags;
  }

  UDATA renderCreateOptionsToFlags() {
    return 0;
  }
  
  UDATA renderVerboseOptionsToFlags() {
    return 0;
  }

  // flags obviated so far:
  /* appendBaseDir (a variable inside getCacheDir)
     appendBaseDir = (NULL == ctrlDirName) || (OMRPORT_SHR_CACHE_TYPE_NONPERSISTENT == cacheType) || (OMRPORT_SHR_CACHE_TYPE_SNAPSHOT == cacheType);

     flags |= OMRSHMEM_GETDIR_USE_USERHOME; <-- covered by useUserHomeDirectoryForCacheLocation().
   */
protected:
  U_32 _cacheSize;
//
//  CreateOptions _createOptions;
//  RuntimeOptions _runtimeOptions;
//  VerboseOptions _verboseOptions;
//  StartupReason _startupReason;
};

#endif
