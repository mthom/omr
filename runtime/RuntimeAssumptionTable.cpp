/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#include <algorithm>
#include <memory.h>
#ifdef J9_PROJECT_SPECIFIC
#include "j9.h"
#endif
#include "control/Recompilation.hpp"
#ifdef J9_PROJECT_SPECIFIC
#include "control/RecompilationInfo.hpp"
#endif
#include "env/PersistentInfo.hpp"
#include "env/jittypes.h"
#include "infra/Monitor.hpp"
#include "infra/CriticalSection.hpp"
#ifdef J9_PROJECT_SPECIFIC
#include "runtime/J9RuntimeAssumptions.hpp"
#include "runtime/RuntimeAssumptions.hpp"
#endif
#include "runtime/OMRRuntimeAssumptions.hpp"

extern TR::Monitor *assumptionTableMutex;

// Must call this during bootstrap on a single thread because it is not MT safe
bool
TR_RuntimeAssumptionTable::init()
   {
   // Set default values for size tables
   // Sizes need to be determined at bootstrap because in this implementation we cannot grow the tables
   // A palliative would be to store a hint in te shared class cache
   size_t sizes[LastAssumptionKind];
   for (int32_t i=0; i < LastAssumptionKind; i++)
      sizes[i] = 251;
   // overrides in case user provided options or we know we are in specific environments
   if (TR::Options::_classExtendRatSize > 0)
      sizes[RuntimeAssumptionOnClassExtend] = TR::Options::_classExtendRatSize;
   else if (TR::Options::sharedClassCache())
      sizes[RuntimeAssumptionOnClassExtend] = 3079; // choices 1543 3079 6151
   if (TR::Options::_methodOverrideRatSize > 0)
      sizes[RuntimeAssumptionOnMethodOverride] = TR::Options::_methodOverrideRatSize;
   if (TR::Options::_classRedefinitionUPICRatSize > 0)
      sizes[RuntimeAssumptionOnClassRedefinitionUPIC] = TR::Options::_classRedefinitionUPICRatSize;
   else if (TR::Options::getCmdLineOptions()->getOption(TR_EnableHCR))
      sizes[RuntimeAssumptionOnClassRedefinitionUPIC] = 1543;

   for (int i=0; i < LastAssumptionKind; i++)
      {
      assumptionCount[i] = 0;
      reclaimedAssumptionCount[i] = 0;
      _tables[i]._spineArraySize = sizes[i];
      size_t storageSize = sizeof(OMR::RuntimeAssumption*)*_tables[i]._spineArraySize;
      _tables[i]._htSpineArray = (OMR::RuntimeAssumption**)TR_PersistentMemory::jitPersistentAlloc(storageSize);
      _tables[i]._markedforDetachCount = (uint32_t*)TR_PersistentMemory::jitPersistentAlloc(sizeof(uint32_t)*_tables[i]._spineArraySize);
      if (!_tables[i]._htSpineArray || !_tables[i]._markedforDetachCount)
         return false;
      memset(_tables[i]._htSpineArray, 0, storageSize);
      memset(_tables[i]._markedforDetachCount, 0, sizeof(uint32_t)*_tables[i]._spineArraySize);
      }
   _marked=0;
   memset(_detachPending, 0, sizeof(bool)*LastAssumptionKind);
   return true;
   }



void
TR_RuntimeAssumptionTable::purgeAssumptionListHead(OMR::RuntimeAssumption *&assumptionList, TR_FrontEnd *fe)
   {
   assumptionList->compensate(fe, 0, 0);

   OMR::RuntimeAssumption *next = assumptionList->getNext();
   printf("Freeing Assumption 0x%p and next assumption is 0x%p \n", assumptionList, next);

   assumptionList->dequeueFromListOfAssumptionsForJittedBody();
   incReclaimedAssumptionCount(assumptionList->getAssumptionKind());

   TR_PersistentMemory::jitPersistentFree(assumptionList);

   assumptionList = next;

   } 

void
TR_RuntimeAssumptionTable::purgeRATArray(TR_FrontEnd *fe, OMR::RuntimeAssumption **array, uint32_t size)
   {
   for (uint32_t index = 0 ; index < size ; index++)
      {
      while (array[index])
         purgeAssumptionListHead(array[index], fe);
      }
   }

void
TR_RuntimeAssumptionTable::purgeRATTable(TR_FrontEnd *fe)
   {
   OMR::CriticalSection purgeRATTable(assumptionTableMutex);
   for (int i=0; i < LastAssumptionKind; i++)
      {
      if (i == RuntimeAssumptionOnRegisterNative)
         continue; // For some reason we don't want to purge this type of assumption
      purgeRATArray(fe, _tables[i]._htSpineArray, _tables[i]._spineArraySize);
      }
   }



void
TR_RuntimeAssumptionTable::addAssumption(OMR::RuntimeAssumption *a, TR_RuntimeAssumptionKind kind, TR_FrontEnd *fe, OMR::RuntimeAssumption **sentinel)
   {
   OMR::CriticalSection addAssumption(assumptionTableMutex);
   a->enqueueInListOfAssumptionsForJittedBody(sentinel);
   // FIXME: how should we deal with memory allocation failures at runtime?

   // TODO: why is the kind needed? Should use a->getAssumptionKind()
   a->setNext(0); // for sanity's sake.  We don't have assumes() in codert.dev
   assumptionCount[kind]++;
   OMR::RuntimeAssumption **headPtr = getBucketPtr(kind, a->hashCode());
   if (*headPtr)
      a->setNext(*headPtr);
   *headPtr = a;

   if (TR::Options::getCmdLineOptions()->getOption(TR_EnableRATPurging))
      {
      //If we are invalidating assumptions, check to see if we should purge the table at this point.
   		if (! (assumptionCount[kind] % 10))
         purgeRATTable(fe);
      }
   }

void
TR_RuntimeAssumptionTable::detachFromRAT(OMR::RuntimeAssumption *assumption)
   {
   OMR::RuntimeAssumption **headPtr = getBucketPtr(assumption->getAssumptionKind(), assumption->hashCode());
   OMR::RuntimeAssumption *cursor = *headPtr;
   OMR::RuntimeAssumption *prev   = NULL;
   while (cursor)
      {
      OMR::RuntimeAssumption *next = cursor->getNext();
      if (cursor == assumption)
         {
         if (prev)
            prev->setNext(assumption->getNext());
         else
            *headPtr = assumption->getNext();
         break;
         }
      prev = cursor;
      cursor = next;
      }
   TR_ASSERT(cursor, "Must find my assumption in rat\n");
   }

