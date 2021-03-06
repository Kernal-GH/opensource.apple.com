/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/* DiskArbitrationTypes.h */

#ifndef __DISKARBITRATIONTYPES_H
#define __DISKARBITRATIONTYPES_H

/*
 * The disk arbitration server registers with the bootstrap server under this
 * name
 */

#define DISKARB_SERVER_NAME "DiskArbitration"
#define ADM_COOKIE_FILE ".autodiskmounted"

/* For the errorCode return value. */

enum {
	kDiskArbNoError = 0,
};

/* For the <flags> parameter to DiskArbitrationRegister. */

enum {
	kDiskArbNotifyNone = 0x00000000,
	kDiskArbNotifyAll = 0xFFFFFFFF,

	kDiskArbNotifyDiskAppearedWithoutMountpoint = 1 << 0,
	kDiskArbNotifyUnmount = 1 << 1,
	kDiskArbNotifyDiskAppearedWithMountpoint = 1 << 2,
	kDiskArbNotifyDiskAppeared = 1 << 3,	/* Obsoleted by kDiskArbNotifyDiskAppeared2 */
	kDiskArbNotifyDiskAppeared2 = 1 << 4,
	kDiskArbNotifyAsync = 1 << 5,
	kDiskArbNotifyBlueBoxBootVolumeUpdated = 1 << 6,
	kDiskArbNotifyCompleted = 1 << 7,
	kDiskArbNotifyChangedDisks = 1 << 8,
	kDiskArbArbitrateUnrecognizedVolumes = 1 << 9,
	kDiskArbNotifyUnrecognizedVolumes = 1 << 10,
	kDiskArbNotifyDiskWillBeChecked = 1 << 12,
	kDiskArbNotifyCallFailed = 1 << 13,
	kDiskArbNotifyCallSucceeded = 1 << 14,
	kDiskArbNotifyDiskApproval = 1 << 15,
	kDiskArbNotifyDiskAppearedComplete = 1 << 16,
};

/*
 * Beware: these definitions must be kept in sync with ClientToServer.defs
 * and ServerToClient.defs
 */

typedef char    DiskArbDiskIdentifier[1024];
typedef char    DiskArbSecurityToken[32];
typedef char    DiskArbMountpoint[1024];
typedef char    DiskArbIOContent[1024];
typedef char    DiskArbDeviceTreePath[1024];
typedef char    DiskArbGenericString[1024];

/* For the <flags> parameter to DiskArbitrationDiskAppeared. */

enum {
	kDiskArbDiskAppearedNoFlags = 0x00000000,

	kDiskArbDiskAppearedLockedMask = 1 << 0,
	kDiskArbDiskAppearedEjectableMask = 1 << 1,
	kDiskArbDiskAppearedWholeDiskMask = 1 << 2,
	kDiskArbDiskAppearedNetworkDiskMask = 1 << 3,
	kDiskArbDiskAppearedBeingCheckedMask = 1 << 4,
	kDiskArbDiskAppearedNonLeafDiskMask = 1 << 5,
	kDiskArbDiskAppearedCDROMMask = 1 << 6,
	kDiskArbDiskAppearedDVDROMMask = 1 << 7,
	kDiskArbDiskAppearedUnrecognizableFormat = 1 << 8,
	kDiskArbDiskAppearedUnrecognizableSection = 1 << 9,
	kDiskArbDiskAppearedRecognizableSectionMounted = 1 << 10,
	kDiskArbDiskAppearedDialogDisplayed = 1 << 11,
	kDiskArbDiskAppearedNoMountMask = 1 << 12,
	kDiskArbDiskAppearedNoSizeMask = 1 << 13,
    kDiskArbDiskAppearedCheckFailed = 1 << 14,
    kDiskArbDiskAppearedInternal = 1 << 15,
};

/* Flags for the DiskArbUnmount...() family of calls */
enum {
	kDiskArbUnmountNoFlags = 0x00000000,

	kDiskArbUnmountAllFlag = 1 << 0,
	kDiskArbUnmountAndEjectFlag = 1 << 1,	/* implies unmount-all */
	kDiskArbUnmountOneFlag = 1 << 2,	/* just unmount one partition */
	kDiskArbForceUnmountFlag = 1 << 3,	/* force the unmount */
	kDiskArbNetworkUnmountFlag = 1 << 4,	/* force the unmount and do not check authorization - the disk is already gone !! */
};

/*
 * Completed notification types - these are hints, and only hints, to carbon
 * that they can contrinue their run loop
 */
