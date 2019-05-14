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

#include "runtime/RelocationRuntimeLogger.hpp"
#include "codegen/FrontEnd.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "runtime/RelocationRecord.hpp"
#include "runtime/RelocationRuntime.hpp"
#include "runtime/RelocationTarget.hpp"


// RelocationRuntimeLogger class
static const char *headerTag = "relocatableDataRT";
namespace TR{ 
RelocationRuntimeLogger::RelocationRuntimeLogger(RelocationRuntime *reloRuntime)
   {
   _reloRuntime = reloRuntime;

   _logLocked = false;
   _headerWasLocked = false;
   _reloStartTime = 0;
   setupOptions(reloRuntime->options());
   _verbose = _logEnabled;
   }

void
RelocationRuntimeLogger::setupOptions(TR::Options *options)
   {
   _logEnabled = false;
   _logLevel = 0;
   if (options)
      {
      _logLevel = options->getAotrtDebugLevel();
      //_logLevel = TR::Options::getAOTCmdLineOptions()->getAotrtDebugLevel();
      _logEnabled = (_logLevel > 0)
                    || options->getOption(TR_TraceRelocatableDataRT)
                    || options->getOption(TR_TraceRelocatableDataDetailsRT);
      }
   }

void
RelocationRuntimeLogger::debug_printf(char *format, ...)
   {
   va_list args;
   char outputBuffer[512];

   va_start(args, format);
   va_end(args);
   }

void
RelocationRuntimeLogger::printf(char *format, ...)
   {
   va_list args;
   char outputBuffer[512];


   va_start(args, format);
   va_end(args);
   }

void
RelocationRuntimeLogger::relocatableDataHeader()
   {
   if (logEnabled())
      {
      _headerWasLocked = lockLog();
      startTag(headerTag);
      }
   }

void
RelocationRuntimeLogger::relocatableDataFooter()
   {
   if (logEnabled())
      {
      endTag(headerTag);
      unlockLog(_headerWasLocked);
      }
   }



void
RelocationRuntimeLogger::relocationDump()
   {
   if (verbose())
      {
      if (0)
         {
         UDATA count = 0;
         RelocationRecord *header;
         RelocationRecord *endRecord;
         U_8 *reloRecord;

         bool wasLocked=lockLog();

         // TODO: this code is busTED
         header = (RelocationRecord *)((U_8 *)reloRuntime()->exceptionTableCacheEntry() + reloRuntime()->aotMethodHeaderEntry()->offsetToRelocationDataItems);
         endRecord = (RelocationRecord *) ((U_8 *) header + header->size(reloRuntime()->reloTarget()));

         reloRecord = (U_8 *) header;
         while (reloRecord < (U_8*)endRecord)
            {
            if (count % 16 == 0)
            count++;
         unlockLog(wasLocked);
         }
      }
   }
   }
void
RelocationRuntimeLogger::relocationTime()
   {
 
   }

void
RelocationRuntimeLogger::versionMismatchWarning()
   {

   }

void
RelocationRuntimeLogger::maxCodeOrDataSizeWarning()
   {
   }


// returns true if this call actually locked the log, false if it was already locked
bool
RelocationRuntimeLogger::lockLog()
   {
   if (!_logLocked)
      {
      return true;
      }
   return false;
   }


// Should pass in the value returned by the corresponding call to lockLog
void
RelocationRuntimeLogger::unlockLog(bool shouldUnlock)
   {
   if (shouldUnlock)
      {
      //TR_ASSERT(_logLocked, "Runtime log unlock request but not holding lock");
      _logLocked = false;
      }
   }

void
RelocationRuntimeLogger::startTag(const char *tag)
   {
   }

void
RelocationRuntimeLogger::endTag(const char *tag)
   {
   }
}