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

/*
 * Controller.m
 *
 * Controller for lookupd
 * 
 * Copyright (c) 1995, NeXT Computer Inc.
 * All rights reserved.
 * Written by Marc Majka
 */

#import <NetInfo/config.h>
#import <NetInfo/system_log.h>
#import <NetInfo/project_version.h>
#import "Controller.h"
#import "Thread.h"
#import "LUPrivate.h"
#import "LUCachedDictionary.h"
#import "Config.h"
#import "LUGlobal.h"
#import "MachRPC.h"
#import "LUServer.h"
#import "CacheAgent.h"
#import "DNSAgent.h"
#import "NILAgent.h"
#import <NetInfo/dsutil.h>
#import <NetInfo/ni_shared.h>
#import "sys.h"
#import <sys/types.h>
#import <sys/param.h>
#import <unistd.h>
#import <string.h>
#import <libc.h>

#define forever for(;;)

extern int gethostname(char *, int);
extern sys_port_type server_port;
extern sys_port_type _lookupd_port(sys_port_type);
extern int _lookup_link();

@implementation Controller
	
/*
 * Server runs in this loop to answer requests
 */
- (void)serverLoop
{
	kern_return_t status;
	lookup_request_msg *request;
	Thread *t, *x;

	t = [Thread currentThread];

	forever
	{
		/* Receive and service a request */
		[t setState:ThreadStateIdle];
		request = (lookup_request_msg *)calloc(1, sizeof(lookup_request_msg));
		status = sys_receive_message(&request->head, sizeof(lookup_request_msg), server_port, 0);
		[t setState:ThreadStateActive];

		if (shutting_down) break;
		if (status != KERN_SUCCESS)
		{
			system_log(LOG_NOTICE, "Server status = %s (%d)", sys_strerror(status), status);
			continue;
		}
		
		syslock_lock(threadCountLock);
		idleThreads--;
		if ((idleThreads == 0) && (threadCount < maxThreads))
		{
			x = [[Thread alloc] init];
			[x setName:"IPC Server"];
			[x shouldTerminate:YES];
			[x run:@selector(serverLoop) context:self];
			threadCount++;
			idleThreads++;
		}
		syslock_unlock(threadCountLock);

		/* request is owned by the thread, which needs to free it when done */
		[t setData:(void *)request];
		[machRPC process];

		/* check idle threads */
		syslock_lock(threadCountLock);
		idleThreads++;
		if ((idleThreads > maxIdleThreads) && ([t shouldTerminate]))
		{
			idleThreads--;
			threadCount--;
			syslock_unlock(threadCountLock);
			system_log(LOG_DEBUG, "serverloop terminated thread (%d idle, max %d)", idleThreads, maxIdleThreads);
			[t terminateSelf];
		}
		syslock_unlock(threadCountLock);

		if (shutting_down) break;
	}
}

- (void)newAgent:(id)agent name:(char *)name
{
	if (agentCount == 0) agents = (id *)malloc(sizeof(id));
	else agents = (id *)realloc(agents, (agentCount + 1) * sizeof(id));
	agents[agentCount] = agent;
	agentNames = appendString(name, agentNames);
	agentCount++;
}

- (id)agentClassNamed:(char *)name
{
	int i;
	char cname[256];

	i = listIndex(name, agentNames);
	if (i != IndexNull) return agents[i];

	sprintf(cname, "%sAgent", name);
	i = listIndex(cname, agentNames);
	if (i != IndexNull) return agents[i];

	return nil;
}

- (void)initConfig
{
	LUArray *configurationArray;
	LUDictionary *global;
	char *logFileName;
	char *logFacilityName;
	FILE *fp;
	int pri;
	time_t now;
	char str[64];

	configurationArray = [configManager config];
	global = [configManager configGlobal:configurationArray];

	statistics_enabled = [configManager boolForKey:"StatisticsEnabled" dict:global default:statistics_enabled];

	logFileName = [configManager stringForKey:"LogFile" dict:global default:NULL];
	fp = fopen(logFileName, "a");
 	system_log_set_logfile(fp);
	freeString(logFileName);

	logFacilityName = [configManager stringForKey:"LogFacility" dict:global default:"LOG_NETINFO"];
	freeString(logFacilityName);

	now = time(0);
	sprintf(str, "lookupd (version %s) starting - %s", _PROJECT_VERSION_, ctime(&now));

	/* remove ctime trailing newline */
	str[strlen(str) - 1] = '\0';
	system_log(LOG_NOTICE, str);

	maxThreads = [configManager intForKey:"MaxThreads" dict:global default:64];
	maxIdleThreads = [configManager intForKey:"MaxIdleThreads" dict:global default:2];
	maxIdleServers = [configManager intForKey:"MaxIdleServers" dict:global default:16];

	pri = [configManager intForKey:"LogPriority" dict:global default:system_log_max_priority()];
	system_log_set_max_priority(pri);

	[configurationArray release];
}

- (id)initWithName:(char *)name
{
	char str[128];
	DNSAgent *dns;

	[super init];

	dnsSearchList = NULL;

	controller = self;

 	if (name == NULL) portName = NULL;
	else portName = copyString((char *)name);

	sprintf(str, "Controller");
	[self setBanner:str];

	if (![self registerPort:portName]) return nil;

	serverLock = syslock_new(1);
	threadCountLock = syslock_new(0);
	threadCount = 1;
	idleThreads = 1;

	serverList = [[LUArray alloc] init];
	[serverList setBanner:"Controller server list"];

	statistics = [[LUDictionary alloc] init];
	[statistics setBanner:"lookupd statistics"];
	
	[statistics setValue:_PROJECT_BUILD_INFO_ forKey:"# build"];
	[statistics setValue:_PROJECT_VERSION_    forKey:"# version"];

	[self newAgent:[CacheAgent class] name:"CacheAgent"];
	[self newAgent:[DNSAgent class] name:"DNSAgent"];
	[self newAgent:[NILAgent class] name:"NILAgent"];

	[self initConfig];

	loginUser = nil;

	machRPC = [[MachRPC alloc] init:self];

	dns = [[DNSAgent alloc] init];
	if (dns != nil)
	{
		dnsSearchList = [dns searchList];
		[dns release];
	}

	return self;
}

