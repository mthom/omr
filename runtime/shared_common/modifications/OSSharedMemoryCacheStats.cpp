#include "OSCacheImpl.hpp"
#include "OSCacheUtils.hpp"
#include "OSSharedMemoryCacheUtils.hpp"
#include "OSSharedMemoryCacheStats.hpp"

#include "ut_omrshr_mods.h"

/**
 * Populates cacheInfo with cache statistics.
 *
 * @param [in] vm  The Java VM
 * @param [in] ctrlDirName  Cache directory
 * @param [in] groupPerm  Group permissions to open the cache directory
 * @param [in] cacheNameWithVGen current cache name
 * @param [out] cacheInfo stucture to be populated with cache statistics
 * @param [in] reason Indicates the reason for getting cache stats. Refer sharedconsts.h for valid values.
 *
 * @return 0 on success
 *
 * was: getCacheStats
 *
 */
IDATA
OSSharedMemoryCacheStats::prepareAndGetCacheStats()//SH_OSCache_Info* cacheInfo)
{
  IDATA retval = 0;
  OMRPORT_ACCESS_FROM_OMRPORT(_cache->_portLibrary);
  char cacheDirName[OMRSH_MAXPATH];

  OSCacheConfigOptions* configOptions = _cache->_configOptions;
  
  OSCacheUtils::getCacheDirName(OMRPORTLIB, _cache->_cacheLocation, cacheDirName, OMRSH_MAXPATH, configOptions);//, OMRPORT_SHR_CACHE_TYPE_NONPERSISTENT);
  if (getCacheStatsHelper(cacheDirName) == 0) {
    /* Using 'SH_OSCachesysv cacheStruct' breaks the pattern of calling getRequiredConstrBytes(), and then allocating memory.
     * However it is consistent with 'SH_OSCachemmap::getCacheStats'.
		 */
    
    // the cache is now passed in as a parameter! The caller should
    // have the responsibility of constructing the cache.
    
//    OSSharedMemoryCache cacheStruct;
//    OSSharedMemoryCache* cache = NULL;
    
//    J9PortShcVersion versionData;
//    OMRSharedCachePreinitConfig piconfig;
    bool attachedMem = false;

//    getValuesFromShcFilePrefix(OMRPORTLIB, cacheNameWithVGen, &versionData);
//    versionData.cacheType = OMRPORT_SHR_CACHE_TYPE_NONPERSISTENT;

    if (configOptions->statIterate() || configOptions->statList()) { // (SHR_STATS_REASON_ITERATE == reason) || (SHR_STATS_REASON_LIST == reason)) {
      // create a cache with a single lock. or at least, we did! It should have at least a single lock.
      // cache = new OSSharedMemoryCache(library, cacheName, cacheDirName, 1, configOptions); //(SH_OSCachesysv *) SH_OSCache::newInstance(OMRPORTLIB, &cacheStruct, cacheInfo->name, cacheInfo->generation, &versionData);
      
      if (!_cache->startup(_cache->_cacheName, _cache->_cacheLocation)) {
	goto done;
      }

      /* Avoid calling attach() for incompatible cache since its anyway going to fail.
       * But we can still get semid from _semhandle as it got populated during startup itself.
       */
      // this has been specialized to OSSharedMemoryCache, so the isCompatible flag is no longer needed.
      // if (0 == cacheInfo->isCompatible) {
      /* if the cache was opened read-only, there is no _semhandle */
//      if (NULL != _cache->_config->_semhandle) {
//	/* SHR_STATS_REASON_LIST needs to populate the os_semid */
//	cacheInfo->os_semid = _cache->_config->_semid = omrshsem_deprecated_getid(_cache->_config->_semhandle);
//      }
//      } else {
      
      /* Attach to the cache, so we can read the fields in the header */
      if (NULL == _cache->attach()) {//omr_vmthread_getCurrent(vm), NULL)) {
	_cache->cleanup();
	goto done;
      }
	
      attachedMem = true;

      /* SHR_STATS_REASON_LIST needs to populate the os_semid */
//      if (0 != _cache->_config->_semid) {
//	   cacheInfo->os_semid = _cache->_config->_semid;
//      }
	
      if (configOptions->statIterate()) {//SHR_STATS_REASON_ITERATE == reason) {
	// this is a pure virtual function! it's meant to be overloaded.
	// was getCacheStatsCommon.
	getCacheStats();
      }
	
      if (attachedMem == true) {
	_cache->detach();
      }
    }
      
    _cache->cleanup();
  } else {
    retval = -1;
  }
  
 done:
  return retval;
}

/**
 * Populates some of cacheInfo for 'SH_OSCachesysv::getCacheStats' and 'SH_OSCachesysv::getJavacoreData'
 *
 * @param [in] vm  The Java VM
 * @param [in] ctrlDirName  Cache directory
 * @param [in] groupPerm  Group permissions to open the cache directory
 * @param [in] cacheNameWithVGen current cache name
 * @param [out] cacheInfo stucture to be populated with cache statistics
 * @param [in] reason Indicates the reason for getting cache stats. Refer sharedconsts.h for valid values.
 * @return 0 on success
 */