/** 
 * Mark an assumption for future detach and reclaiming from the RAT
 * @param assumption The assumption to be marked for removal
 * Once all assumptions are marked a call to reclaimMarkedFromRAT() will free
 * the marked assumptions. The caller must obtain the assumptionTableMutex lock
 * and must remove the assumption from the metadata linked list right after
 * calling this method because the metadata assumption list next pointer is
 * unusable after this method is executed.
 */
void
TR_RuntimeAssumptionTable::markForDetachFromRAT(OMR::RuntimeAssumption *assumption)
   {
   TR_RatHT *hashTable = findAssumptionHashTable(assumption->getAssumptionKind());
   _detachPending[assumption->getAssumptionKind()] = true;
   hashTable->_markedforDetachCount[(assumption->hashCode() % hashTable->_spineArraySize)]++;
   assumption->markForDetach();
   }

/**
 * Traverse the entire RAT detaching and reclaiming all marked assumptions.
 * This assumes that the assumptions have already been detached from the 
 * metadata's linked list. Only RAT 'kinds' that have any marked assumptions
 * will be traversed, and only the hashtable linked-lists that have a non-zero 
 * marked for detach count will be traversed.
 */
void
TR_RuntimeAssumptionTable::reclaimMarkedAssumptionsFromRAT()
   {
   TR_RatHT *hashTable;
   OMR::RuntimeAssumption *cursor, *next, *prev;
   int kind, reclaimed=0;

   assumptionTableMutex->enter();
   for (kind=0; kind < LastAssumptionKind; kind++) // for each table
      {
      if (_detachPending[kind] == true)  // Is there anything to remove from this table?
         {
         hashTable = _tables + kind;
         for (size_t i = 0; i < hashTable->_spineArraySize; ++i) // for each bucket in the table
            {
            // Look for linked list nodes to remove until all marked nodes have been deleted
            for (cursor = hashTable->_htSpineArray[i], prev = NULL; cursor && hashTable->_markedforDetachCount[i] > 0; cursor = next)
               {
               next = cursor->getNext();
               if (cursor->isMarkedForDetach())
                  {
                  if (prev)
                     prev->setNext(next);
                  else
                     {
                     TR_ASSERT( hashTable->_htSpineArray[i] == cursor, "RAT spine head is not cursor!" );
                     hashTable->_htSpineArray[i] = next;
                     }
                  hashTable->_markedforDetachCount[i]--;
                  // Now release the assumption
                  incReclaimedAssumptionCount(kind);
                  cursor->reclaim();
                  cursor->paint(); // RAS
                  TR_PersistentMemory::jitPersistentFree(cursor);
                  reclaimed++; // RAS
                  }
               else
                  {
                  prev = cursor;
                  }
               }
            TR_ASSERT( hashTable->_markedforDetachCount[i]==0, "RAT detach count should be 0!" );
            }
         _detachPending[kind] = false;
         }
      }
   TR_ASSERT( _marked == reclaimed, "Did not reclaim all the marked assumptions!" );
   _marked=0;
   assumptionTableMutex->exit();
   }

/**
 * Mark and detach all of the assumptions in the metadata's circular linked list
 * @param md The metadata for which to mark & detach assumptions from.
 * @param reclaimPrePrologueAssumptions Are all assumptions free to be removed (ClassUnloading vs. recompiled)
 * This only detaches from the metadata's linked list, not a detach from the RAT,
 * to remove from the RAT you need to call reclaimMarkedAssumptionsFromRAT()
 */
void
TR_RuntimeAssumptionTable::markAssumptionsAndDetach(void * md, bool reclaimPrePrologueAssumptions)
   {
#ifdef J9_PROJECT_SPECIFIC
   J9JITExceptionTable *metaData = (J9JITExceptionTable*) md;
   OMR::RuntimeAssumption *sentry = (OMR::RuntimeAssumption*)(metaData->runtimeAssumptionList);
   OMR::RuntimeAssumption *cursor, *next;

   assumptionTableMutex->enter();
   if (sentry)
      {
      TR_ASSERT(sentry->getAssumptionKind() == RuntimeAssumptionSentinel, "First assumption must be the sentinel\n");
      OMR::RuntimeAssumption *notReclaimedList = sentry;
      for ( cursor=sentry->getNextAssumptionForSameJittedBody(); cursor != sentry; cursor=next)
         {
         next = cursor->getNextAssumptionForSameJittedBody();
         if (cursor->isAssumingMethod(metaData, reclaimPrePrologueAssumptions))
            {
            markForDetachFromRAT(cursor); // Mark for deletion
            _marked++;
            }
         else // This could be an assumption to the persistentBodyInfo which is not reclaimed
            {
            #if defined(PROD_WITH_ASSUMES) || defined(DEBUG)
            TR_RuntimeAssumptionKind kind = cursor->getAssumptionKind();
            TR_ASSERT(kind == RuntimeAssumptionOnClassRedefinitionPIC ||
                      kind == RuntimeAssumptionOnClassRedefinitionUPIC || 
                      kind == RuntimeAssumptionOnClassRedefinitionNOP,
               "non redefinition assumption (RA=%p kind=%d key=%p) left after metadata reclamation\n",
               cursor, kind, cursor->getKey());
            #endif
            cursor->setNextAssumptionForSameJittedBody(notReclaimedList);
            notReclaimedList = cursor;
            }
         }

      if (notReclaimedList != sentry) // some entries were not reclaimed
         {
         // Must attach the notReclaimedList to the sentinel
         sentry->setNextAssumptionForSameJittedBody(notReclaimedList);
         }
      else // nothing is kept in this list; free the sentry too
         {
         sentry->paint(); // RAS
         TR_PersistentMemory::jitPersistentFree(sentry);
         metaData->runtimeAssumptionList = NULL;
         }
      }
   assumptionTableMutex->exit();
#endif
   }


