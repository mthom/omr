/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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


/**
 * This file contains code samples for using guards
 */

#include <iostream>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

#include "JitBuilder.hpp"
#include "TypeDictionary.hpp"
#include "MethodBuilder.hpp"

#include "Guards.hpp"

using std::cout;
using std::cerr;

GuardMethod::GuardMethod(OMR::JitBuilder::TypeDictionary* types)
   : OMR::JitBuilder::MethodBuilder(types),
     _assumption(-1)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("test_guards");
   DefineParameter("x", Int32);
   DefineParameter("y", Int32);
   DefineReturnType(Int32);
   }

/*
 * Method basically shows how a nopped guard can be used
 * to make a runtime assumption that can later be patched
 * if/when the assumption is violated, causing recovery
 * code to execute rather than the guarded path.
 */

bool
GuardMethod::buildIL()
   {
   OMR::JitBuilder::IlBuilder *guardedPath = NULL;
   OMR::JitBuilder::IlBuilder *recoveryPath = NULL;

   _assumption = NOPGuard(&guardedPath, &recoveryPath);

   guardedPath->Return(
   guardedPath->   ConstInt32(42));

   recoveryPath->Return(
   recoveryPath->   Sub(
   recoveryPath->      Load("x"),
   recoveryPath->      Load("y")));

   return true;
   }

int
main(int argc, char *argv[])
   {
   cout << "Step 1: initialize JIT\n";
   bool initialized = initializeJit();
   if (!initialized)
      {
      cerr << "FAIL: could not initialize JIT\n";
      exit(-1);
      }

   cout << "Step 2: define type dictionary\n";
   OMR::JitBuilder::TypeDictionary types;

   cout << "Step 3: compile method builder\n";
   GuardMethod mb(&types);
   void *entry = 0;
   int32_t rc = (*compileMethodBuilder)(&mb, &entry);
   if (rc != 0)
      {
      cerr << "FAIL: compilation error " << rc << "\n";
      exit(-2);
      }

   cout << "Step 4: invoke compiled code\n";
   GuardMethodFunction *func = (GuardMethodFunction *) entry;
   int32_t x, y, total, pass;

   total=0; pass=0;

   x=0;     y=0;    total++; if (func(x, y) == 42)    pass++;
   x=10;    y=0;    total++; if (func(x, y) == 42)    pass++;
   x=0;     y=10;   total++; if (func(x, y) == 42)    pass++;
   x=500;   y=499;  total++; if (func(x, y) == 42)    pass++;

   invalidateJitAssumption(mb.assumption());   

   x=30;    y=10;   total++; if (func(x, y) == (x-y)) pass++;
   x=10;    y=0;    total++; if (func(x, y) == (x-y)) pass++;
   x=100;   y=99;   total++; if (func(x, y) == (x-y)) pass++;

   cout << "Step 5: shutdown JIT\n";
   shutdownJit();

   if (pass == total)
      cout << "ALL TESTS PASSED\n";
   else
      cout << pass << " out of " << total << " passed\n";

   return total - pass;
   }