IDATA
OSSharedMemoryCacheStats::getCacheStatsHelper(const char* cacheDirName)//, SH_OSCache_Info* cacheInfo)
{
  OMRPORT_ACCESS_FROM_OMRPORT(_cache->_portLibrary);//vm->_runtime->_portLibrary);
  OMRPortShmemStatistic statbuf;

  UDATA statrc = 0;
  OSCacheConfigOptions* configOptions = _cache->_configOptions;
    
  Trc_SHR_OSC_Sysv_getCacheStatsHelper_Entry(_cache->_cacheName);

  // J9 specific. notice how it strips the version and gen tags to get to the core cache name!
  // hence, we just start with the core cache name, and work around the need for this step.
//  if (removeCacheVersionAndGen(cacheInfo->name, CACHE_ROOT_MAXLEN, versionLen, cacheNameWithVGen) != 0) {
//    Trc_SHR_OSC_Sysv_getCacheStatsHelper_removeCacheVersionAndGenFailed();
//    return -1;
//  }
  
#if !defined(WIN32)
  statrc = OSSharedMemoryCacheUtils::StatSysVMemoryHelper(OMRPORTLIB, cacheDirName, configOptions->groupPermissions(),
							  _cache->_cacheName, &statbuf);
#else
  statrc = omrshmem_stat(cacheDirName, configOptions->groupPermissions(), _cache->_cacheName, &statbuf);
#endif
  /*  
  if (statrc == 0) {
    // J9 specific.
//#if defined(J9ZOS390)
//    if ((J9SH_ADDRMODE != cacheInfo->versionData.addrmode) && (0 == statbuf.size)) {
//      // JAZZ103 PR 79909 + PR 88930: On z/OS, if the shared memory segment of the cache is created by a 64-bit JVM but
//      // is being accessed by a 31-bit JVM, then the size of shared memory segment may not be verifiable.
//      // In such cases, we should avoid doing any operation - destroy or list -
//      // to avoid accidental access to shared memory segment belonging to some other application.
//       
//      Trc_SHR_Assert_True(OMRSH_ADDRMODE_32 == OMRSH_ADDRMODE);
//      omrnls_printf( J9NLS_INFO, J9NLS_SHRC_OSCACHE_SHARED_CACHE_SIZE_ZERO_AND_DIFF_ADDRESS_MODE, J9SH_ADDRMODE_64, J9SH_ADDRMODE_64, cacheInfo->name, J9SH_ADDRMODE_32);
//      Trc_SHR_OSC_Sysv_getCacheStatsHelper_shmSizeZeroDiffAddrMode();
//      return -1;
//    }
//#endif // defined(J9ZOS390)
    cacheInfo->os_shmid = (statbuf.shmid!=(UDATA)-1)?statbuf.shmid:(UDATA)OMRSH_OSCACHE_UNKNOWN;
    // os_semid is populated in getJavacoreData() as we can't access _semid here
    cacheInfo->os_semid = (UDATA)OMRSH_OSCACHE_UNKNOWN;
    cacheInfo->lastattach = (statbuf.lastAttachTime!=-1)?(statbuf.lastAttachTime*1000):OMRSH_OSCACHE_UNKNOWN;
    cacheInfo->lastdetach = (statbuf.lastDetachTime!=-1)?(statbuf.lastDetachTime*1000):OMRSH_OSCACHE_UNKNOWN;
    cacheInfo->createtime = OMRSH_OSCACHE_UNKNOWN;
    cacheInfo->nattach = (statbuf.nattach!=(UDATA)-1)?statbuf.nattach:(UDATA)OMRSH_OSCACHE_UNKNOWN;
  } else if (configOptions->statToDestroy() || configOptions->statExpired()) { //(SHR_STATS_REASON_DESTROY == reason) || (SHR_STATS_REASON_EXPIRE == reason)) {
    // When destroying the cache, we can ignore failure to get shared memory stats.
    // This allows to delete control files for cache which does not have shared memory.
    
    cacheInfo->os_shmid = (UDATA)OMRSH_OSCACHE_UNKNOWN;
    cacheInfo->os_semid = (UDATA)OMRSH_OSCACHE_UNKNOWN;
    cacheInfo->lastattach = OMRSH_OSCACHE_UNKNOWN;
    cacheInfo->lastdetach = OMRSH_OSCACHE_UNKNOWN;
    cacheInfo->createtime = OMRSH_OSCACHE_UNKNOWN;
    cacheInfo->nattach = (UDATA)OMRSH_OSCACHE_UNKNOWN;
  } else {
    Trc_SHR_OSC_Sysv_getCacheStatsHelper_cacheStatFailed();
    return -1;
  }
  */
  Trc_SHR_OSC_Sysv_getCacheStatsHelper_Exit();
  return statrc;
}