// Reclaim all assumptions that are placed in a circular linked list
// Note that the assumption related to persistentMethodInfo must be left intact
// If metaData==NULL we are allowed to reclaim all assumptions (e.g. when compilation fails)
void
TR_RuntimeAssumptionTable::reclaimAssumptions(OMR::RuntimeAssumption **sentinel, void* metaData, bool reclaimPrePrologueAssumptions)
   {
   OMR::CriticalSection reclaimAssumptions(assumptionTableMutex);
   OMR::RuntimeAssumption *sentry = *sentinel;
   if (*sentinel != NULL) // list is not empty
      {
      int32_t numAssumptionsNotReclaimed = 0; // RAS
      TR_ASSERT(sentry->getAssumptionKind() == RuntimeAssumptionSentinel, "First assumption must be the sentinel\n");
      OMR::RuntimeAssumption *notReclaimedList = sentry;
      OMR::RuntimeAssumption *cursor = sentry->getNextAssumptionForSameJittedBody();
      while (cursor != sentry)
         {
         OMR::RuntimeAssumption *next = cursor->getNextAssumptionForSameJittedBody();
         if (!metaData || cursor->isAssumingMethod(metaData, reclaimPrePrologueAssumptions))
            {
            // Must detach the entry from the RAT
            detachFromRAT(cursor);
            incReclaimedAssumptionCount(cursor->getAssumptionKind());
            cursor->reclaim();
            cursor->paint(); // RAS
            TR_PersistentMemory::jitPersistentFree(cursor);
            }
         else // This could be an assumption to the persistentBodyInfo which is not reclaimed
            {
            cursor->setNextAssumptionForSameJittedBody(notReclaimedList);
            notReclaimedList = cursor;
            numAssumptionsNotReclaimed++; // RAS
            }
         cursor = next;
         }
      if (notReclaimedList != sentry) // some entries were not reclaimed
         {
         // Must attach the notReclaimedList to the sentinel
         sentry->setNextAssumptionForSameJittedBody(notReclaimedList);

#ifdef PROD_WITH_ASSUMES
         if (numAssumptionsNotReclaimed > 1)
            {
            cursor = sentry->getNextAssumptionForSameJittedBody();
            while (cursor != sentry)
               {
               TR_RuntimeAssumptionKind kind = cursor->getAssumptionKind();
               if (kind != RuntimeAssumptionOnClassRedefinitionPIC &&
                   kind != RuntimeAssumptionOnClassRedefinitionUPIC &&
                   kind != RuntimeAssumptionOnClassRedefinitionNOP)
                  {
                  fprintf(stderr, "%d assumptions were left after metadata %p assumption reclaiming\n", numAssumptionsNotReclaimed, metaData);
                  fprintf(stderr, "RA=%p kind=%d key=%p assumingPC=%p\n", cursor, cursor->getAssumptionKind(), (void*)(cursor->getKey()), cursor->getFirstAssumingPC());
                  TR_ASSERT(false, "non redefinition assumptions left after metadata reclamation\n");
                  }
               cursor = cursor->getNextAssumptionForSameJittedBody();
               }
            }
#endif
         }
      else // nothing is kept in this list; free the sentry too
         {
         sentry->paint(); // RAS
         TR_PersistentMemory::jitPersistentFree(sentry);
         *sentinel = NULL;
         }
      }
   }

int32_t
TR_RuntimeAssumptionTable::countRatAssumptions()
   {
   int32_t count = 0;
   OMR::CriticalSection countRatAssumptions(assumptionTableMutex);
   for (int k=0; k < LastAssumptionKind; k++) // for each table
      {
      TR_RatHT *hashTable = _tables + k;
      size_t hashTableSize = hashTable->_spineArraySize;
      for (size_t i = 0; i < hashTableSize; ++i) // for each bucket
         {
         for (OMR::RuntimeAssumption *cursor = hashTable->_htSpineArray[i]; cursor; cursor = cursor->getNext()) // for each entry
            count++;
         }
      }
   return count;
   }


void
TR_RuntimeAssumptionTable::reclaimAssumptions(void *md, bool reclaimPrePrologueAssumptions)
   {
#ifdef J9_PROJECT_SPECIFIC
   J9JITExceptionTable *metaData = (J9JITExceptionTable*) md;
   reclaimAssumptions((OMR::RuntimeAssumption**)(&metaData->runtimeAssumptionList), metaData, reclaimPrePrologueAssumptions);

   // HCR: Note that we never reclaim assumptions on the PersistentMethodInfo.
   // First, it's not safe to do so here, since the fact that a method body
   // is being reclaimed does not imply that its PMI is no longer in use.
   // Second, it's not necessary because replaced classes don't get unloaded,
   // and so the PMI structure will never get freed; hence, the assumption is harmless.
#endif
   }

void
TR_RuntimeAssumptionTable::notifyUserAssumptionTrigger(TR_FrontEnd *vm,
                                                       uint32_t assumptionTriggered)
   {
   OMR::CriticalSection notifyUserTriggerEvent(assumptionTableMutex);
   OMR::RuntimeAssumption **headPtr = getBucketPtr(RuntimeAssumptionOnUserTrigger, hashCode((uintptrj_t)assumptionTriggered));
   TR::PatchNOPedGuardSiteOnUserTrigger *cursor = (TR::PatchNOPedGuardSiteOnUserTrigger *)(*headPtr);
   TR::PatchNOPedGuardSiteOnUserTrigger *prev   = 0;
   while (cursor)
      {
      TR::PatchNOPedGuardSiteOnUserTrigger *next = (TR::PatchNOPedGuardSiteOnUserTrigger*)cursor->getNext();
      cursor->compensate(vm, 0, 0);
      prev = cursor;
      cursor = next;
      }
   }

void
TR_RuntimeAssumptionTable::notifyClassUnloadEvent(TR_FrontEnd *vm, bool isSMP,
                                                  TR_OpaqueClassBlock *assumingClass,
                                                  TR_OpaqueClassBlock *unloadedClass)
   {
   OMR::CriticalSection notifyClassUnloadEvent(assumptionTableMutex);
   OMR::RuntimeAssumption **headPtr = getBucketPtr(RuntimeAssumptionOnClassUnload, hashCode((uintptrj_t)assumingClass));
   TR_UnloadedClassPicSite *cursor = (TR_UnloadedClassPicSite*)(*headPtr);
   TR_UnloadedClassPicSite *prev   = 0;
   while (cursor)
      {
      TR_UnloadedClassPicSite *next = (TR_UnloadedClassPicSite*)cursor->getNext();

      if (cursor->matches((uintptrj_t)assumingClass) &&
          ((unloadedClass == assumingClass) ||
           (cursor->getPicEntry() == (uintptrj_t)unloadedClass) ))
         {
         cursor->compensate(vm, 0, 0);
         if (assumingClass == unloadedClass)
            {
            // before deleting the assumption, let's take it out from the list hung off persistentJittedBodyInfo
            // TODO: replace these statements with CHTable::removeAssumptionFromList(OMR::RuntimeAssumption *&list, OMR::RuntimeAssumption *assumption, OMR::RuntimeAssumption *prev);
            cursor->dequeueFromListOfAssumptionsForJittedBody();
            incReclaimedAssumptionCount(cursor->getAssumptionKind());
            cursor->paint(); // RAS

            TR_PersistentMemory::jitPersistentFree(cursor);
            if (prev)
               prev->setNext(next);
            else
               *headPtr = next;
            cursor = next;
            continue;
            }
         }
      prev = cursor;
      cursor = next;
      }
   }

