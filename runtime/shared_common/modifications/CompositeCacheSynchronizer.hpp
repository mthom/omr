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
#if !defined(COMPOSITE_CACHE_SYNCHRONIZER_HPP_INCLUDED)
#define COMPOSITE_CACHE_SYNCHRONIZER_HPP_INCLUDED

class CompositeCacheSynchronizer;

/* yes, the same interface is reproduced, but CompositeCacheSynchronizer
 * is meant to be a visitor.
 */
class CacheLockOwner {
public:
  virtual bool hasCacheMutex(CompositeCacheSynchronizer* synchronizer) {
    return synchronizer->hasCacheMutex(*this);
  }

  virtual bool enterCacheMutex(CompositeCacheSynchronizer* synchronizer) {
    return synchronizer->enterCacheMutex(*this);
  }

  virtual bool exitCacheMutex(CompositeCacheSynchronizer* synchronizer) {
    return synchronizer->exitCacheMutex(*this);
  }
};

/* This class substitutes for the _commonCCInfo of type
 * J9ShrCompositeCacheCommonInfo struct. Users of the shared class can
 * use it to provide their own customized notions of
 * multithreaded-ness, how to manage mutexes and who owns them,
 * etc. It's divorced from concepts such as a VM, pointers to thread
 * objects.. all the trappings of the J9 shared cache have been
 * abstracted out, but could easily be restored.
 *
 * This is another visitor pattern manifestation! What else would it be.
 */
class CompositeCacheSynchronizer
{
public:
  /* CacheLockOwner is expected to wrap both the lock owner (ie. an
   * OMR thread) and whatever sort of identifier refers to a
   * lock. These are both expected to be lightweight objects, and
   * CacheLockOwner objects aren't expected to live beyond the scope
   * of the member function calls, so there should be no reason to
   * persist them.
   *
   * Returns bool to indicate success/failure. If further information
   * is desired, write it to the CacheLockOwner* object.
   */
  virtual bool hasCacheMutex(CacheLockOwner&) = 0;
  virtual bool enterCacheMutex(CacheLockOwner&) = 0;
  virtual bool exitCacheMutex(CacheLockOwner&) = 0;
};
#endif
