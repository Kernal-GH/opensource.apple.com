/*
 * Copyright (c) 2000-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

#ifndef _SCPREFERENCESPRIVATE_H
#define _SCPREFERENCESPRIVATE_H

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SCPreferences.h>


/*!
	@enum SCPreferencesKeyType
	@discussion Used with the SCDynamicStoreKeyCreatePreferences() function
		to describe the resulting CFStringRef argument.
	@constant kSCPreferencesKeyLock Key used when exclusive access to the
		stored preferences is obtained or released.
	@constant kSCPreferencesKeyCommit Key used when new preferences are
		committed to the store
	@constant kSCPreferencesKeyApply Key used when new preferences are
		to be applied to the active system configuration.
 */
enum {
	kSCPreferencesKeyLock	= 1,
	kSCPreferencesKeyCommit	= 2,
	kSCPreferencesKeyApply	= 3
};
typedef	int32_t	SCPreferencesKeyType;

__BEGIN_DECLS

/*!
	@function SCDynamicStoreKeyCreatePreferences
	@discussion Creates a key that can be used by the SCDynamicStoreSetNotificationKeys()
		function to receive notifications of changes to the saved
		preferences.
	@param allocator ...
	@param prefsID A string that identifies the name of the
		group of preferences to be accessed/updated.
	@param keyType A kSCPreferencesKeyType indicating the type a notification
		key to be returned.
	@result A notification string for the specified preference identifier.
 */
CFStringRef
SCDynamicStoreKeyCreatePreferences	(
					CFAllocatorRef		allocator,
					CFStringRef		prefsID,
					SCPreferencesKeyType	keyType
					);

SCPreferencesRef
SCUserPreferencesCreate			(
					CFAllocatorRef		allocator,
					CFStringRef		name,
					CFStringRef		prefsID,
					CFStringRef		user
					);

CFStringRef
SCDynamicStoreKeyCreateUserPreferences	(
					CFAllocatorRef		allocator,
					CFStringRef		prefsID,
					CFStringRef		user,
					SCPreferencesKeyType	keyType
					);

__END_DECLS

#endif /* _SCPREFERENCESPRIVATE_H */