void
TR_UnloadedClassPicSite::compensate(TR_FrontEnd *, bool isSMP, void *)
   {
#if (defined(TR_HOST_X86) || defined(TR_HOST_S390))
   if (_size == 4)
      {
      *(int32_t *)_picLocation = -1;
#if (defined(TR_HOST_64BIT) && defined(TR_HOST_S390))
      //Check if LARL followed by IIHL/IIHH
      {
      int8_t * cursor = (int8_t *)_picLocation - 2;
      if (*(cursor+6)== (int8_t)0xA5)
         {
         int8_t opcode_byte = *(cursor+7) & 0x0F;
         if (opcode_byte == 0 || opcode_byte == 1)
            {
            if (*cursor == (int8_t)0xC0)
               {
               int8_t larlreg = *(cursor+1) & 0xF0;
               int8_t immreg = *(cursor+7) & 0xF0;
               if (larlreg == immreg)
                  {
                  //patch IIHL/IIHH with LGHI Rx,1
                  *(int32_t *)(cursor+6) = 0xA7090001 ;
                  *(int8_t *)(cursor+7) |= larlreg ;
                  }
               }
            }

         }
      }
#endif
      }
   else
      {
      *(int64_t *)_picLocation = -1;
      }
#elif defined(TR_HOST_POWER)
   // On PPC, the patching is on a 4-byte entity regardless of 32/64bit JIT
   extern void ppcCodeSync(unsigned char *codeStart, unsigned int codeSize);
   *((int32_t *)_picLocation) |= 1;
   ppcCodeSync(_picLocation, 4);
#elif defined(TR_HOST_ARM)
   extern void armCodeSync(unsigned char *codeStart, unsigned int codeSize);
   uint32_t value = *(uint32_t *)_picLocation;
   // On ARM, we patch the last instruction to load constant to mov rX, #1
   value &= 0xf010f000;
   value |= 0x03a00001;
   *((uint32_t *)_picLocation) = value;
   armCodeSync(_picLocation, 4);
#else
   //   TR_ASSERT(0, "unloaded class PIC patching is not implemented on this platform yet");
#endif
   //printf("---###--- unloaded class PIC location %p\n", _picLocation);
   //fprintf(stderr, "---###--- unloaded class PIC\n");
   }

void
TR::PatchNOPedGuardSite::dumpInfo()
   {
   OMR::RuntimeAssumption::dumpInfo("TR::PatchNOPedGuardSite");
   TR_VerboseLog::write(" location=%p destination=%p", _location, _destination);
   }

void
TR::PatchNOPedGuardSiteOnUserTrigger::dumpInfo()
   {
   OMR::RuntimeAssumption::dumpInfo("TR::PatchNOPedGuardSiteForUserTrigger");
   TR_VerboseLog::write(" assumption=%d, picLocation=%p dest=%p", getAssumptionID(), getLocation(), getDestination() );
   }

void
TR_UnloadedClassPicSite::dumpInfo()
   {
   OMR::RuntimeAssumption::dumpInfo("TR_UnloadedClassPicSite");
   TR_VerboseLog::write(" picLocation=%p size=%d", _picLocation, _size );
   }

TR_UnloadedClassPicSite *
TR_UnloadedClassPicSite::make(
   TR_FrontEnd *fe, TR_PersistentMemory * pm, uintptrj_t key, uint8_t *picLocation, uint32_t size,
   TR_RuntimeAssumptionKind kind,  OMR::RuntimeAssumption **sentinel)
   {
   TR_UnloadedClassPicSite *result = new (pm) TR_UnloadedClassPicSite(pm, key, picLocation, size);
   result->addToRAT(pm, RuntimeAssumptionOnClassUnload, fe, sentinel);
   return result;
   }

#ifdef J9_PROJECT_SPECIFIC
TR_RedefinedClassRPicSite *
R_RedefinedClassRPicSite::make(
   TR_FrontEnd *fe, TR_PersistentMemory * pm, uintptrj_t key, uint8_t *picLocation, uint32_t size,
   OMR::RuntimeAssumption **sentinel)
   {
   TR_RedefinedClassRPicSite *result = new (pm) TR_RedefinedClassRPicSite(pm, key, picLocation, size);
   result->addToRAT(pm, RuntimeAssumptionOnClassRedefinitionPIC, fe, sentinel);
   // FAR:: we should replace RuntimeAssumptionOnClassRedefinitionPIC by result->getAssumptionKind();
   return result;
   }

TR_RedefinedClassUPicSite *
TR_RedefinedClassUPicSite::make(
   TR_FrontEnd *fe, TR_PersistentMemory * pm, uintptrj_t key, uint8_t *picLocation, uint32_t size,
   OMR::RuntimeAssumption **sentinel)
   {
   TR_RedefinedClassUPicSite *result = new (pm) TR_RedefinedClassUPicSite(pm, key, picLocation, size);
   result->addToRAT(pm, RuntimeAssumptionOnClassRedefinitionUPIC, fe, sentinel);
   return result;
   } 

