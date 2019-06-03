/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#include "modronbase.h"

#include "../../../omrglue/CollectorLanguageInterfaceImpl.hpp"
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
#include "ConcurrentSafepointCallback.hpp"
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */
#if defined(OMR_GC_MODRON_COMPACTION)
#include "CompactScheme.hpp"
#endif /* OMR_GC_MODRON_COMPACTION */
#include "EnvironmentStandard.hpp"
#include "GCExtensionsBase.hpp"
#include "MarkingScheme.hpp"
#include "ObjectIterator.hpp"
#include "mminitcore.h"
#include "omr.h"
#include "omrvm.h"
#include "OMRVMInterface.hpp"
#include "ParallelTask.hpp"
// #include "ScanClassesMode.hpp"

//#include "Heap.hpp"
#include "HeapRegionIterator.hpp"
#include "ObjectHeapIteratorAddressOrderedList.hpp"

#include "ForwardedHeader.hpp"
#include "GlobalCollector.hpp"
#include "MarkingDelegate.hpp"

template<> OMRHeap* GetHeap<OMRHeap>();

/* this is to instantiate this member function with omrobjectptr_t = uintrp_t*,
   which does not exist in libomrgc.a. 
*/
void
MM_MarkingScheme::assertNotForwardedPointer(MM_EnvironmentBase *env, omrobjectptr_t objectPtr)
{
	/* This is an expensive assert - fetching class slot during marking operation, thus invalidating benefits of leaf optimization.
	 * TODO: after some soaking remove it!
	 */
	if (_extensions->isConcurrentScavengerEnabled()) {
		MM_ForwardedHeader forwardHeader(objectPtr);
		omrobjectptr_t forwardPtr = forwardHeader.getNonStrictForwardedObject();
		/* It is ok to encounter a forwarded object during overlapped concurrent scavenger/marking (or even root scanning),
		 * but we must do nothing about it (if in backout, STW global phase will recover them).
		 */
		Assert_GC_true_with_message3(env, ((NULL == forwardPtr) || (!_extensions->getGlobalCollector()->isStwCollectionInProgress() && _extensions->isConcurrentScavengerInProgress())),
			"Encountered object %p forwarded to %p (header %p) while Concurrent Scavenger/Marking not in progress\n", objectPtr, forwardPtr, &forwardHeader);
	}
}

/**
 * Initialization
 */
// MM_CollectorLanguageInterfaceImpl *
// MM_CollectorLanguageInterfaceImpl::newInstance(MM_EnvironmentBase *env)
// {
// 	MM_CollectorLanguageInterfaceImpl *cli = NULL;
// 	OMR_VM *omrVM = env->getOmrVM();
// 	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(omrVM);
// 
// 	cli = (MM_CollectorLanguageInterfaceImpl *)extensions->getForge()->allocate(sizeof(MM_CollectorLanguageInterfaceImpl), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
// 	if (NULL != cli) {
// 		new(cli) MM_CollectorLanguageInterfaceImpl(omrVM);
// 		if (!cli->initialize(omrVM)) {
// 			cli->kill(env);
// 			cli = NULL;
// 		}
// 	}
// 
// 	return cli;
// }

//void
//MM_CollectorLanguageInterfaceImpl::kill(MM_EnvironmentBase *env)
//{
//	OMR_VM *omrVM = env->getOmrVM();
//	tearDown(omrVM);
//	MM_GCExtensionsBase::getExtensions(omrVM)->getForge()->free(this);
//}

//void
//MM_CollectorLanguageInterfaceImpl::tearDown(OMR_VM *omrVM)
//{
//
//}

bool
MM_MarkingDelegate::initialize(MM_EnvironmentBase *env, MM_MarkingScheme *markingScheme)
{
  //  _objectModel = &(env->getExtensions()->objectModel);
  _markingScheme = markingScheme;
  return true;
}

static gc_oop_t
mark_object(gc_oop_t oop)
{
  if (IS_TAGGED(oop)) {
    return oop;
  }
  
  MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(omr_vmthread_getCurrent(GetHeap<OMRHeap>()->getOMRVM()));
  MM_CollectorLanguageInterfaceImpl *cli = (MM_CollectorLanguageInterfaceImpl*)env->getExtensions()->collectorLanguageInterface;

  cli->markObject(env, (omrobjectptr_t)oop);

  return oop;
}

void
MM_MarkingDelegate::scanRoots(MM_EnvironmentBase *env)
{
  if (env->_currentTask->synchronizeGCThreadsAndReleaseSingleThread(env, UNIQUE_ID)) {
    // This walks the globals of the universe, and the interpreter
    GetUniverse()->WalkGlobals(mark_object);
    env->_currentTask->releaseSynchronizedGCThreads(env);
  }
}

//void
//MM_CollectorLanguageInterfaceImpl::flushNonAllocationCaches(MM_EnvironmentBase *env)
//{
//}
//
//OMR_VMThread *
//MM_CollectorLanguageInterfaceImpl::attachVMThread(OMR_VM *omrVM, const char *threadName, uintptr_t reason)
//{
//	OMR_VMThread *omrVMThread = NULL;
//	omr_error_t rc = OMR_ERROR_NONE;
//
//	rc = OMR_Glue_BindCurrentThread(omrVM, threadName, &omrVMThread);
//	if (OMR_ERROR_NONE != rc) {
//		return NULL;
//	}
//	return omrVMThread;
//}
//
//void
//MM_CollectorLanguageInterfaceImpl::detachVMThread(OMR_VM *omrVM, OMR_VMThread *omrVMThread, uintptr_t reason)
//{
//	if (NULL != omrVMThread) {
//		OMR_Glue_UnbindCurrentThread(omrVMThread);
//	}
//}
//
//void
//MM_CollectorLanguageInterfaceImpl::markingScheme_masterSetupForGC(MM_EnvironmentBase *env)
//{
//}
