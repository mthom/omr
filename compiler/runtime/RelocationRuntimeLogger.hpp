/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#ifndef RELOCATION_RUNTIME_LOGGER_INCL
#define RELOCATION_RUNTIME_LOGGER_INCL

#include "env/TRMemory.hpp"
#include "runtime/OMRRelocationRuntimeTypes.hpp"
#include "env/JitConfig.hpp" // for JitConfig, it got moved

namespace TR {class RelocationRuntime;}
namespace TR { class Options; }
namespace TR 
{
#define	RELO_LOG(r, n, ...) \
   if (((TR::RelocationRuntimeLogger *)r)->logDetailEnabled((int32_t)n)) \
      {\
      ((TR::RelocationRuntimeLogger *)r)->debug_printf(__VA_ARGS__);\
      }\

class RelocationRuntimeLogger {
   public:
      TR_ALLOC(TR_Memory::Relocation)
      void * operator new(size_t, JitConfig *);
      RelocationRuntimeLogger(RelocationRuntime *relort);

      void setupOptions(TR::Options *options);

      RelocationRuntime *reloRuntime()                  { return _reloRuntime; }

      bool logEnabled()                                    { return _logEnabled; }
      bool logDetailEnabled(int32_t n)                     { return (n <=_logLevel); }

      void debug_printf(char *format, ...);
      void printf(char *format, ...);

      void relocatableDataHeader();
      void relocatableDataFooter();

      void relocationDump();
      void relocationTime();
      void versionMismatchWarning();
      void maxCodeOrDataSizeWarning();

   private:
      void startTag(const char *tag);
      void endTag(const char *tag);

      bool lockLog();
      void unlockLog(bool wasLocked);

      bool verbose()                                       { return _verbose; }


      RelocationRuntime *_reloRuntime;
      int32_t _logLevel;
      bool _logLocked;
      bool _headerWasLocked;
      bool _logEnabled;
      bool _verbose;
 
      UDATA _reloStartTime;
   };
}
#endif   // RELOCATION_RUNTIME_LOGGER_INCL