void
TR_RedefinedClassPicSite::compensate(TR_FrontEnd *, bool isSMP, void *newKey)
   {
#if (defined(TR_HOST_X86) || defined(TR_HOST_S390))
   if (_size == 4)
      {
      *(int32_t *)_picLocation = (uintptrj_t)newKey;
#if (defined(TR_HOST_64BIT) && defined(TR_HOST_S390))
      //Check if LARL followed by IIHL/IIHH
      {
      int8_t * cursor = (int8_t *)_picLocation - 2;
      if (*(cursor+6)== (int8_t)0xA5)
         {
         int8_t opcode_byte = *(cursor+7) & 0x0F;
         if (opcode_byte == 0 || opcode_byte == 1)
            {
            if (*cursor == (int8_t)0xC0)
               {
               int8_t larlreg = *(cursor+1) & 0xF0;
               int8_t immreg = *(cursor+7) & 0xF0;
               if (larlreg == immreg)
                  {
                  //patch IIHL/IIHH with LGHI Rx,1
                  *(int32_t *)(cursor+6) = 0xA7090001 ;
                  *(int8_t *)(cursor+7) |= larlreg ;
                  }
               }
            }

         }
      }
#endif
      }
   else
      {
      *(int64_t *)_picLocation = (uintptrj_t)newKey;
      }
#elif defined(TR_HOST_POWER)
   extern void ppcCodeSync(unsigned char *codeStart, unsigned int codeSize);
#if (defined(TR_HOST_64BIT))
   *(int64_t *)_picLocation = (uintptrj_t)newKey;
   // we need to flush the pre-load of this address
   // ppcCodeSync(_picLocation, 4);
#else
   *(int32_t *)_picLocation = (uintptrj_t)newKey;
#endif
#elif defined(TR_HOST_ARM)
   *(int32_t *)_picLocation = (uintptrj_t)newKey;
#else
   //   TR_ASSERT(0, "redefined class PIC patching is not implemented on this platform yet");
#endif
   //printf("---###--- redefined class PIC location %p\n", _picLocation);
   //fprintf(stderr, "---###--- redefined class PIC\n");
   }

void
TR_RedefinedClassPicSite::dumpInfo()
   {
   OMR::RuntimeAssumption::dumpInfo("TR_RedefinedClassPicSite");
   TR_VerboseLog::write(" picLocation=%p size=%d", _picLocation, _size );
   }

void
TR_RuntimeAssumptionTable::notifyClassRedefinitionEvent(TR_FrontEnd *vm, bool isSMP,
                                                        void *oldKey,
                                                        void *newKey)
   {
   OMR::CriticalSection notifyClassRedefinitionEvent(assumptionTableMutex);
   bool reportDetails = TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseRuntimeAssumptions);

   OMR::RuntimeAssumption **oldHeadPtr = getBucketPtr(RuntimeAssumptionOnClassRedefinitionPIC,  hashCode((uintptrj_t)oldKey));
   OMR::RuntimeAssumption **newHeadPtr = getBucketPtr(RuntimeAssumptionOnClassRedefinitionPIC,  hashCode((uintptrj_t)newKey));

   TR_RedefinedClassPicSite *pic_cursor = (TR_RedefinedClassPicSite*)(*oldHeadPtr);
   TR_RedefinedClassPicSite *predecessor = NULL;

   OMR::RuntimeAssumption **raArray = findAssumptionHashTable(RuntimeAssumptionOnClassRedefinitionPIC)->_htSpineArray;

   if (reportDetails)
      {
      TR_VerboseLog::vlogAcquire();
      TR_VerboseLog::writeLine(TR_Vlog_RA,"Scanning for PIC assumptions for %p in array %p bucket %p", oldKey, raArray, oldHeadPtr);
      if (!pic_cursor)
         TR_VerboseLog::writeLine(TR_Vlog_RA,"oldKey %p not registered with PIC!", oldKey);
      TR_VerboseLog::vlogRelease();
      }

   while (pic_cursor)
      {
      TR_RedefinedClassPicSite *pic_next = (TR_RedefinedClassPicSite*)pic_cursor->getNext();
      if (reportDetails)
         TR_VerboseLog::writeLineLocked(TR_Vlog_RA, "old=%p @ %p", pic_cursor->getKey(), pic_cursor->getPicLocation());
      if (pic_cursor->matches((uintptrj_t)oldKey))
         {
         if (reportDetails)
            {
            TR_VerboseLog::vlogAcquire();
            TR_VerboseLog::write(" compensating new=%p (array %p bucket %p)", newKey, raArray, newHeadPtr);
            TR_VerboseLog::vlogRelease();
            }

         pic_cursor->compensate(vm, 0, newKey);

         // HCR update the assumption
         pic_cursor->setKey((uintptrj_t)newKey);
         if (oldHeadPtr == newHeadPtr)
            {
            predecessor = pic_cursor;
            }
         else
            {
            // New key has a new hash code, so move it to the appropriate bucket
            if (predecessor)
               predecessor->setNext(pic_next);
            else
               *oldHeadPtr = pic_next;
            pic_cursor->setNext(*newHeadPtr);
            *newHeadPtr = pic_cursor;
            }
         }
      else
         {
         predecessor = pic_cursor;
         }
      pic_cursor = pic_next;
      }
   oldHeadPtr = getBucketPtr(RuntimeAssumptionOnClassRedefinitionNOP, hashCode((uintptrj_t)oldKey));
   OMR::RuntimeAssumption* nop_cursor = *oldHeadPtr;
   OMR::RuntimeAssumption* nop_prev = NULL;

   raArray = findAssumptionHashTable(RuntimeAssumptionOnClassRedefinitionNOP)->_htSpineArray;
   if (reportDetails)
      {
      TR_VerboseLog::vlogAcquire();
      TR_VerboseLog::writeLine(TR_Vlog_RA,"Scanning for NOP assumptions for %p in array %p bucket %p", oldKey, raArray, oldHeadPtr);
      if (!nop_cursor)
         TR_VerboseLog::writeLine(TR_Vlog_RA,"oldKey %p not registered with NOP!", oldKey);
      TR_VerboseLog::vlogRelease();
      }

   while (nop_cursor)
      {
      OMR::RuntimeAssumption* nop_next = nop_cursor->getNext();
      if (reportDetails)
         TR_VerboseLog::writeLine(TR_Vlog_RA, "old=%p @ %p", nop_cursor->getKey(), nop_cursor->getFirstAssumingPC());
      if (nop_cursor->matches((uintptrj_t)oldKey))
         {
         if (reportDetails)
            {
            TR_VerboseLog::vlogAcquire();
            TR_VerboseLog::write(" compensating new=%p", newKey);
            TR_VerboseLog::vlogRelease();
            }
         nop_cursor->compensate(vm, 0, 0);
         nop_cursor->dequeueFromListOfAssumptionsForJittedBody();
         incReclaimedAssumptionCount(nop_cursor->getAssumptionKind());
         nop_cursor->reclaim();
         nop_cursor->paint(); // RAS

         // HCR remove the assumption
         TR_PersistentMemory::jitPersistentFree(nop_cursor);
         if (nop_prev)
            nop_prev->setNext(nop_next);
         else
            *oldHeadPtr = nop_next;
         nop_cursor = nop_next;
         continue;
         }
      nop_prev = nop_cursor;
      nop_cursor = nop_next;
      }

   if (reportDetails)
      TR_VerboseLog::writeLineLocked(TR_Vlog_RA, "Scanning for unresolved PIC assumptions");

   uintptrj_t resolvedKey;
   uintptrj_t initialKey;

   TR_RatHT *classRedefinitionUPICAssumptionTable = findAssumptionHashTable(RuntimeAssumptionOnClassRedefinitionUPIC);
   for (size_t index = 0; index < classRedefinitionUPICAssumptionTable->_spineArraySize; index++)
      {
      pic_cursor = (TR_RedefinedClassPicSite*)classRedefinitionUPICAssumptionTable->_htSpineArray[index];
      while (pic_cursor)
         {
         TR_RedefinedClassPicSite *pic_next = (TR_RedefinedClassPicSite*)pic_cursor->getNext();
         if (!pic_cursor->isForAddressMaterializationSequence())
            {
            resolvedKey = *(uintptrj_t *)(pic_cursor->getPicLocation());
            initialKey = pic_cursor->getKey();
            if ((uintptrj_t)oldKey == resolvedKey)
               {
               if (reportDetails)
                  TR_VerboseLog::writeLineLocked(TR_Vlog_RA, "old=%p resolved=%p @ %p patching new=%p", initialKey, resolvedKey, pic_cursor->getPicLocation(), *(uintptrj_t *)(pic_cursor->getPicLocation()));
               *(uintptrj_t *)(pic_cursor->getPicLocation()) = (uintptrj_t)newKey;
               }
            }
         pic_cursor = pic_next;
         }
      }

