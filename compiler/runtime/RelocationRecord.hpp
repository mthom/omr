// /*******************************************************************************
//  * Copyright (c) 2000, 2019 IBM Corp. and others
//  *
//  * This program and the accompanying materials are made available under
//  * the terms of the Eclipse Public License 2.0 which accompanies this
//  * distribution and is available at http://eclipse.org/legal/epl-2.0
//  * or the Apache License, Version 2.0 which accompanies this distribution
//  * and is available at https://www.apache.org/licenses/LICENSE-2.0.
//  *
//  * This Source Code may also be made available under the following Secondary
//  * Licenses when the conditions for such availability set forth in the
//  * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
//  * version 2 with the GNU Classpath Exception [1] and GNU General Public
//  * License, version 2 with the OpenJDK Assembly Exception [2].
//  *
//  * [1] https://www.gnu.org/software/classpath/license.html
//  * [2] http://openjdk.java.net/legal/assembly-exception.html
//  *
//  * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
//  *******************************************************************************/

#ifndef TR_RELOCATION_RECORD_INCL
#define TR_RELOCATION_RECORD_INCL



#include "runtime/OMRRelocationRecord.hpp"


namespace TR
{

class OMR_EXTENSIBLE RelocationRecord: public OMR::RelocationRecordConnector
   {
   public:
      RelocationRecord(TR::RelocationRuntime *reloRuntime, TR::RelocationRecordBinaryTemplate *record) 
      : OMR::RelocationRecordConnector(reloRuntime,record)
        {}
      
      RelocationRecord(): OMR::RelocationRecordConnector() {}
      static RelocationRecord *create(TR::RelocationRecord *storage, TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget,
				      TR::RelocationRecordBinaryTemplate *recordPointer);

    
   };

class OMR_EXTENSIBLE RelocationRecordBinaryTemplate: public OMR::RelocationRecordBinaryTemplateConnector
   {
      public:
      RelocationRecordBinaryTemplate():OMR::RelocationRecordBinaryTemplateConnector(){};
   };
class OMR_EXTENSIBLE RelocationRecordGroup: public OMR::RelocationRecordGroupConnector
   {
      public:
      RelocationRecordGroup(TR::RelocationRecordBinaryTemplate *groupData)
      :OMR::RelocationRecordGroupConnector(groupData){};
   };
class OMR_EXTENSIBLE RelocationRecordMethodCallAddress: public OMR::RelocationRecordMethodCallAddressConnector
   {
   public:
      RelocationRecordMethodCallAddress(TR::RelocationRuntime *reloRuntime, TR::RelocationRecordBinaryTemplate *record) 
      : OMR::RelocationRecordMethodCallAddressConnector(reloRuntime,record)
        {}
      
      RelocationRecordMethodCallAddress(): OMR::RelocationRecordMethodCallAddressConnector() {}
      static RelocationRecord *create(TR::RelocationRecord *storage, TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget,
				      TR::RelocationRecordBinaryTemplate *recordPointer);

    
   };
class OMR_EXTENSIBLE RelocationRecordDataAddress: public OMR::RelocationRecordDataAddressConnector
   {
   public:
      RelocationRecordDataAddress(TR::RelocationRuntime *reloRuntime, TR::RelocationRecordBinaryTemplate *record) 
      : OMR::RelocationRecordDataAddressConnector(reloRuntime,record)
        {}
      
      RelocationRecordDataAddress(): OMR::RelocationRecordDataAddressConnector() {}
      static RelocationRecord *create(TR::RelocationRecord *storage, TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget,
				      TR::RelocationRecordBinaryTemplate *recordPointer);

    
   };
class OMR_EXTENSIBLE RelocationRecordDisplacementSite: public OMR::RelocationRecordDisplacementSiteConnector
   {
   public:
      RelocationRecordDisplacementSite(TR::RelocationRuntime *reloRuntime, TR::RelocationRecordBinaryTemplate *record) 
      : OMR::RelocationRecordDisplacementSiteConnector(reloRuntime,record)
        {}
      
      RelocationRecordDisplacementSite(): OMR::RelocationRecordDisplacementSiteConnector() {}
      static RelocationRecord *create(TR::RelocationRecord *storage, TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget,
				      TR::RelocationRecordBinaryTemplate *recordPointer);

    
   };
class OMR_EXTENSIBLE RelocationRecordMethodCallAddressBinaryTemplate: public OMR::RelocationRecordMethodCallAddressBinaryTemplateConnector
   {
      public:
      RelocationRecordMethodCallAddressBinaryTemplate():OMR::RelocationRecordMethodCallAddressBinaryTemplateConnector(){};
   };
class OMR_EXTENSIBLE RelocationRecordDataAddressBinaryTemplate: public OMR::RelocationRecordDataAddressBinaryTemplateConnector
   {
      public:
      RelocationRecordDataAddressBinaryTemplate():OMR::RelocationRecordDataAddressBinaryTemplateConnector(){};
   };
class OMR_EXTENSIBLE RelocationRecordDisplacementSiteBinaryTemplate: public OMR::RelocationRecordDisplacementSiteBinaryTemplateConnector
   {
      public:
      RelocationRecordDisplacementSiteBinaryTemplate():OMR::RelocationRecordDisplacementSiteBinaryTemplateConnector(){};
   };
class OMR_EXTENSIBLE RelocationRecordSOMObjectBinaryTemplate: public OMR::RelocationRecordSOMObjectBinaryTemplateConnector
   {
      public:
      RelocationRecordSOMObjectBinaryTemplate():OMR::RelocationRecordSOMObjectBinaryTemplateConnector(){};
   };
}

#endif /* RELOCATION_RECORD_INCL */
