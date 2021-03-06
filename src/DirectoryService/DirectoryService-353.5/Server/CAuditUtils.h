/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*!
 * @header CAuditUtils
 */

#ifndef __CAuditUtils_h__
#define	__CAuditUtils_h__		1

#define USE_BSM_AUDIT		1

#include <sys/ucred.h>
#include <bsm/audit.h>
#include <bsm/audit_uevents.h>
#include <sys/syscall.h>
#include <bsm/libbsm.h>
#define	_SYS_AUDIT_H	// disable sys/audit.h

#include <stdio.h>
#include <stdlib.h>				// for malloc()
#include <stdbool.h>
#include "PrivateTypes.h"
#include "DirServicesConst.h"
#include "DirServicesTypes.h"

#define kAuditUnknownNameStr "unknown"

typedef enum
{
	kATHChange,
	kATHAdd,
	kATHRemove
} AuditTypeHint;

/* list of authentication methods that are audited */

#define		kAuditAuthPasswordChangeConsts		8
#define		kAuditAuthChangeConsts				15
#define		kAuditAuthMethodConsts				33

enum {
	kAuditCtlStrNewUser						= 0,
	kAuditCtlStrModifyUser1					= 1,
	kAuditCtlStrModifyUser2					= 2,
	kAuditCtlStrModifyPassword				= 3,
	kAuditCtlStrDeleteUser					= 4,
	kAuditCtlStrCreateGroup					= 5,
	kAuditCtlStrDeleteGroup					= 6,
	kAuditCtlStrModifyGroupMembership1		= 7,
	kAuditCtlStrModifyGroupMembership2		= 8,
	kAuditCtlStrAddToGroup					= 9,
	kAuditCtlStrRemoveFromGroup				= 10,
	kAuditCtlStrModifyGroupAttribute		= 11,
	kAuditCtlStrAuthenticateUser			= 12,
	kAuditControlStrConsts
};

#ifdef __cplusplus
extern "C" {
#endif

uInt32		AuditForThisEvent			( uInt32 inType, void *inData, char **outTextStr );
tDirStatus	AuditUserOrGroupRecord		( tRecordReference inRecRef, char **outRecNameStr, char **outRecTypeStr, uInt32 *outEventCode );
tDirStatus	AuditGetRecordRefInfo		( tRecordReference inRecRef, char **outRecNameStr, char **outRecTypeStr );
tDirStatus	AuditGetNameFromAuthBuffer	( tDataNodePtr inAuthMethod, tDataBufferPtr inAuthBuffer, char **outUserNameStr );

#ifdef __cplusplus
}
#endif

#endif