#if defined(TR_HOST_X86)
   if (reportDetails)
      TR_VerboseLog::writeLineLocked(TR_Vlog_RA, "Scanning for unresolved PIC address materialization assumptions");

   for (size_t index = 0; index < classRedefinitionUPICAssumptionTable->_spineArraySize; index++)
      {
      pic_cursor = (TR_RedefinedClassPicSite*)classRedefinitionUPICAssumptionTable->_htSpineArray[index];
      while (pic_cursor)
         {
         TR_RedefinedClassPicSite *pic_next = (TR_RedefinedClassPicSite*)pic_cursor->getNext();
         if (pic_cursor->isForAddressMaterializationSequence())
            {
            uint8_t *location    = pic_cursor->getPicLocation();
#if defined(TR_HOST_64BIT)
            bool isAddressMaterialization =
                  (location[0] & 0xf0) == 0x40  // REX
               && (location[1] & 0xf8) == 0xb8; // MOV
            uintptrj_t *immLocation = (uintptrj_t*)(location+2);
#else
            bool isAddressMaterialization =
                  (location[0] & 0xf8) == 0xb8; // MOV
            uintptrj_t *immLocation = (uintptrj_t*)(location+1);
#endif
            // Make sure it really is a materialization sequence (ie. mov reg, Imm) before patching it
            //
            if (isAddressMaterialization){
               resolvedKey = *immLocation;
               initialKey = pic_cursor->getKey();
               if ((uintptrj_t)oldKey == resolvedKey)
                  {
                  if (reportDetails)
                     TR_VerboseLog::writeLineLocked(TR_Vlog_RA, "old=%p resolved=%p @ %p+2 patching new=%p", initialKey, resolvedKey, location, newKey);
                  *immLocation = (uintptrj_t)newKey;
                  }
               }
            }
         pic_cursor = pic_next;
         }
      }

#endif

#if defined(TR_HOST_POWER)
   if (reportDetails)
      TR_VerboseLog::writeLineLocked(TR_Vlog_RA, "Scanning for unresolved PIC address materialization assumptions");

   uint32_t resolvedKey1, resolvedKey2, resolvedKey3, resolvedKey4, tempKey5;
   uint32_t oldKey1, oldKey2, oldKey3, oldKey4;
#if defined(TR_HOST_64BIT)
   oldKey1 = (uint32_t)((uintptrj_t)oldKey >> 48);
   oldKey2 = (uint32_t)((uintptrj_t)oldKey >> 32 & 0xffff);
   oldKey3 = (uint32_t)((uintptrj_t)oldKey >> 16 & 0xffff);
   oldKey4 = (uint32_t)((uintptrj_t)oldKey & 0xffff);
#else
   oldKey1 = HI_VALUE((uintptrj_t)oldKey);
   oldKey2 = LO_VALUE((uintptrj_t)oldKey);
