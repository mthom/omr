/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#ifndef OMR_RELOCATION_RUNTIME_TYPES
#define OMR_RELOCATION_RUNTIME_TYPES
#include "omrcomp.h"
#include "env/JitConfig.hpp" // for JitConfig, it got moved
typedef struct OMRJITExceptionTable {
//	struct J9ConstantPool* constantPool;
//	struct J9Method* ramMethod;
	UDATA startPC;
	UDATA endWarmPC;
	UDATA startColdPC;
	UDATA endPC;
	UDATA totalFrameSize;
	I_32 size;
	UDATA flags;
	UDATA registerSaveDescription;
	void* gcStackAtlas;
	void* inlinedCalls;
	void* bodyInfo;
	struct  OMRJITExceptionTable* nextMethod;
	struct  OMRJITExceptionTable* prevMethod;
	I_32 hotness;
	UDATA codeCacheAlloc;
}  OMRJITExceptionTable;

typedef struct OMRJITDataCacheHeader {
	U_32 size;
	U_32 type;
} OMRJITDataCacheHeader;

typedef struct OMRMemorySegment {
	uintptr_t type;
	uintptr_t size;
	uint8_t *baseAddress;
	uint8_t *heapBase;
	uint8_t *heapTop;
	uint8_t *heapAlloc;
	struct OMRMemorySegment *nextSegment;
	struct OMRMemorySegment *previousSegment;
	struct OMRMemorySegmentList *memorySegmentList;
	uintptr_t unused1;
	struct OMRClassLoader *classLoader;
	void *memorySpace;
	struct OMRMemorySegment *nextSegmentInClassLoader;
} OMRMemorySegment;

typedef struct OMRMethod {
	U_8* bytecodes;
//	struct J9ConstantPool* constantPool;
	void* methodRunAddress;
	void* extra;
} OMRMethod;

#define OMR_JIT_QUEUED_FOR_COMPILATION -5
#define OMR_JIT_NEVER_TRANSLATE -3
#define OMR_JIT_RESOLVE_FAIL_COMPILE -2
#define OMR_JIT_DCE_EXCEPTION_INFO 0x1
#define OMR_JIT_TRANSITION_METHOD_ENTER 0x1
#define OMR_JIT_TOGGLE_RI_ON_TRANSITION 0x1
#define OMR_JIT_DCE_STACK_ATLAS 0x2
#define OMR_JIT_TRANSITION_METHOD_EXIT 0x2
#define OMR_JIT_TOGGLE_RI_IN_COMPILED_CODE 0x2
#define OMR_JIT_DCE_RELOCATION_DATA 0x4
#define OMR_JIT_DCE_THUNK_MAPPING_LIST 0x8
#define OMR_JIT_DCE_THUNK_MAPPING 0x10
#define OMR_JIT_DCE_HASH_TABLE 0x20
#define OMR_JIT_DCE_MCC_HT_ENTRY 0x40
#define OMR_JIT_DCE_AOT_METHOD_HEADER 0x80
#define OMR_JIT_DCE_UNALLOCATED 0x100
#define OMR_JIT_DCE_IN_USE 0x200
#define OMR_JIT_DCE_AOT_PERSISTENT_INFO 0x400

#endif