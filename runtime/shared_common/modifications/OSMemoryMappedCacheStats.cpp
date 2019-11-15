#include "OSCacheImpl.hpp"
#include "OSCacheUtils.hpp"
#include "OSMemoryMappedCacheStats.hpp"

#include "ut_omrshr_mods.h"

/**
 * Method to get the statistics for a shared classes cache
 * 
 * Needs to be able to get stats for all cache generations
 * 
 * This method returns the last attached, detached and created times,
 * whether the cache is in use and that it is a persistent cache.
 * 
 * Details of data held in the cache data area are not accessed here
 * 
 * @param [in] vm The Java VM
 * @param [in] ctrlDirName  Cache directory
 * @param [in] cacheNameWithVGen Filename of the cache to stat
 * @param [out]	cacheInfo Pointer to the structure to be completed with the cache's details
 * @param [in] reason Indicates the reason for getting cache stats. Refer sharedconsts.h for valid values.
 * 
 * @return 0 on success and -1 for failure
 */
IDATA
OSMemoryMappedCacheStats::prepareAndGetCacheStats() // SH_OSCache_Info *cacheInfo)
{
        OMRPORT_ACCESS_FROM_OMRPORT(_cache->_portLibrary);
	/* Using 'SH_OSCachemmap cacheStruct' breaks the pattern of calling getRequiredConstrBytes(), and then allocating memory.
	 * However it is consistent with 'SH_OSCachesysv::getCacheStats'.
	 */
	/*
	SH_OSCachemmap cacheStruct;
	SH_OSCachemmap *cache = NULL;
	void *cacheHeader;
	I_64 *timeValue;
	I_32 inUse;
	*/
	IDATA lockRc;
	/*
	OMRSharedCachePreinitConfig piconfig;
	J9PortShcVersion versionData;
	UDATA reasonForStartup = SHR_STARTUP_REASON_NORMAL;
	
	Trc_SHR_OSC_Mmap_getCacheStats_Entry(cacheNameWithVGen, cacheInfo);

	getValuesFromShcFilePrefix(portLibrary, cacheNameWithVGen, &versionData);
	versionData.cacheType = OMRPORT_SHR_CACHE_TYPE_PERSISTENT;
	*/

//	if (removeCacheVersionAndGen(cacheInfo->name, CACHE_ROOT_MAXLEN, J9SH_VERSION_STRING_LEN+1, cacheNameWithVGen) != 0) {
//		return -1;
//	}
	
	OSCacheConfigOptions* configOptions = _cache->_configOptions;

	if (configOptions->statToDestroy()) {
	   configOptions->setOpenReason(OSCacheConfigOptions::StartupReason::Destroy);
	} else if (configOptions->statExpired()) {
	   configOptions->setOpenReason(OSCacheConfigOptions::StartupReason::Expired);
        }

	/* We try to open the cache read/write */
	if (!_cache->startup(_cache->_cacheName, _cache->_cacheLocation)) {
	   /* If that fails - try to open the cache read-only */
	   configOptions->setReadOnlyOpenMode();

	   if (!_cache->startup(_cache->_cacheName, _cache->_cacheLocation)) {
	      _cache->cleanup();
	      return -1;
	   }
	} else {
	   /* Try to acquire the attach write lock. This will only succeed if noone else
	    * has the attach read lock i.e. there is noone connected to the cache */
	   lockRc = _cache->_config->tryAcquireAttachWriteLock(OMRPORTLIB);
	   if (0 == lockRc) {
	      Trc_SHR_OSC_Mmap_getCacheStats_cacheNotInUse();
	      _inUse = 0;
	      _cache->_config->releaseAttachWriteLock(OMRPORTLIB);
	   } else {
	     Trc_SHR_OSC_Mmap_getCacheStats_cacheInUse();
	     _inUse = 1;
	   }	   
	}

	if (!configOptions->openToDestroyExistingCache()) {
	   IDATA rc = _cache->internalAttach();
	   if (0 != rc) {
	      _cache->setError(rc);
	      _cache->cleanup();
	      return -1;
	   }
	}

	if (configOptions->statIterate()) {
	   getCacheStats();
	}

	_cache->internalDetach();
	_cache->cleanup();

	return 0;
}