#endif
   for (size_t index = 0; index < classRedefinitionUPICAssumptionTable->_spineArraySize; index++)
      {
      pic_cursor = (TR_RedefinedClassPicSite*)classRedefinitionUPICAssumptionTable->_htSpineArray[index];
      while (pic_cursor)
         {
         TR_RedefinedClassPicSite *pic_next = (TR_RedefinedClassPicSite*)pic_cursor->getNext();
         if (pic_cursor->isForAddressMaterializationSequence())
            {
            resolvedKey1 = *(uint32_t *)(pic_cursor->getPicLocation());
            resolvedKey2 = *(uint32_t *)(pic_cursor->getPicLocation()+4);

#if defined(TR_HOST_64BIT)
            resolvedKey3 = *(uint32_t *)(pic_cursor->getPicLocation()+12);
            resolvedKey4 = *(uint32_t *)(pic_cursor->getPicLocation()+16);

#endif
            initialKey = pic_cursor->getKey();
#if defined(TR_HOST_64BIT)
            if (oldKey4 == (resolvedKey4 & 0xffff) && oldKey3 ==(resolvedKey3 & 0xffff) &&
               oldKey2 == (resolvedKey2 & 0xffff) && oldKey1 == (resolvedKey1 & 0xffff))
               {
               // Make sure it really is a materialization sequence
               // lis
               // ori
               // rldicr
               // oris
               // ori
               tempKey5 = *(uint32_t *)(pic_cursor->getPicLocation()+8);
               bool isAddressMaterialization =
                       ((resolvedKey1 >> 26) == 0x0f)  //lis
                     &&((resolvedKey2 >> 26) == 0x18)  //ori
                     &&((tempKey5 >> 26) == 0x1e)  //rldicr
                     &&((resolvedKey3 >> 26) == 0x19)  //oris
                     &&((resolvedKey4 >> 26) == 0x18);     //ori

               if (isAddressMaterialization)
				  {
            	  if (reportDetails)
            		  TR_VerboseLog::writeLineLocked(TR_Vlog_RA, "o=%p @ %p  r=%p %p %p %p",
                                                         initialKey, pic_cursor->getPicLocation(),
                                                         resolvedKey1, resolvedKey2, resolvedKey3, resolvedKey4);
                  *(uint32_t *)(pic_cursor->getPicLocation()) = resolvedKey1 & 0xffff0000 | (uint32_t)((uintptrj_t)newKey >> 48);;
                  *(uint32_t *)(pic_cursor->getPicLocation()+4) = resolvedKey2 & 0xffff0000 | (uint32_t)((uintptrj_t)newKey >> 32 & 0xffff);;
                  *(uint32_t *)(pic_cursor->getPicLocation()+12) = resolvedKey3 & 0xffff0000 | (uint32_t)((uintptrj_t)newKey >> 16 & 0xffff);;
                  *(uint32_t *)(pic_cursor->getPicLocation()+16) = resolvedKey4 & 0xffff0000 | (uint32_t)((uintptrj_t)newKey & 0xffff);;
                  if (reportDetails)
                     {
                     TR_VerboseLog::vlogAcquire();
                     TR_VerboseLog::write(" patched n=%p %p %p %p",
                     *(uint32_t *)(pic_cursor->getPicLocation()),
                     *(uint32_t *)(pic_cursor->getPicLocation()+4),
                     *(uint32_t *)(pic_cursor->getPicLocation()+12),
                     *(uint32_t *)(pic_cursor->getPicLocation()+16));
                     TR_VerboseLog::vlogRelease();
                     }
                  }
               }
#else
            if (oldKey2 == (resolvedKey2 & 0xffff) && oldKey1 == (resolvedKey1 & 0xffff))
               {

               // Make sure it really is a materialization sequence
               // lis
               // addi
               bool isAddressMaterialization =
                    ((resolvedKey1 >> 26) == 0x0f)  //lis
                  &&((resolvedKey2 >> 26) == 0x0e); //addi
               if (isAddressMaterialization)
                  {
            	  if (reportDetails)
                     TR_VerboseLog::writeLineLocked(TR_Vlog_RA, "o=%p r=%p %p @ %p", initialKey, resolvedKey1, resolvedKey2, pic_cursor->getPicLocation());
                  *(uint32_t *)(pic_cursor->getPicLocation())   = resolvedKey1 & 0xffff0000 | HI_VALUE((uint32_t)newKey);
                  *(uint32_t *)(pic_cursor->getPicLocation()+4) = resolvedKey2 & 0xffff0000 | LO_VALUE((uint32_t)newKey);
                  if (reportDetails)
                     {
                     TR_VerboseLog::vlogAcquire();
                     TR_VerboseLog::write(" patched n=%p %p", *(uint32_t *)(pic_cursor->getPicLocation()), *(uint32_t *)(pic_cursor->getPicLocation()+4));
                     TR_VerboseLog::vlogRelease();
                     }
                  }
               }
#endif
            }
         pic_cursor = pic_next;
         }
      }
#endif

   }

void
TR_RedefinedClassPicSite::compensate(TR_FrontEnd *, bool isSMP, void *newKey)
   {
#if (defined(TR_HOST_X86) || defined(TR_HOST_S390))
   if (_size == 4)
      {
      *(int32_t *)_picLocation = (uintptrj_t)newKey;
#if (defined(TR_HOST_64BIT) && defined(TR_HOST_S390))
      //Check if LARL followed by IIHL/IIHH
      {
      int8_t * cursor = (int8_t *)_picLocation - 2;
      if (*(cursor+6)== (int8_t)0xA5)
         {
         int8_t opcode_byte = *(cursor+7) & 0x0F;
         if (opcode_byte == 0 || opcode_byte == 1)
            {
            if (*cursor == (int8_t)0xC0)
               {
               int8_t larlreg = *(cursor+1) & 0xF0;
               int8_t immreg = *(cursor+7) & 0xF0;
               if (larlreg == immreg)
                  {
                  //patch IIHL/IIHH with LGHI Rx,1
                  *(int32_t *)(cursor+6) = 0xA7090001 ;
                  *(int8_t *)(cursor+7) |= larlreg ;
                  }
               }
            }

         }
      }
#endif
      }
   else
      {
      *(int64_t *)_picLocation = (uintptrj_t)newKey;
      }
#elif defined(TR_HOST_POWER)
   extern void ppcCodeSync(unsigned char *codeStart, unsigned int codeSize);
#if (defined(TR_HOST_64BIT))
   *(int64_t *)_picLocation = (uintptrj_t)newKey;
   // we need to flush the pre-load of this address
   // ppcCodeSync(_picLocation, 4);
#else
   *(int32_t *)_picLocation = (uintptrj_t)newKey;
#endif
#elif defined(TR_HOST_ARM)
   *(int32_t *)_picLocation = (uintptrj_t)newKey;
#else
   //   TR_ASSERT(0, "redefined class PIC patching is not implemented on this platform yet");
#endif
   //printf("---###--- redefined class PIC location %p\n", _picLocation);
   //fprintf(stderr, "---###--- redefined class PIC\n");
   }

void
TR_RedefinedClassPicSite::dumpInfo()
   {
   OMR::RuntimeAssumption::dumpInfo("TR_RedefinedClassPicSite");
   TR_VerboseLog::write(" picLocation=%p size=%d", _picLocation, _size );
   }

