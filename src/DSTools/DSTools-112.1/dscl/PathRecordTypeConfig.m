/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
 * @header PathRecordTypeConfig
 */


#import "PathRecordTypeConfig.h"
#import "DSoException.h"
#import "DSoNode.h"

@implementation PathRecordTypeConfig

- (NSArray*) getList:(NSString*)inKey
{
    NSArray *list = nil;

    NS_DURING
        if ([_recordType hasPrefix:@kDSStdRecordTypePrefix])
            list = [[_node findRecordNames:@kDSRecordsAll
                            ofType:[_recordType UTF8String]
                            matchType:eDSExact]
                        sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];
        else
            list = [[_node findRecordNames:@"dsConfigType::GetAllRecords"
                            ofType:[_recordType UTF8String]
                            matchType:eDSExact]
                        sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];
	NS_HANDLER
		if (!DS_EXCEPTION_STATUS_IS(eDSRecordNotFound) &&
	        !DS_EXCEPTION_STATUS_IS(eDSInvalidRecordType))
		{
			[localException raise];
		}
	NS_ENDHANDLER

	return list;
}

@end
