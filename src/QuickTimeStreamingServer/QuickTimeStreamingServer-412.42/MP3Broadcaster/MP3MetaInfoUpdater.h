/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1999-2001 Apple Computer, Inc.  All Rights Reserved. The
 * contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License.  Please
 * obtain a copy of the License at http://www.apple.com/publicsource and
 * read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.  Please
 * see the License for the specific language governing rights and
 * limitations under the License.
 *
 *
 * @APPLE_LICENSE_HEADER_END@
 *
 */

#ifndef __MP3MetaInfoUpdater_H__
#define __MP3MetaInfoUpdater_H__

#include "OSThread.h"
#include "OSCond.h"
#include "TCPSocket.h"

class MP3MetaInfoUpdater : public OSThread
{
public:
	MP3MetaInfoUpdater(char* password, char* mountPoint, UInt32 addr, UInt16 port);
	~MP3MetaInfoUpdater();
	
	void Entry();
	
	void RequestMetaInfoUpdate(char* song);
	
private:
	void DoUpdateMetaInfo();
	
	OSCond mCond;
	OSMutex mMutex;
	char mSong[600];
	char* mPassword;
	char* mMountPoint;
	TCPSocket mSocket;
	UInt32 mAddr;
	UInt16 mPort;
	bool mFirstTime;
};

#endif
