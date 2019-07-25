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

#ifndef OMR_AOTADAPTER_INCL
#define OMR_AOTADAPTER_INCL
#include "env/RawAllocator.hpp"
#include "infra/Annotations.hpp"
#ifndef OMR_AOTADAPTER_CONNECTOR
#define OMR_AOTADAPTER_CONNECTOR
namespace OMR { class AotAdapter; }
namespace OMR { typedef OMR::AotAdapter AotAdapterConnector; }
#endif
namespace TR 
    {
    class AotAdapter;
    class SharedCacheRelocationRuntime;
    class SharedCache;
    class CodeCache;
    class CompilerEnv;
    }
namespace OMR
{
class OMR_EXTENSIBLE AotAdapter{
public:
    AotAdapter(){};
    TR::AotAdapter* self();
    void initializeAOTClasses(TR::RawAllocator allocator);
    TR::SharedCache* sc() {return _sharedCache;}
    TR::SharedCacheRelocationRuntime* rr() {return _reloRuntime;}
    TR::CodeCache* cc() {return _codeCache;}
 private:
    TR::SharedCache* _sharedCache;
    TR::SharedCacheRelocationRuntime* _reloRuntime;
    TR::CodeCache*    _codeCache;
    TR::CompilerEnv* _compiler;
};
}
#endif // !defined(OMR_AOTADAPTER_INCL)