enum {
	kDiskArbCompletedNothing = 0x00000000,

	kDiskArbCompletedDiskAppeared = 1 << 0,
	kDiskArbCompletedPreUnmount = 1 << 1,
	kDiskArbCompletedPostUnmount = 1 << 2,
	kDiskArbCompletedPreEject = 1 << 3,
	kDiskArbCompletedPostEject = 1 << 4,
};

/* Completed Rename */
enum {
	kDiskArbRenameUnsuccessful = 0,
	kDiskArbRenameSuccessful = 1 << 0,
	kDiskArbRenameRequiresRemount = 1 << 1,
};

/* Disk Reservation Status */
enum {
	kDiskArbDeviceIsReserved = 0,
	kDiskArbDeviceIsNotReserved = 1,
	kDiskArbDeviceReservationObtained = 2,
	kDiskArbDeviceReservationRefused = 3,
};

/* VSDB Permissions */
#define kDiskArbVSDBPermissionsNotExist 	0
#define kDiskArbVSDBPermissionsEnabled		1
#define kDiskArbVSDBPermissionsDisabled		2


#define kDiskArbNoUser -1

enum {
	kDiskArbHandlesNothing = 0x00000000,

	kDiskArbHandlesUnrecognizedFixedMedia = 1 << 0,
	kDiskArbHandlesUnrecognizedCDMedia = 1 << 1,
	kDiskArbHandlesUnrecognizedDVDMedia = 1 << 2,
	kDiskArbHandlesUnrecognizedOtherRemovableMedia = 1 << 3,

	kDiskArbHandlesUninitializedFixedMedia = 1 << 10,
	kDiskArbHandlesUninitializedCDMedia = 1 << 11,
	kDiskArbHandlesUninitializedDVDMedia = 1 << 12,
	kDiskArbHandlesUninitializedOtherRemovableMedia = 1 << 13,
};

//success and failure of specific calls
enum {
	kDiskArbRequestFailed = 0x00000000,
	kDiskArbUnmountRequestFailed = 1 << 0,
	kDiskArbEjectRequestFailed = 1 << 1,
	kDiskArbUnmountAndEjectRequestFailed = 1 << 2,
	kDiskArbDiskChangeRequestFailed = 1 << 3,
	kDiskArbSetEncodingRequestFailed = 1 << 4,
	kDiskArbVSDBAdoptRequestFailed = 1 << 5,
	kDiskArbVSDBDisownRequestFailed = 1 << 6,
	kDiskArbVSDBAdoptRequestSucceeded = 1 << 7,
	kDiskArbVSDBDisownRequestSucceeded = 1 << 8,
	kDiskArbSetEncodingRequestSucceeded = 1 << 9,
};

//reasons for failure
enum {
    kDiskArbVolumeDoesNotExist = 1 << 0,
    kDiskArbParametersIncorrect = 1 << 1,
    kDiskArbInsecureRequest = 1 << 2,
    kDiskArbIsBusy = 1 << 3,
    kDiskArbDiskIsNetwork = 1 << 4,
    kDiskArbInvalidVolumeFormat = 1 << 5,
    kDiskArbVolumeCannotUnmount = 1 << 6,
    kDiskArbVSDBCannotDisown = 1 << 7,
    kDiskArbVSDBCannotAdopt = 1 << 8,
};

//acknowledgement return values

enum {
    kDiskArbApproveMounting				= 0,
    kDiskArbDisallowMounting			= 1 << 0,
    kDiskArbEjectDevice					= 1 << 2,
    kDiskArbRequireAuthentication		= 1 << 3,
    kDiskArbMountReadOnly				= 1 << 4,
};


#define kDiskArbHandlesAllUnrecognizedMedia ( kDiskArbHandlesUnrecognizedFixedMedia | kDiskArbHandlesUnrecognizedCDMedia | kDiskArbHandlesUnrecognizedDVDMedia | kDiskArbHandlesUnrecognizedOtherRemovableMedia)

#define kDiskArbHandlesAllUninitializedMedia ( kDiskArbHandlesUninitializedFixedMedia | kDiskArbHandlesUninitializedCDMedia | kDiskArbHandlesUninitializedDVDMedia | kDiskArbHandlesUninitializedOtherRemovableMedia )

#define kDiskArbHandlesAllUnrecognizedOrUninitializedMedia ( kDiskArbHandlesAllUnrecognizedMedia | kDiskArbHandlesAllUninitializedMedia)


#endif
