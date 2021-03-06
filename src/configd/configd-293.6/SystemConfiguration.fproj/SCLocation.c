/*
 * Copyright (c) 2002, 2004, 2006 Apple Computer, Inc. All rights reserved.
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

/*
 * Modification History
 *
 * Nov 28, 2001			Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>


CFStringRef
SCDynamicStoreKeyCreateLocation(CFAllocatorRef allocator)
{
	return CFRetain(kSCDynamicStoreDomainSetup);
}


CFStringRef
SCDynamicStoreCopyLocation(SCDynamicStoreRef store)
{
	CFDictionaryRef		dict		= NULL;
	CFStringRef		key;
	CFStringRef		location	= NULL;
	Boolean			tempSession	= FALSE;

	if (store == NULL) {
		store = SCDynamicStoreCreate(NULL,
					     CFSTR("SCDynamicStoreCopyLocation"),
					     NULL,
					     NULL);
		if (store == NULL) {
			SCLog(_sc_verbose, LOG_INFO, CFSTR("SCDynamicStoreCreate() failed"));
			return NULL;
		}
		tempSession = TRUE;
	}

	key  = SCDynamicStoreKeyCreateLocation(NULL);
	dict = SCDynamicStoreCopyValue(store, key);
	CFRelease(key);
	if (!isA_CFDictionary(dict)) {
		_SCErrorSet(kSCStatusNoKey);
		goto done;
	}

	location = CFDictionaryGetValue(dict, kSCDynamicStorePropSetupCurrentSet);
	if (!isA_CFString(location)) {
		_SCErrorSet(kSCStatusNoKey);
		goto done;
	}

	CFRetain(location);

    done :


	if (tempSession)	CFRelease(store);
	if (dict)		CFRelease(dict);

	return location;
}