void
TR_RuntimeAssumptionTable::notifyMutableCallSiteChangeEvent(TR_FrontEnd *fe, uintptrj_t cookie)
   {
   OMR::CriticalSection notifyMutableCallSiteChangeEvent(assumptionTableMutex);

   bool reportDetails = TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseRuntimeAssumptions);

   OMR::RuntimeAssumption **headPtr = getBucketPtr(RuntimeAssumptionOnMutableCallSiteChange, hashCode(cookie));
   TR_PatchNOPedGuardSiteOnMutableCallSiteChange *cursor = (TR_PatchNOPedGuardSiteOnMutableCallSiteChange*)(*headPtr);
   TR_PatchNOPedGuardSiteOnMutableCallSiteChange *prev   = 0;
   while (cursor)
      {
      TR_PatchNOPedGuardSiteOnMutableCallSiteChange *next = (TR_PatchNOPedGuardSiteOnMutableCallSiteChange*)cursor->getNext();

      if (cursor->matches(cookie))
         {
         if (reportDetails)
            {
            TR_VerboseLog::vlogAcquire();
            TR_VerboseLog::writeLine(TR_Vlog_RA,"compensating cookie " UINT64_PRINTF_FORMAT_HEX " ", cookie);
            cursor->dumpInfo();
            TR_VerboseLog::vlogRelease();
            }
         cursor->compensate(fe, 0, 0);

         // before deleting the assumption, let's take it out from the list hung off persistentJittedBodyInfo
         // TODO: replace these statements with CHTable::removeAssumptionFromList(OMR::RuntimeAssumption *&list, OMR::RuntimeAssumption *assumption, OMR::RuntimeAssumption *prev);
         cursor->dequeueFromListOfAssumptionsForJittedBody();
         incReclaimedAssumptionCount(cursor->getAssumptionKind());
         cursor->paint(); // RAS

         TR_PersistentMemory::jitPersistentFree(cursor);
         if (prev)
            prev->setNext(next);
         else
            *headPtr = next;
         }
      else
         {
         prev = cursor;
         }
      cursor = next;
      }
   }
#endif

void
OMR::RuntimeAssumption::addToRAT(TR_PersistentMemory * persistentMemory,
                                 TR_RuntimeAssumptionKind kind,
                                 TR_FrontEnd *fe,
                                 OMR::RuntimeAssumption **sentinel)
   {
   persistentMemory->getPersistentInfo()->getRuntimeAssumptionTable()->addAssumption(this, kind, fe, sentinel);
   bool reportDetails = TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseRuntimeAssumptions);
   if (reportDetails)
      {
      TR_VerboseLog::vlogAcquire();
      TR_VerboseLog::writeLine(TR_Vlog_RA,"Adding %s assumption: ", runtimeAssumptionKindNames[kind] );
      dumpInfo();
      TR_VerboseLog::vlogRelease();
      }
   }

void
OMR::RuntimeAssumption::dumpInfo(char *subclassName)
   {
   TR_VerboseLog::write("%s@%p: key=%p", subclassName, this, _key);
   }

bool
OMR::RuntimeAssumption::isAssumingRange(uintptrj_t rangeStartPC,
                                        uintptrj_t rangeEndPC,
                                        uintptrj_t rangeColdStartPC,
                                        uintptrj_t rangeColdEndPC,
                                        uintptrj_t rangeStartMD,
                                        uintptrj_t rangeEndMD)
   {
   return (assumptionInRange(rangeStartPC, rangeEndPC) ||
           (rangeColdStartPC && assumptionInRange(rangeColdStartPC, rangeColdEndPC)) ||
           (rangeStartMD && assumptionInRange(rangeStartMD, rangeEndMD)));
   }

bool OMR::RuntimeAssumption::isAssumingMethod(void *md, bool reclaimPrePrologueAssumptions)
   {
#ifdef J9_PROJECT_SPECIFIC
   J9JITExceptionTable *metaData = (J9JITExceptionTable*) md;

   // MetaData->startPC refers to the interpreter entry point, and does not include pre-prologue.
   // If we need to reclaim pre-prologue assumptions (i.e. on class unloading), we need to scan
   // from the beginning of the code cache allocation of the method (metaData->codeCacheAlloc).
   uintptrj_t metaStartPC = (reclaimPrePrologueAssumptions)?metaData->codeCacheAlloc:metaData->startPC;
   if (assumptionInRange(metaStartPC, metaData->endWarmPC) ||
           (metaData->startColdPC && assumptionInRange(metaData->startColdPC, metaData->endPC)))
      return true;

   if (assumptionInRange((uintptrj_t)metaData, ((uintptrj_t)metaData) + ((uintptrj_t)metaData->size)))
      {
      TR_PersistentJittedBodyInfo *bodyInfo = (TR_PersistentJittedBodyInfo *)metaData->bodyInfo;
      if (bodyInfo && bodyInfo->getMethodInfo() && bodyInfo->getMethodInfo()->isInDataCache())
         {
         // Don't reclaim if address is in the persistent body/method info in the data cache
         if (!assumptionInRange((uintptrj_t)metaData->bodyInfo, ((uintptrj_t) metaData->bodyInfo) + (uintptrj_t)sizeof(TR_PersistentJittedBodyInfo) + (uintptrj_t)sizeof(TR_PersistentMethodInfo)))
            return true;
         }
      else
         {
         return true;
         }
      }
#endif

   return false;
   }


// Currrent assumption must be taken out from the circular list of assumptions
// starting in the jittedBodyInfo. We start from the current assumption and go around
// until we find it again. This way we can find previous entry so we can detach it
// from the list
// must be executed under assumptionTableMutex
void
OMR::RuntimeAssumption::dequeueFromListOfAssumptionsForJittedBody()
   {
   // We should not try to detach TR::SentinelRuntimeAssumption
   TR_ASSERT(getAssumptionKind() != RuntimeAssumptionSentinel, "Sentinel assumptions cannot be detached");
   OMR::RuntimeAssumption *crt = this->getNextAssumptionForSameJittedBody();
   OMR::RuntimeAssumption *prev = this;
   TR_ASSERT(crt, "Assumption must be queued when trying to detach");
   while (crt != this)
      {
      // any such assumption (except the sentinel) must have an assumption location belonging to this body
      prev = crt;
      crt = crt->getNextAssumptionForSameJittedBody();
      }
   // Now I completed a full circle
   prev->setNextAssumptionForSameJittedBody(crt->getNextAssumptionForSameJittedBody());
   crt->setNextAssumptionForSameJittedBody(NULL); // assumption no longer in the circular list
   /* need a frontend */
   if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseRuntimeAssumptions))
      {
      TR_VerboseLog::vlogAcquire();
      TR_VerboseLog::writeLine(TR_Vlog_RA, "Deleting %s assumption: ", runtimeAssumptionKindNames[getAssumptionKind()] );
      dumpInfo();
      TR_VerboseLog::vlogRelease();
      }
   }

static uint32_t hash(TR_OpaqueClassBlock * h, uint32_t size)
   {
   // 2654435761 is the golden ratio of 2^32.
   //
   return (((uint32_t)(uintptrj_t)h >> 2) * 2654435761u) % size;
   }