- (char *)portName
{
	return portName;
}

- (char **)dnsSearchList
{
	return dnsSearchList;
}

- (void)dealloc
{
	if (dnsSearchList != NULL) freeList(dnsSearchList);
	dnsSearchList = NULL;

	[machRPC release];
	machRPC = nil;

	if (serverList != nil)
		[serverList release];

	if (portName != NULL) 
	{
		sys_destroy_service(portName);
		freeString(portName);
	}
	portName = NULL;

	sys_port_free(server_port);

	if (loginUser != nil) [loginUser release];

	syslock_free(serverLock);
	syslock_free(threadCountLock);

	if (statistics != nil) [statistics release];
	freeList(agentNames);
	agentNames = NULL;
	free(agents);

	[super dealloc];
}

- (BOOL)registerPort:(char *)name
{
	kern_return_t status;
	int restart;

	if (portName == NULL) return YES;

	restart = 0;
	system_log(LOG_DEBUG, "Registering service \"%s\"", portName);

	if (streq(name, DefaultName)) restart = 1;

	status = sys_create_service(name, &server_port, restart);
	if (status == KERN_SUCCESS) return YES;

	system_log(LOG_ERR, "Can't create service! (error %d)", status);
	return NO;
}

- (void)startServerThread
{
	Thread *t;

	syslock_lock(serverLock);

	/*
	 * Create the thread
	 */
	t = [[Thread alloc] init];
	[t setName:"IPC Server"];
	[t shouldTerminate:YES];
	[t run:@selector(serverLoop) context:self];
	idleThreads = 1;

	syslock_unlock(serverLock);

	system_log(LOG_DEBUG, "Started IPC Server");
}

/*
 * Get an idle server from the server list
 */
- (LUServer *)checkOutServer
{
	LUServer *server;
	int i;

	syslock_lock(serverLock);
	server = nil;

	for (i = [serverList count] - 1; i >= 0; i--)
	{
		if ([[serverList objectAtIndex:i] isIdle])
		{
			server = [serverList objectAtIndex:i];
			[server setIsIdle:NO];
			break;
		}
	}

	if (server == nil)
	{
		/*
		 * No servers available - create a new server 
		 */
		server = [[LUServer alloc] init];
		[server setIsIdle:NO];
		[serverList addObject:server];
		[server release];
	}

	syslock_unlock(serverLock);
	return server;
}

- (void)checkInServer:(LUServer *)server
{
	int i, len, idleServerCount;

	[cacheAgent sweepCache];
	syslock_lock(serverLock);

	[server setIsIdle:YES];

	idleServerCount = 0;
	len = [serverList count];
	for (i = 0; i < len; i++)
	{
		if ([[serverList objectAtIndex:i] isIdle]) idleServerCount++;
	}

	if (idleServerCount > maxIdleServers)
	{
		[serverList removeObject:server];
	}

	syslock_unlock(serverLock);
}

- (void)setLoginUser:(int)uid
{
	LUServer *s;
	char scratch[256];

	if (loginUser != nil)
	{
		[cacheAgent removeObject:loginUser];
		[loginUser release];
		loginUser = nil;
	}

	sprintf(scratch, "%d", uid);
	s = [self checkOutServer];
	loginUser = [s itemWithKey:"uid" value:scratch category:LUCategoryUser];
	[self checkInServer:s];

	if (loginUser != nil)
	{
		[cacheAgent addObject:loginUser key:"name" category:LUCategoryUser];
		[cacheAgent addObject:loginUser key:"uid" category:LUCategoryUser];
		[loginUser setTimeToLive:(time_t)-1];
		sprintf(scratch, "%s (console user)", [loginUser banner]);
		[loginUser setBanner:scratch];
	}
}

- (void)flushCache
{
	[cacheAgent flushCache];
}

- (void)suspend
{
	/* XXX suspend */
}

/*
 * This is just here to send a message to the server port.
 * This wakes the thread blocked on message receive in serverLoop.
 */
- (void)lookupdMessage
{
	sys_port_type s;
	kern_return_t status;
	int proc;

	s = lookupd_port(portName);
	status = _lookup_link(s, "_getstatistics", &proc);

	[[Thread currentThread] terminateSelf];
}

- (unsigned int)memorySize
{
	unsigned int size, i;

	size = [super memorySize];

	size += 60;
	if (serverLock != NULL) size += sizeof(syslock);

	size += (NCATEGORIES * 4);

	if (agentNames != NULL)
	{
		for (i = 0; agentNames[i] != NULL; i++)
		{
			size += 4;
			size += (strlen(agentNames[i]) + 1);
		}
	}
	if (dnsSearchList != NULL)
	{
		for (i = 0; dnsSearchList[i] != NULL; i++)
		{
			size += 4;
			size += (strlen(dnsSearchList[i]) + 1);
		}
	}

	if (portName != NULL) size += (strlen(portName) + 1);

	size += (agentCount * 4);

	return size;
}

@end
