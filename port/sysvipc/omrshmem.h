/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#ifndef omrshmem_h
#define omrshmem_h

/* @ddr_namespace: default */
#include <sys/types.h>
#include "omrport.h"

typedef struct omrshmem_handle {
	int32_t shmid;
	char* baseFileName;
	void* regionStart;
	int64_t timestamp;
	int32_t perm;
	uintptr_t size;
	uintptr_t currentStorageProtectKey;
	uintptr_t controlStorageProtectKey;
	uintptr_t flags;
	OMRMemCategory * category;
} omrshmem_handle;




/*
 * The control file format is definied in the below two structures.
 * 'j9shmem_controlBaseFileFormat' is readable from all non zero byte
 * control files generated by java 5,6, and 7.
 * 
 * Note: There are two structures because of CMVC 163844. The fields beyond, 
 * and including, 'int64_t size'can't be read from deprecated/older control files 
 * b/c a padding byte is inserted by newer (not older) compilers before the int64_t 
 * (to ensure it is 8 byte aligned). Specifically this is a problem when 
 * working with caches from 32 bit java5 on linux.
 */
typedef struct omrshmem_controlBaseFileFormat {
	int32_t version;
	int32_t modlevel;
	key_t ftok_key;
	int32_t proj_id;
	int32_t shmid;
} omrshmem_controlBaseFileFormat;

typedef struct omrshmem_controlFileFormat {
	omrshmem_controlBaseFileFormat common;
	int64_t size;
	int32_t uid;
	int32_t gid;
} omrshmem_controlFileFormat;

#endif     /* omrshmem_h */


