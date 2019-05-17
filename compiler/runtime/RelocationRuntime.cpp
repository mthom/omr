/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "compiler/runtime/RelocationRuntime.hpp"
#include "compiler/runtime/RelocationTarget.hpp"

TR::RelocationRuntime::RelocationRuntime(TR::JitConfig *jitCfg):OMR::RelocationRuntimeConnector(jitCfg)
   {
   _method = NULL;
   _jitConfig = jitCfg;
   _trMemory = NULL;
   _options = NULL;


   #if defined(TR_HOST_X86)
      #if defined(TR_HOST_64BIT)
      _reloTarget =  (new (PERSISTENT_NEW) TR::RelocationTarget(this));
      #else
      _reloTarget = new (PERSISTENT_NEW) TR_X86RelocationTarget(this);
      #endif
   #elif defined(TR_HOST_POWER)
      #if defined(TR_HOST_64BIT)
      _reloTarget = new (PERSISTENT_NEW) TR_PPC64RelocationTarget(this);
      #else
      _reloTarget = new (PERSISTENT_NEW) TR_PPC32RelocationTarget(this);
      #endif
   #elif defined(TR_HOST_S390)
      _reloTarget = new (PERSISTENT_NEW) TR_S390RelocationTarget(this);
   #elif defined(TR_HOST_ARM)
      _reloTarget = new (PERSISTENT_NEW) TR_ARMRelocationTarget(this);
   #else
      TR_ASSERT(0, "Unsupported relocation target");
   #endif

   if (_reloTarget == NULL)
      {
      // TODO: need error condition here
      return;
      }


      _isLoading = false;

#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
      _numValidations = 0;
      _numFailedValidations = 0;
      _numInlinedMethodRelos = 0;
      _numFailedInlinedMethodRelos = 0;
      _numInlinedAllocRelos = 0;
      _numFailedInlinedAllocRelos = 0;
#endif
   }


TR::SharedCacheRelocationRuntime::SharedCacheRelocationRuntime(TR::JitConfig *jitCfg)
   :OMR::SharedCacheRelocationRuntimeConnector(jitCfg){}