/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
 * @header ServerControl
 */

#if DEBUG_SRVR
# include <stdio.h>			// for stderr, fprintf(), et al
#endif

#include "ServerControl.h"
#include "DirServicesConst.h"
#include "DirServicesPriv.h"
#include "CHandlers.h"
#include "CRefTable.h"
#include "DSMutexSemaphore.h"
#include "DSCThread.h"
#include "CServerPlugin.h"
#include "CPluginHandler.h"
#include "CSearchPlugin.h"
#include "CNodeList.h"
#include "CLog.h"
#include "CPluginConfig.h"
#include "SharedConsts.h"
#include "CFile.h"
#include "CAuditUtils.h"
#include "DSMachEndian.h"
#include "WorkstationService.h"
#include "Mbrd_MembershipResolver.h"
#include "DSLDAPUtils.h"
#include "CDSPluginUtils.h"
#include "DSTCPEndpoint.h"
#include "DSTCPEndian.h"
#include "CInternalDispatch.h"
#include "COSUtils.h"

#include <mach/mach.h>
#include <mach/notify.h>
#include <sys/stat.h>							//used for mkdir and stat
#include <IOKit/pwr_mgt/IOPMLib.h>				//required for power management handling
#include <syslog.h>								// for syslog()
#include <time.h>								// for time
#include <bsm/libbsm.h>
#include <uuid/uuid.h>
#include <netdb.h>
#include <sys/socket.h>
#include <assert.h>
#include <SystemConfiguration/SCDynamicStore.h>
#include <dispatch/dispatch.h>
#include <sys/sysctl.h>	// for struct kinfo_proc and sysctl()
#include <fcntl.h>

// This is for MIG
extern "C" {
	#include "DirectoryServiceMIGServer.h"
	#include "DSlibinfoMIGServer.h"
	#include "DSmemberdMIGServer.h"
	#include "DSlibinfoMIGAsyncReply.h"
}

dispatch_source_t ServerControl::fTCPListener = NULL;

//power management
extern void dsPMNotificationHandler ( void *refContext, io_service_t service, natural_t messageType, void *notificationID );
extern io_object_t		gPMDeregisterNotifier;
extern io_connect_t		gPMKernelPort;

//network change
extern void NetworkChangeCallBack(SCDynamicStoreRef aSCDStore, CFArrayRef changedKeys, void *callback_argument);
extern CFRunLoopRef		gPluginRunLoop;

extern bool				gLogAPICalls;
extern bool				gDebugLogging;
extern dsBool			gDSFWCSBPDebugLogging;
extern bool				gIgnoreSunsetTime;

extern	bool			gServerOS;
extern dsBool			gDSDebugMode;
extern CCachePlugin	   *gCacheNode;
extern dsBool			gDSLocalOnlyMode;
extern dsBool			gDSInstallDaemonMode;
extern DSEventSemaphore gKickCacheRequests;

// ---------------------------------------------------------------------------
//	* Globals
//
// ---------------------------------------------------------------------------

UInt32					gAPICallCount		= 0;
UInt32					gLookupAPICallCount	= 0;
ServerControl		   *gSrvrCntl			= nil;
CRefTable				gRefTable( ServerControl::RefDeallocProc );
CPlugInList			   *gPlugins			= nil;
dispatch_queue_t		gLibinfoQueue		= NULL;
CPluginConfig		   *gPluginConfig		= nil;
CNodeList			   *gNodeList			= nil;
CPluginHandler		   *gPluginHandler		= nil;
char				   *gDSLocalFilePath	= nil;
UInt32					gLocalSessionCount	= 0;
DSSemaphore				gLocalSessionLock("::gLocalSessionLock");

DSMutexSemaphore	   *gTCPHandlerLock			= new DSMutexSemaphore("::gTCPHandlerLock");	//mutex on create and destroy of CHandler threads
DSMutexSemaphore	   *gPerformanceLoggingLock = new DSMutexSemaphore("::gPerformanceLoggingLock");	//mutex on manipulating performance logging matrix
DSMutexSemaphore	   *gLazyPluginLoadingLock	= new DSMutexSemaphore("::gLazyPluginLoadingLock");	//mutex on loading plugins lazily
DSMutexSemaphore	   *gHashAuthFailedMapLock  = new DSMutexSemaphore("::gHashAuthFailedMapLock");	//mutex on failed shadow hash login table
DSMutexSemaphore	   *gHashAuthFailedLocalMapLock = gHashAuthFailedMapLock; //same mutex but different name
DSMutexSemaphore	   *gMachThreadLock			= new DSMutexSemaphore("::gMachThreadLock");	//mutex on count of mig handler threads
DSMutexSemaphore	   *gTimerMutex				= new DSMutexSemaphore("::gTimerMutex");		//mutex for creating/deleting timers

pid_t					gDaemonPID;
in_addr_t				gDaemonIPAddress;
UInt32					gRefCountWarningLimit						= 500;
UInt32					gDelayFailedLocalAuthReturnsDeltaInSeconds  = 1;
UInt32					gMaxHandlerThreadCount						= kMaxHandlerThreads;
dsBool					gToggleDebugging							= false;
bool					gFirstNetworkUpAtBoot						= false;
bool					gNetInfoPluginIsLoaded						= false;

dispatch_source_t		gLibinfoDispatchSource						= NULL;
dispatch_source_t		gAPIDispatchSource							= NULL;
dispatch_source_t		gMembershipDispatchSource					= NULL;

extern mach_port_t		gLibinfoMachPort;
extern mach_port_t		gAPIMachPort;
extern mach_port_t		gMembershipMachPort;

//eDSLookupProcedureNumber MUST be SYNC'ed with lookupProcedures
const char *lookupProcedures[] =
{
    "firstprocnum",
    
    // Users
    "getpwnam",
    "getpwuuid",
    "getpwuid",
    "getpwent",
    
    // Groups
    "getgrnam",
    "getgruuid",
    "getgrgid",
    "getgrent",
    
    // Services
    "getservbyname",
    "getservbyport",
    "getservent",
    
    // Protocols
    "getprotobyname",
    "getprotobynumber",
    "getprotoent",
    
    // RPCs
    "getrpcbyname",
    "getrpcbynumber",
    "getrpcent",
    
    // Mounts
    "getfsbyname",
    "getfsent",
    
    // Printers -- deprecated
//    "prdb_getbyname",
//    "prdb_get",
    
    // bootparams -- deprecated
//    "bootparams_getbyname",
//    "bootparams_getent",
    
    // Aliases
    "alias_getbyname",
    "alias_getent",
    
    // Networks
    "getnetent",
    "getnetbyname",
    "getnetbyaddr",
    
    // Bootp -- deprecated
//    "bootp_getbyip",
//    "bootp_getbyether",
    
    // NetGroups
    "innetgr",
    "getnetgrent",
    
    // DNS calls
#ifdef HANDLE_DNS_LOOKUPS    
    "getaddrinfo",
    "getnameinfo",
#endif
    "gethostbyname",
    "gethostbyaddr",
    "gethostent",
    "getmacbyname",
    "gethostbymac",
	
	"getbootpbyhw",
	"getbootpbyaddr",

	// other calls
#ifdef HANDLE_DNS_LOOKUPS    
    "dns_proxy",
#endif
    "gethostbyname_service",
    "_flushcache",
    "_flushentry",
    
    "getpwnam_ext",
    "getpwnam_initext",
    "getgrnam_ext",
    "getgrnam_initext",
	
    "lastprocnum",
    NULL				// safety in case we get out of sync
};

#pragma mark -
#pragma mark MIG Call Handler Routines - separate DS, Lookup, and memberd servers
#pragma mark -

kern_return_t dsmig_do_checkUsernameAndPassword( mach_port_t server,
												 sStringPtr username,
												 sStringPtr password,
												 int32_t *result,
												 audit_token_t atoken )
{
	CRequestHandler handler;
	char *debugDataTag = NULL;
	
	if ( (gDebugLogging) || (gLogAPICalls) )
	{
		pid_t	aPID;
		audit_token_to_au32( atoken, NULL, NULL, NULL, NULL, NULL, &aPID, NULL, NULL );

		debugDataTag = handler.BuildAPICallDebugDataTag( NULL, aPID, "checkpw()", "Server" );
		DbgLog( kLogHandler, "%s : dsmig DAC : Username = %s", debugDataTag, username );
	}

#if USE_BSM_AUDIT
	uid_t			auidp;
	uid_t			euidp;
	gid_t			egidp;
	uid_t			ruidp;
	gid_t			rgidp;
	pid_t			pidp;
	au_asid_t		asidp;
	au_tid_t		tidp;
	audit_token_to_au32( atoken, &auidp, &euidp, &egidp, &ruidp, &rgidp, &pidp, &asidp, &tidp );
	char *textStr = nil;
	UInt32 bsmEventCode = AuditForThisEvent( kCheckUserNameAndPassword, username, &textStr );
#endif
		
	*result = handler.DoCheckUserNameAndPassword( username, password, eDSExact, NULL, NULL );
	
#if USE_BSM_AUDIT
	// BSM Audit
	if ( bsmEventCode > 0 )
	{
		token_t *tok = NULL;
		
		if ( *result == eDSNoErr )
		{
			if ( textStr != NULL ) tok = au_to_text( textStr );
			audit_write_success( bsmEventCode, tok,
									auidp,
									euidp,
									egidp,
									ruidp,
									rgidp,
									pidp,
									asidp,
									&tidp );
		}
		else
		{
			audit_write_failure( bsmEventCode, textStr, (int)*result,
									auidp,
									euidp,
									egidp,
									ruidp,
									rgidp,
									pidp,
									asidp,
									&tidp );
		}
	}
	DSFreeString( textStr );	// sets to NULL; required
#endif

	if ( debugDataTag )
	{
		DbgLog( kLogHandler, "%s : dsmig DAR : Username %s : Result code = %d", debugDataTag, username, *result );
		free( debugDataTag );
	}

	return KERN_SUCCESS;
}

// used to process the mach messages and route to the correct MIG server
// key difference is, it forces the thread running to be an internal dispatch (not to be confused with libdispatch) thread
//   which prevents calls to DS APIs from going over mach back to ourselves
static boolean_t dsmig_demux_internaldispatch( mach_msg_header_t *request, mach_msg_header_t *reply )
{
	boolean_t	result;
	
	CInternalDispatch::AddCapability();
	
	if ( request->msgh_id >= 60000 ) // 60000 are memberd requests
		result = DSmemberdMIG_server( request, reply );
	else if ( request->msgh_id >= 50000 ) // 50000 are libinfo requests
		result = DSlibinfoMIG_server( request, reply );
	else
		result = DirectoryServiceMIG_server(request, reply);

	if ( request->msgh_id == MACH_NOTIFY_NO_SENDERS ) {
		mach_vm_address_t context = NULL;
		
		// clean up the mach message port
		mach_port_get_context( mach_task_self(), request->msgh_local_port, &context );
		DbgLog( kLogDebug, "dsdispatch_no_senders_notification:: %u", request->msgh_local_port );
		gRefTable.CleanRefsForMachRefs( request->msgh_local_port );
		
		if ( context != 0 ) {
            dispatch_cancel( (dispatch_source_t) context );
            dispatch_release( (dispatch_source_t) context );
		}
	}
	
	return result;
}

static void
dsdispatch_process_wentaway( pid_t inProc )
{
    DbgLog( kLogNotice, "dsdispatch_process_wentaway:: PID: %d - has exited, exec'd, or closed session", inProc );
    
    // no need to keep this daemon going anymore as it launches on demand
    // create a lazy timer
    // TODO: need a better on-demand solution, we use hold a mutex while we exit,
    // clients will recover
    dispatch_source_t source = dispatch_source_timer_create( DISPATCH_TIMER_ONESHOT, 
                                                             5ULL * NSEC_PER_SEC,
                                                             1ULL * NSEC_PER_SEC,
                                                             NULL,
                                                             dispatch_get_concurrent_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT),
                                                             ^(dispatch_source_t ds) {
                                                                 // use try lock in case we collide with another timer which gets done on the
                                                                 // same thread will cause an abort
                                                                 if ( dispatch_source_get_error(ds, NULL) == 0 && gLocalSessionLock.WaitTryLock() ) {
                                                                     if ( gLocalSessionCount == 0 ) {
                                                                         // just stop the runloop it will exit
                                                                         CFRunLoopStop( CFRunLoopGetMain() );
                                                                         
                                                                         // we intentionally don't unlock cause we are going to exit
                                                                     } else {
                                                                         gLocalSessionLock.SignalLock();
                                                                     }
                                                                 }
                                                             } );
    assert( source != NULL );
}

static dispatch_source_t
CreateDispatchSourceForMachPort( mach_port_t newServer, size_t maxSize, pid_t inPID, bool bPerClientPort )
{
#define MY_MIG_OPTIONS	(MACH_RCV_TIMEOUT | MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_CTX) | \
                         MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0))
	
    dispatch_source_t machSource;
    dispatch_queue_t machQueue;
    
    if ( bPerClientPort == true ) {
        // we use serial queues for per-client ports to ensure ordered requests
        machQueue = dispatch_queue_create( "per-client MIG queue", NULL );
        assert( machQueue != NULL );
        
        machSource = dispatch_source_machport_create( newServer,
                                                      DISPATCH_MACHPORT_RECV,
                                                      DISPATCH_SOURCE_CREATE_SUSPENDED,
                                                      machQueue,
                                                      ^(dispatch_source_t ds) {
                                                          if ( dispatch_source_get_error(ds, NULL) == 0 ) {
                                                              mach_msg_server( dsmig_demux_internaldispatch,
                                                                               maxSize,
                                                                               newServer,
                                                                               MY_MIG_OPTIONS );
                                                          } else {
                                                              mach_port_mod_refs( mach_task_self(), newServer, MACH_PORT_RIGHT_RECEIVE, -1 );
                                                          }
                                                      } );
        assert( machSource != NULL );
        
        mach_port_set_context( mach_task_self(), newServer, (mach_vm_address_t) machSource );
        
        // Request no-senders notification so we can tell when client dies
        mach_port_t	oldTargetOfNotification	= MACH_PORT_NULL;
        mach_port_request_notification( mach_task_self(), newServer, MACH_NOTIFY_NO_SENDERS, 1, newServer, 
                                        MACH_MSG_TYPE_MAKE_SEND_ONCE, &oldTargetOfNotification );
        
        dispatch_release( machQueue );
        
        // we only use the process source in localonly mode to track who is still around
        if ( gDSLocalOnlyMode == true && inPID > 0 ) {
            dispatch_source_t procSource = dispatch_source_proc_create( inPID,
                                                                        DISPATCH_PROC_EXIT | DISPATCH_PROC_EXEC, 
                                                                        NULL,
                                                                        dispatch_get_concurrent_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT),
                                                                        ^(dispatch_source_t ds) { 
                                                                            if ( dispatch_source_get_error(ds, NULL) == 0 ) {
                                                                                dsdispatch_process_wentaway( inPID );
                                                                                dispatch_cancel( ds );
                                                                            }
                                                                            else {
                                                                                dispatch_release( ds );
                                                                            }
                                                                        } );
            assert( procSource != NULL );
        }        
    }
    else {
        // our main launchd registered ports are always on the concurrent queue to ensure re-entrancy
        machQueue = dispatch_get_concurrent_queue( DISPATCH_QUEUE_PRIORITY_DEFAULT );
        
        dispatch_group_t migGroup = dispatch_group_create();
        machSource = dispatch_source_machport_create( newServer,
                                                      DISPATCH_MACHPORT_RECV,
                                                      DISPATCH_SOURCE_CREATE_SUSPENDED,
                                                      machQueue,
                                                      ^(dispatch_source_t ds) {
                                                          if ( dispatch_source_get_error(ds, NULL) == 0 ) {
                                                              dispatch_group_async( migGroup,
                                                                                    machQueue, 
                                                                                    ^(void) {
                                                                                        mach_msg_server( dsmig_demux_internaldispatch,
                                                                                                         maxSize,
                                                                                                         newServer,
                                                                                                         MY_MIG_OPTIONS );
                                                                                    } );
                                                          } else {
                                                              dispatch_group_wait( migGroup, UINT64_MAX );
                                                              dispatch_release( migGroup );
                                                              mach_port_mod_refs( mach_task_self(), newServer, MACH_PORT_RIGHT_RECEIVE, -1 );
                                                          }
                                                      } );
        assert( machSource != NULL );
    }
	
    dispatch_resume( machSource );
    
	return machSource;
}

kern_return_t dsmig_do_create_api_session( mach_port_t server, mach_port_t *newServer, audit_token_t *atoken )
{
	pid_t	aPID;
	
	// let's get the audit data PID
	audit_token_to_au32( *atoken, NULL, NULL, NULL, NULL, NULL, &aPID, NULL, NULL );
	mach_port_allocate( mach_task_self(), MACH_PORT_RIGHT_RECEIVE, newServer );
	
	CreateDispatchSourceForMachPort( *newServer, kMaxMIGMsg, aPID, true );
	
	if ( LoggingEnabled(kLogInfo) == true ) {
		char *pName = dsGetNameForProcessID( aPID );
		DbgLog( kLogDebug, "dsdispatch_create_api_session - created new dispatch source for Mach port: %u Client: '%s' PID: %d", 
			    *newServer, pName, aPID );
		DSFree( pName );
	}
	
	return KERN_SUCCESS;
}

kern_return_t dsmig_do_close_api_session( mach_port_t server, audit_token_t atoken )
{
	pid_t	aPID;
	
	// let's get the audit data PID
	audit_token_to_au32( atoken, NULL, NULL, NULL, NULL, NULL, &aPID, NULL, NULL );
	
	gRefTable.CleanRefsForMachRefs( server );
	
	if ( LoggingEnabled(kLogInfo) == true ) {
		char *pName = dsGetNameForProcessID( aPID );
		DbgLog( kLogDebug, "dsdispatch_close_api_session - cleaning up for port: %u Client: '%s' PID: %d", 
			    server, pName, aPID );
		DSFree( pName );
	}
	
	return KERN_SUCCESS;
}

kern_return_t dsmig_do_api_call( mach_port_t server,
								 mach_msg_type_name_t serverPoly,
								 sComDataPtr msg_data,
								 mach_msg_type_number_t msg_dataCnt,
								 vm_offset_t msg_data_ool,
								 mach_msg_type_number_t msg_data_oolCnt,
								 sComDataPtr reply_msg,
								 mach_msg_type_number_t *reply_msgCnt,
								 vm_offset_t *reply_msg_ool,
								 mach_msg_type_number_t *reply_msg_oolCnt,
								 audit_token_t atoken )
{
	kern_return_t	kr			= KERN_FAILURE;
	sComDataPtr		pComData	= NULL;
	UInt32			uiLength	= 0;
	UInt32			dataLength	= 0;
	UInt32			dataSize	= 0;
	
	if( msg_dataCnt )
	{
		pComData	= (sComDataPtr) msg_data;
		uiLength	= msg_dataCnt;
	}
	else
	{
		pComData	= (sComDataPtr) msg_data_ool;
		uiLength	= msg_data_oolCnt;
	}

	if ( pComData->type.msgt_translate == 0 )
	{
		dataLength	= pComData->fDataLength;
		dataSize	= pComData->fDataSize;
	}
	else
	{
		dataLength	= DSGetLong( &pComData->fDataLength, kDSSwapNetworkToHostOrder );
		dataSize	= DSGetLong( &pComData->fDataSize, kDSSwapNetworkToHostOrder );
	}
	
	// lets see if the packet is big enough.. and see if the length matches the msg_data size minus 1
	if( uiLength >= (sizeof(sComData) - 1) )
	{
		if( dataLength == (uiLength - (sizeof(sComData) - 1)) )
		{
			// we need to copy because we will allocate/deallocate it in the handler
			//   but based on the size it thinks it is
			sComData *pRequest = (sComData *) calloc( sizeof(sComData) + dataSize, 1 );
			if ( pRequest == NULL )
				return KERN_MEMORY_ERROR;
			
			CRequestHandler handler;
			double reqStartTime = 0;
			double reqEndTime = 0;
			
			bcopy( (void *)pComData, pRequest, uiLength );
			
			// need to swap if it wasn't sent in little endian
			if ( pRequest->type.msgt_translate != 0 ) {
				SwapMachMessage( pRequest, kDSSwapNetworkToHostOrder );
			}

			// need to populate the port
			pRequest->fMachPort = server;
			
			// let's get the audit data and add it to the sComData
			audit_token_to_au32( atoken, NULL, (uid_t *)&pRequest->fEffectiveUID, NULL, (uid_t *)&pRequest->fUID, NULL, (pid_t *)&pRequest->fPID, NULL, NULL );
			
			if ( (gDebugLogging) || (gLogAPICalls) )
			{
				reqStartTime = dsTimestamp();
			}
			
			handler.HandleRequest( &pRequest );
			
			if ( (gDebugLogging) || (gLogAPICalls) )
			{
				reqEndTime = dsTimestamp();

				// if the request took more than 2 seconds, log a message
				double totalTime = (reqEndTime - reqStartTime) / USEC_PER_SEC;
				if (totalTime > 2.0)
				{
					char *debugDataTag = handler.BuildAPICallDebugDataTag( NULL, pRequest->fPID, "API", "Server" );
					DbgLog( kLogHandler, "%s : dsmig DAR : Excessive request time %f seconds", debugDataTag, totalTime );
					free( debugDataTag );
				}
			}
			
			// set the PID in the return to our PID for RefTable purposes
			pRequest->fPID = gDaemonPID;
			
			UInt32 dataLen = pRequest->fDataLength;
			
			// need to swap if it wasn't sent in little endian
			if ( pRequest->type.msgt_translate != 0 ) {
				SwapMachMessage( pRequest, kDSSwapHostToNetworkOrder );
			}

			// if it will fit in the fixed buffer, use it otherwise use OOL
			if( sizeof(sComData) + dataLen <= *reply_msgCnt )
			{
				*reply_msgCnt = sizeof(sComData) + dataLen - 1;
				bcopy( pRequest, reply_msg, *reply_msgCnt );
				*reply_msg_oolCnt = 0;
			}
			else
			{
				*reply_msgCnt = 0; // ool, set the other to 0
				vm_read( mach_task_self(), (vm_address_t)pRequest, (sizeof(sComData) + dataLen - 1), reply_msg_ool, reply_msg_oolCnt );
			}

			// free our allocated request data...
			free( pRequest );
			pRequest = NULL;
			
			gAPICallCount++;
			
			if (gLogAPICalls)
			{
				if ( (gAPICallCount % 1023) == 1023 ) // every 1023 calls so we can do bit-wise check
				{
					syslog(LOG_CRIT,"API clients have called APIs %d times", gAPICallCount);
				}
			}

			kr = KERN_SUCCESS;
		}
		else
		{
			syslog( LOG_ALERT, "dsmig_do_api_call:  Bad message size %d, does not correlate with contents length %d + header %d", uiLength, dataLength, (sizeof(sComData) - 1) );
		}
	}
	else
	{
		syslog( LOG_ALERT, "dsmig_do_api_call message is too small to be valid message %d < %d", uiLength, sizeof(sComData) - 1 );
	}
	
	if( msg_data_oolCnt )
	{
		vm_deallocate( mach_task_self(), msg_data_ool, msg_data_oolCnt );
	}
	
	return kr;
}

kern_return_t libinfoDSmig_do_GetProcedureNumber
										(	mach_port_t server,
											char* indata,
											int *procnumber,
											audit_token_t atoken )
{
	kern_return_t	kr				= KERN_FAILURE;
	char			*debugDataTag	= NULL;

	// NOTE:  All MIG calls are Endian swapped automatically, nothing required on the client/server side
	
	if ( (gDebugLogging) || (gLogAPICalls) )
	{
		pid_t			aPID;
		CRequestHandler handler;
		
		audit_token_to_au32( atoken, NULL, NULL, NULL, NULL, NULL, &aPID, NULL, NULL );
		
		debugDataTag = handler.BuildAPICallDebugDataTag( NULL, aPID, "libinfo", "Server" );
		DbgLog( kLogHandler, "%s : libinfomig DAC : Procedure Request = %s", debugDataTag, indata );
	}
	
	*procnumber = 0;
	if (indata != NULL)
	{
		for (int idx = 1; idx < (int)kDSLUlastprocnum && lookupProcedures[idx] != NULL; idx++)
		{
			if ( 0 == strcmp(indata, lookupProcedures[idx]) )
			{
				*procnumber = idx;
				kr = KERN_SUCCESS;
				break;
			}
		}
	}

	if ( debugDataTag )
	{
		DbgLog( kLogHandler, "%s : libinfomig DAR : Procedure = %s (%d) : Result code = %d", debugDataTag, indata,
				*procnumber, kr );
		
		free( debugDataTag );
	}
	
	return kr;
}

kern_return_t libinfoDSmig_do_Query(	mach_port_t server,
										int32_t procnumber,
										inline_data_t request,
										mach_msg_type_number_t requestCnt,
										inline_data_t reply,
										mach_msg_type_number_t *replyCnt,
										vm_offset_t *ooreply,
										mach_msg_type_number_t *ooreplyCnt,
										audit_token_t atoken )
{
	kern_return_t	kr				= KERN_FAILURE;
	kvbuf_t		   *returnedBuf		= NULL;
	Boolean			bValidProcedure	= ( (procnumber > 0) && (procnumber < (int)kDSLUlastprocnum) ? TRUE : FALSE);
	char			*debugDataTag	= NULL;
    pid_t			aPID;
	
	// Note: MIG deals with all byte swapping automatically no need to swap int32_t or any other standard integer type
	// spawn a thread request
    audit_token_to_au32( atoken, NULL, NULL, NULL, NULL, NULL, &aPID, NULL, NULL );
    
	if ( (gDebugLogging) || (gLogAPICalls) )
	{
		CRequestHandler handler;

		if ( bValidProcedure )
		{
			debugDataTag = handler.BuildAPICallDebugDataTag( NULL, aPID, "libinfo", "Server" );
			DbgLog( kLogHandler, "%s : libinfomig DAC : Procedure = %s (%d)", debugDataTag, lookupProcedures[procnumber], procnumber );
            
            if( aPID == (SInt32)gDaemonPID )
            {
                DbgLog( kLogHandler, "%s : libinfomig DAC : Dispatching from/to ourself", debugDataTag, lookupProcedures[procnumber],
                        procnumber );
            }
		}
		else
		{
			debugDataTag = handler.BuildAPICallDebugDataTag( NULL, aPID, "libinfo", "Server" );
			DbgLog( kLogHandler, "%s : libinfomig DAC : Invalid Procedure = %d", debugDataTag, procnumber );
		}
	}
    
    // if the cache node is NULL and (pid != DS or DNS lookup) wait for cache node
#ifdef HANDLE_DNS_LOOKUPS
    if( gCacheNode == NULL && (aPID != (SInt32)gDaemonPID || (procnumber >= kDSLUgetaddrinfo && procnumber <= kDSLUdns_proxy )) )
    {
        gKickCacheRequests.WaitForEvent(); // wait until cache node is ready
    }
#endif
	
	//pass the data directly into the Cache plugin and only if the SearchNode is initialized
    // or the search node is not initialized and we aren't dispatching to ourselves
	if ( gCacheNode != NULL && bValidProcedure )
	{
		struct stat sb;
		
		if ( procnumber == kDSLUflushcache && lstat("/AppleInternal", &sb) == 0 && lstat("/var/db/disableAppleInternal", &sb) == -1 ) {
			char *pName = dsGetNameForProcessID( aPID );
			syslog( LOG_ERR, "***Mobility: PID: %d '%s' requested flush of libinfo cache - this could affect sleep/wake/etc.", aPID, 
				    pName );
			DSFree( pName );
		}
		
        returnedBuf = gCacheNode->ProcessLookupRequest(procnumber, request, requestCnt, aPID);
        if ( (returnedBuf != NULL) && (returnedBuf->databuf != NULL) )
        {
            // if it will fit in the fixed buffer, use it otherwise use OOL
            if( returnedBuf->datalen <= *replyCnt )
            {
                *replyCnt = returnedBuf->datalen;
                bcopy( returnedBuf->databuf, reply, returnedBuf->datalen );
                *ooreplyCnt = 0; // inline set OOL to 0
            }
            else
            {
                vm_read( mach_task_self(), (vm_address_t)(returnedBuf->databuf), returnedBuf->datalen, ooreply, 
                         ooreplyCnt );
                *replyCnt = 0; // ool, set the other to 0
            }
        }
        else
        {
            *replyCnt = 0;
            *ooreplyCnt = 0;
        }
        
		kr = KERN_SUCCESS;
		
        kvbuf_free(returnedBuf);
	}

	if ( debugDataTag )
	{
		if ( bValidProcedure )
		{
			DbgLog( kLogHandler, "%s : libinfomig DAR : Procedure = %s (%d) : Result code = %d", debugDataTag, 
					lookupProcedures[procnumber], procnumber, kr );
		}
		else
		{
			DbgLog( kLogHandler, "%s : libinfomig DAR : Invalid Procedure = %d", debugDataTag, procnumber );
		}

		free( debugDataTag );
	}

	return kr;
}

kern_return_t libinfoDSmig_do_Query_async(	mach_port_t server,
                                            mach_port_t replyToPort,
                                            int32_t procnumber,
                                            inline_data_t request,
                                            mach_msg_type_number_t requestCnt,
                                            mach_vm_address_t callbackAddr,
                                            audit_token_t atoken )
{
    Boolean					bValidProcedure	= ( (procnumber > 0) && (procnumber < (int)kDSLUlastprocnum) ? TRUE : FALSE);
    char					*debugDataTag	= NULL;
    __block sLibinfoRequest	*pLibinfoRequest = NULL;
    
    if ( (gDebugLogging) || (gLogAPICalls) )
    {
        pid_t			aPID;
        CRequestHandler handler;
        
        audit_token_to_au32( atoken, NULL, NULL, NULL, NULL, NULL, &aPID, NULL, NULL );
        
        if ( bValidProcedure )
        {
            debugDataTag = handler.BuildAPICallDebugDataTag( NULL, aPID, "libinfo", "Server" );
            DbgLog( kLogHandler, "%s : libinfomig DAC : Async Procedure = %s (%d)", debugDataTag, lookupProcedures[procnumber], procnumber );
            
            if( aPID == (pid_t)gDaemonPID )
            {
                DbgLog( kLogHandler, "%s : libinfomig DAC : Dispatching from/to ourself", debugDataTag, lookupProcedures[procnumber],
                        procnumber );
            }
        }
        else
        {
            debugDataTag = handler.BuildAPICallDebugDataTag( NULL, aPID, "libinfo", "Server" );
            DbgLog( kLogHandler, "%s : libinfomig DAC : Invalid Async Procedure = %d", debugDataTag, procnumber );
        }
    }
    
    if( bValidProcedure )
    {
        pLibinfoRequest = new sLibinfoRequest;
        if ( pLibinfoRequest != NULL )
		{
			pLibinfoRequest->fBuffer = (char *) calloc( requestCnt, sizeof(char) );
			if ( pLibinfoRequest->fBuffer != NULL )
			{
				pLibinfoRequest->fReplyPort = replyToPort;
				pLibinfoRequest->fProcedure = procnumber;
				pLibinfoRequest->fToken = atoken;
				pLibinfoRequest->fCallbackAddr = callbackAddr;
				
				bcopy( request, pLibinfoRequest->fBuffer, requestCnt );
				pLibinfoRequest->fBufferLen = requestCnt;
				
				void (^theBlock)(void) = ^(void) {
					char			*debugDataTag = NULL;
					pid_t			aPID;
					CRequestHandler handler;
					
					audit_token_to_au32( pLibinfoRequest->fToken, NULL, NULL, NULL, NULL, NULL, &aPID, NULL, NULL );
					
					if ( (gDebugLogging) || (gLogAPICalls) )
					{
						debugDataTag = handler.BuildAPICallDebugDataTag( NULL, aPID, "libinfo", "Server" );
						DbgLog( kLogHandler, "%s : libinfomig DAC : Async Procedure = %s (%d) : Handle request %X", debugDataTag, lookupProcedures[pLibinfoRequest->fProcedure], pLibinfoRequest->fProcedure, pLibinfoRequest );
					}                    
					
					// now process the request
					kvbuf_t *pResponse = gCacheNode->ProcessLookupRequest( pLibinfoRequest->fProcedure, pLibinfoRequest->fBuffer, pLibinfoRequest->fBufferLen, aPID );
					
					if ( pResponse != NULL )
					{
						if( pResponse->datalen <= MAX_MIG_INLINE_DATA )
						{
							if ( libinfoDSmig_Response_async( pLibinfoRequest->fReplyPort, pResponse->databuf, pResponse->datalen, 0, 0, pLibinfoRequest->fCallbackAddr ) != MACH_MSG_SUCCESS )
							{
								// need to clean up mach port if we got any error, we can't recover
								mach_port_destroy( mach_task_self(), pLibinfoRequest->fReplyPort );
							}
						}
						else
						{
							vm_offset_t data = 0;
							mach_msg_type_number_t dataCnt = 0;
							
							vm_read( mach_task_self(), (vm_address_t) pResponse->databuf, pResponse->datalen, &data, &dataCnt );
							
							if ( libinfoDSmig_Response_async( pLibinfoRequest->fReplyPort, (char *)"", 0, data, dataCnt, pLibinfoRequest->fCallbackAddr ) != MACH_MSG_SUCCESS )
							{
								// if we get any error, then we need to clean up the vm_dealloc
								vm_deallocate( mach_task_self(), data, dataCnt );
								mach_port_destroy( mach_task_self(), pLibinfoRequest->fReplyPort );
							}
						}
					}
					else
					{
						if ( libinfoDSmig_Response_async( pLibinfoRequest->fReplyPort, (char *)"", 0, 0, 0, pLibinfoRequest->fCallbackAddr ) != MACH_MSG_SUCCESS )
						{
							// need to clean up mach port if we got any error, we can't recover
							mach_port_destroy( mach_task_self(), pLibinfoRequest->fReplyPort );
						}
					}
					
					kvbuf_free( pResponse );
					pResponse = NULL;
					
					if ( debugDataTag )
					{
						DbgLog( kLogHandler, "%s : libinfomig DAR : Async Procedure = %s (%d)", debugDataTag, lookupProcedures[pLibinfoRequest->fProcedure], pLibinfoRequest->fProcedure );
						DSFree( debugDataTag );
					}
					
					DSFree( pLibinfoRequest->fBuffer );
					DSDelete( pLibinfoRequest );
				};
				
				dispatch_async( gLibinfoQueue, theBlock );
			}
			else
			{
				DSDelete( pLibinfoRequest );
			}
		}
		
		// need to dispose of rights if we aren't going to queue the message
		if ( pLibinfoRequest == NULL ) {
			mach_port_mod_refs( mach_task_self(), replyToPort, MACH_PORT_RIGHT_SEND_ONCE, -1 );
		}
    }
    
    if ( debugDataTag )
    {
        if ( bValidProcedure )
        {
            if( pLibinfoRequest != NULL )
            {
                DbgLog( kLogHandler, "%s : libinfomig DAR : Async Procedure = %s (%d) : Request %X queued", debugDataTag, 
                        lookupProcedures[procnumber], procnumber, pLibinfoRequest );
            }
            else
            {
                DbgLog( kLogHandler, "%s : libinfomig DAR : Async Procedure = %s (%d) : Request not queued", debugDataTag, 
                        lookupProcedures[procnumber], procnumber );
            }
        }
        else
        {
            DbgLog( kLogHandler, "%s : libinfomig DAR : Invalid Procedure = %d", debugDataTag, procnumber );
        }
        
        free( debugDataTag );
    }
    
    return KERN_SUCCESS;
}

kern_return_t memberdDSmig_do_MembershipCall( mach_port_t server, kauth_identity_extlookup *request, audit_token_t *atoken )
{
	char			*debugDataTag	= NULL;
	
	// mig calls all use seqno as a byte order field, so check if we need to swap
	int needsSwap = (request->el_seqno != 1) && (request->el_seqno == ntohl(1));
	
	// Note: MIG deals with all byte swapping automatically no need to swap int32_t or any other standard integer type
	// if defined that way BUT request is uint_8 array
	if (needsSwap)
		Mbrd_SwapRequest(request);

	if ( (gDebugLogging) || (gLogAPICalls) )
	{
		CRequestHandler handler;
		pid_t			aPID;
		
		audit_token_to_au32( *atoken, NULL, NULL, NULL, NULL, NULL, &aPID, NULL, NULL );
		
		debugDataTag = handler.BuildAPICallDebugDataTag( NULL, aPID, "MembershipCall", "Server" );
		DbgLog( kLogHandler, "%s : mbr_mig DAC %s", debugDataTag, (needsSwap ? " : Via Rosetta" : "") );
		
		if( aPID == (SInt32)gDaemonPID )
		{
			DbgLog( kLogHandler, "%s : mbr_mig DAC : Dispatching from/to ourself", debugDataTag );
		}
	}
	
	Mbrd_ProcessLookup( request );

	if (needsSwap)
		Mbrd_SwapRequest(request);

	if ( debugDataTag )
	{
		DbgLog( kLogHandler, "%s : mbr_mig DAR", debugDataTag );
		free( debugDataTag );
	}

	return KERN_SUCCESS;
}

kern_return_t memberdDSmig_do_GetStats(mach_port_t server, StatBlock *stats)
{
	Mbrd_ProcessGetStats( stats );
	
	return KERN_SUCCESS;
}

kern_return_t memberdDSmig_do_ClearStats(mach_port_t server, audit_token_t atoken)
{
	char *debugDataTag = NULL;

	if ( (gDebugLogging) || (gLogAPICalls) )
	{
		CRequestHandler handler;
		pid_t			aPID;
		
		audit_token_to_au32( atoken, NULL, NULL, NULL, NULL, NULL, &aPID, NULL, NULL );
		
		debugDataTag = handler.BuildAPICallDebugDataTag( NULL, aPID, "ClearStats", "Server" );
		DbgLog( kLogHandler, "%s : mbr_mig DAC", debugDataTag );
	}
	
	Mbrd_ProcessResetStats();
	
	if ( debugDataTag )
	{
		DbgLog( kLogHandler, "%s : mbr_mig DAR", debugDataTag );
		free( debugDataTag );
	}
	
	return KERN_SUCCESS;
}

kern_return_t memberdDSmig_do_MapName(mach_port_t server, uint8_t isUser, mstring name, guid_t *guid, audit_token_t *atoken)
{
	kern_return_t	result;
	char			*debugDataTag	= NULL;
	
	if ( (gDebugLogging) || (gLogAPICalls) )
	{
		CRequestHandler handler;
		pid_t			aPID;
		
		audit_token_to_au32( *atoken, NULL, NULL, NULL, NULL, NULL, &aPID, NULL, NULL );
		
		debugDataTag = handler.BuildAPICallDebugDataTag( NULL, aPID, "MapName", "Server" );
		DbgLog( kLogHandler, "%s : mbr_mig DAC : Name = %s : isUser = %d", debugDataTag, name, (int) isUser );
	}
	
	// note the length is not required, it's not checked for c-strings
	result = Mbrd_ProcessMapIdentifier( isUser ? ID_TYPE_USERNAME : ID_TYPE_GROUPNAME, name, -1, guid );

	if ( debugDataTag )
	{
		if (result == KERN_SUCCESS)
		{
			char	uuid_string[37] = { 0, };
			
			uuid_unparse_upper( guid->g_guid, uuid_string );
			DbgLog( kLogHandler, "%s : mbr_mig DAR : Name = %s : GUID = %s", debugDataTag, name, uuid_string );
		}
		else
		{
			DbgLog( kLogHandler, "%s : mbr_mig DAR : Name = %s : Not found", debugDataTag, name );
		}
		
		free( debugDataTag );
	}	
	
	return result;
}

kern_return_t memberdDSmig_do_MapIdentifier(mach_port_t server, int idType, 
											vm_offset_t identifier, mach_msg_type_number_t identifierCnt,
											vm_offset_t ooidentifier, mach_msg_type_number_t ooidentifierCnt,
											guid_t *guid, audit_token_t *atoken)
{
	kern_return_t	result;
	char			*debugDataTag	= NULL;
	void			*idValue		= (void *) (identifierCnt ? identifier : ooidentifier);
	size_t			idLength		= (identifierCnt ? : ooidentifierCnt);
	
	if ( (gDebugLogging) || (gLogAPICalls) )
	{
		CRequestHandler handler;
		pid_t			aPID;
		
		audit_token_to_au32( *atoken, NULL, NULL, NULL, NULL, NULL, &aPID, NULL, NULL );
		
		debugDataTag = handler.BuildAPICallDebugDataTag( NULL, aPID, "MapIdentifier", "Server" );
		
		switch ( idType ) {
			case ID_TYPE_UID:
			case ID_TYPE_GID:
				DbgLog( kLogHandler, "%s : mbr_mig DAC : ID Type = %d, value = %d", debugDataTag, idType, *((id_t *) idValue) );
				break;
				
			case ID_TYPE_USERNAME:
			case ID_TYPE_GROUPNAME:
			case ID_TYPE_X509_DN:
			case ID_TYPE_KERBEROS:
				DbgLog( kLogHandler, "%s : mbr_mig DAC : ID Type = %d, value = %s", debugDataTag, idType, idValue );
				break;
				
			case ID_TYPE_GSS_EXPORT_NAME:
			default:
				DbgLog( kLogHandler, "%s : mbr_mig DAC : ID Type = %d", debugDataTag, idType );
				break;
		}
	}
	
	result = Mbrd_ProcessMapIdentifier( idType, idValue, idLength, guid );
	
	if ( debugDataTag )
	{
		if (result == KERN_SUCCESS)
		{
			if ( (gDebugLogging) || (gLogAPICalls) )
			{
				char	uuid_string[37] = { 0, };
				
				uuid_unparse_upper( guid->g_guid, uuid_string );
				DbgLog( kLogHandler, "%s : mbr_mig DAR : GUID = %s", debugDataTag, uuid_string );
			}
		}
		else
		{
			DbgLog( kLogHandler, "%s : mbr_mig DAR : Not found", debugDataTag );
		}
		
		free( debugDataTag );
	}	
	
	return result;
}

kern_return_t memberdDSmig_do_GetGroups(mach_port_t server, uint32_t uid, uint32_t* numGroups, GIDArray gids, audit_token_t *atoken)
{
	int		result;
	char	*debugDataTag = NULL;

	if ( (gDebugLogging) || (gLogAPICalls) )
	{
		CRequestHandler handler;
		pid_t			aPID;
		
		audit_token_to_au32( *atoken, NULL, NULL, NULL, NULL, NULL, &aPID, NULL, NULL );
		
		debugDataTag = handler.BuildAPICallDebugDataTag( NULL, aPID, "GetGroups", "Server" );
		DbgLog( kLogHandler, "%s : mbr_mig DAC : uid = %u", debugDataTag, uid );
	}

	result = Mbrd_ProcessGetGroups(uid, numGroups, gids);

	if ( debugDataTag )
	{
		DbgLog( kLogHandler, "%s : mbr_mig DAR : Total groups = %u", debugDataTag, (*numGroups) );
		free( debugDataTag );
	}	
	
	return (kern_return_t)result;
}

kern_return_t memberdDSmig_do_GetAllGroups(mach_port_t server, uint32_t uid, uint32_t* numGroups, GIDList *gids, mach_msg_type_number_t *gidsCnt, 
										   audit_token_t *atoken)
{
	int		result;
	char	*debugDataTag = NULL;
	
	if ( (gDebugLogging) || (gLogAPICalls) )
	{
		CRequestHandler handler;
		pid_t			aPID;
		
		audit_token_to_au32( *atoken, NULL, NULL, NULL, NULL, NULL, &aPID, NULL, NULL );
		
		debugDataTag = handler.BuildAPICallDebugDataTag( NULL, aPID, "UserGroup_GetAllGroups", "Server" );
		DbgLog( kLogHandler, "%s : mbr_mig DAC : uid = %u", debugDataTag, uid );
	}
	
	GIDList tempList = NULL;
	
	(*gids) = NULL;
	(*gidsCnt) = 0;
    
	result = Mbrd_ProcessGetAllGroups(uid, numGroups, &tempList);
	if ( result == KERN_SUCCESS && (*numGroups) > 0 && tempList != NULL )
	{
		result = vm_read( mach_task_self(), (vm_address_t) tempList, ((*numGroups) * sizeof(gid_t)), (vm_offset_t *) gids, gidsCnt );
		if ( result == KERN_SUCCESS )
		{
			// need to set the value to count not length of bytes
			(*gidsCnt) = (*numGroups);
		}
		
		DSFree( tempList );
	}
	
	if ( debugDataTag )
	{
		DbgLog( kLogHandler, "%s : mbr_mig DAR : Total groups = %u", debugDataTag, (*numGroups) );
		free( debugDataTag );
	}	
	
	return (kern_return_t)result;
}

kern_return_t memberdDSmig_do_ClearCache(mach_port_t server, audit_token_t atoken)
{
	char		*debugDataTag = NULL;
	struct stat	sb;
	
	if ( gDebugLogging == true || 
		 gLogAPICalls == true || 
		 (lstat("/AppleInternal", &sb) == 0 && lstat("/var/db/disableAppleInternal", &sb) == -1) )
	{
		pid_t	aPID;
		
		audit_token_to_au32( atoken, NULL, NULL, NULL, NULL, NULL, &aPID, NULL, NULL );
		
		if ( gDebugLogging == true || gLogAPICalls == true )
		{
			CRequestHandler handler;
			
			debugDataTag = handler.BuildAPICallDebugDataTag( NULL, aPID, "ClearCache", "Server" );
			DbgLog( kLogHandler, "%s : mbr_mig DAC", debugDataTag );
		}
		else
		{
			char *pName = dsGetNameForProcessID( aPID );
			syslog( LOG_ERR, "***Mobility: PID: %d '%s' requested flush of membership cache - this could affect sleep/wake/etc.", aPID, 
				    pName );
			DSFree( pName );
		}
	}

	Mbrd_ProcessResetCache();
	
	if ( debugDataTag )
	{
		DbgLog( kLogHandler, "%s : mbr_mig DAR", debugDataTag );
		free( debugDataTag );
	}		

	return KERN_SUCCESS;
}

kern_return_t memberdDSmig_do_DumpState(mach_port_t server, audit_token_t atoken)
{
	char	*debugDataTag = NULL;
	
	if ( (gDebugLogging) || (gLogAPICalls) )
	{
		CRequestHandler handler;
		pid_t			aPID;
		
		audit_token_to_au32( atoken, NULL, NULL, NULL, NULL, NULL, &aPID, NULL, NULL );
		
		debugDataTag = handler.BuildAPICallDebugDataTag( NULL, aPID, "DumpState", "Server" );
		DbgLog( kLogHandler, "%s : mbr_mig DAC", debugDataTag );
	}
    
	Mbrd_ProcessDumpState();

	if ( debugDataTag )
	{
		DbgLog( kLogHandler, "%s : mbr_mig DAR", debugDataTag );
		free( debugDataTag );
	}		
	
	return KERN_SUCCESS;
}

#pragma mark -
#pragma mark ServerControl Routines
#pragma mark -

// ---------------------------------------------------------------------------
//	* ServerControl()
//
// ---------------------------------------------------------------------------

ServerControl::ServerControl ( void )
{
    gDaemonPID			= getpid();
	//we use a non valid IP Address of zero for ourselves
	gDaemonIPAddress	= 0;

	fTCPHandlerThreadsCnt		= 0;
	fSCDStore					= 0;	
	fPerformanceStatGatheringActive	= false; //default
	fMemberDaemonFlushCacheRequestCount	= 0;
	
#ifdef BUILD_IN_PERFORMANCE
	fLastPluginCalled			= 0;
	fPerfTableNumPlugins		= 0;
	fPerfTable					= nil;
	
#if PERFORMANCE_STATS_ALWAYS_ON
	fPerformanceStatGatheringActive	= true;
#else
	fPerformanceStatGatheringActive	= false;
#endif
#endif

	if (gDSDebugMode)
	{
		fServiceNameString = CFStringCreateWithCString( NULL, kDSStdMachDebugPortName, kCFStringEncodingUTF8 );
	}
	else if (gDSLocalOnlyMode)
	{
		fServiceNameString = CFStringCreateWithCString( NULL, kDSStdMachLocalPortName, kCFStringEncodingUTF8 );
	}
	else
	{
		fServiceNameString = CFStringCreateWithCString( NULL, kDSStdMachPortName, kCFStringEncodingUTF8 );
	}
	
	if (gDaemonPID > 100) //assumption here is that we crashed and restarted so say netowrk is already running ie. we are usually less than 50
	{
		gFirstNetworkUpAtBoot = true;
	}
    
} // ServerControl


// ---------------------------------------------------------------------------
//	* ~ServerControl()
//
// ---------------------------------------------------------------------------

ServerControl::~ServerControl ( void )
{
} // ~ServerControl


// ----------------------------------------------------------------------------
//	* RefDeallocProc()
//    used to clean up plug-in specific data for a reference
// ----------------------------------------------------------------------------

SInt32 ServerControl::RefDeallocProc ( UInt32 inRefNum, eRefType inRefType, CServerPlugin *inPluginPtr )
{
	SInt32	dsResult	= eDSNoErr;
	double	inTime		= 0;
	double	outTime		= 0;
	
	if (inPluginPtr != nil)
	{
		// we should call the plug-in to clean up its table
		sCloseDirNode closeData;
		closeData.fResult = eDSNoErr;
		closeData.fInNodeRef = inRefNum;
		switch (inRefType)
		{
			case eRefTypeDirNode:
				closeData.fType = kCloseDirNode;
				break;
				
			case eRefTypeRecord:
				closeData.fType = kCloseRecord;
				break;
				
			case eRefTypeAttributeList:
				closeData.fType = kCloseAttributeList;
				break;
				
			case eRefTypeAttributeValueList:
				closeData.fType = kCloseAttributeValueList;
				break;
				
			default:
				closeData.fType = 0;
				break;
		}
		
		if (closeData.fType != 0)
		{
			if (gLogAPICalls)
			{
				inTime = dsTimestamp();
			}
			inPluginPtr->ProcessRequest( &closeData );
			dsResult = closeData.fResult;
			if (gLogAPICalls)
			{
				outTime = dsTimestamp();
				syslog(LOG_CRIT,"Ref table dealloc callback, API Call: %s, PlugIn Used: %s, Result: %d, Duration: %.2f usec",
					   CRequestHandler::GetCallName( closeData.fType ), inPluginPtr->GetPluginName(), dsResult,
					   (outTime - inTime));
			}
		}
	}
	
	return( dsResult );
} // RefDeallocProc

static void __StartKernelListener( void )
{
	static CRequestHandler handler;
	
	dispatch_async( dispatch_get_concurrent_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT), 
				    ^(void) {
						kern_return_t kresult = syscall( SYS_identitysvc, KAUTH_EXTLOOKUP_REGISTER, 0 );
						if ( kresult == KERN_SUCCESS )
						{
							SrvrLog( kLogApplication, "Registered for Kernel identity service requests" );

							do
							{
								kauth_identity_extlookup *request = (kauth_identity_extlookup *) calloc( 1, sizeof(kauth_identity_extlookup) );
							   
								kresult = syscall( SYS_identitysvc, KAUTH_EXTLOOKUP_WORKER, request );
								if ( kresult == KERN_SUCCESS )
								{
#if 0
									// 6449641
									if ( (gDebugLogging) || (gLogAPICalls) ) {
										char *debugDataTag = handler.BuildAPICallDebugDataTag( NULL, request->el_info_pid, 
																							   "mbr_syscall", "Server" );
										DbgLog( kLogAPICalls, "%s : dequeue kauth request %X", debugDataTag, request );
									}
#endif
									
									dispatch_async( dispatch_get_concurrent_queue(DISPATCH_QUEUE_PRIORITY_HIGH), 
												    ^(void) {
														char *debugDataTag = NULL;
														
														if ( (gDebugLogging) || (gLogAPICalls) ) {
															debugDataTag = handler.BuildAPICallDebugDataTag( NULL, request->el_info_pid, 
																											 "mbr_syscall", "Server" );
															DbgLog( kLogAPICalls, "%s : process kauth result %X", debugDataTag, request );
														}
														
														CInternalDispatch::AddCapability();
														request->el_flags |= kKernelRequest;
														Mbrd_ProcessLookup( request );
														request->el_flags &= ~kKernelRequest;
														
														kern_return_t result = syscall( SYS_identitysvc, KAUTH_EXTLOOKUP_RESULT, request );
														if ( debugDataTag != NULL ) {
															if ( result == KERN_SUCCESS ) {
																DbgLog( kLogAPICalls, "%s : delivered kauth result %X", debugDataTag, request );
															}
															else {
																DbgLog( kLogAPICalls, "%s : failed to deliver kauth result %X - %d", 
																	    debugDataTag, request, result );
															}
															
															free( debugDataTag );
														}
														
														free( request );
													} );
									request = NULL;
								}
								else
								{
									syslog( LOG_ERR, "Fatal error %d from KAUTH_EXTLOOKUP_WORKER setup (%d: %s)", kresult, errno, strerror(errno) );
									DSFree( request );
								}
							} while ( kresult == KERN_SUCCESS );
						}
						else {
							syslog( LOG_ERR, "Got error %d trying to register with kernel", kresult );
						}
					   
						syscall( SYS_identitysvc, KAUTH_EXTLOOKUP_DEREGISTER, 0 );
					} );
}

void ServerControl::StartKernelListener( void )
{
	__StartKernelListener(); // 6453258
}

SInt32 ServerControl::StartUpServer ( void )
{
	SInt32				result		= eDSNoErr;
	struct stat			statResult;
	try
	{
		if ( gNodeList == nil )
		{
			gNodeList = new CNodeList();
			if ( gNodeList == nil ) throw((SInt32)eMemoryAllocError);
		}

		// Let's initialize membership globals just in case we get called
		Mbrd_InitializeGlobals();

		if ( gPluginConfig == nil )
		{
			gPluginConfig = new CPluginConfig();
			if ( gPluginConfig == nil ) throw( (SInt32)eMemoryAllocError );
			gPluginConfig->Initialize();
		}
		
		if ( gPlugins == nil )
		{
			gPlugins = new CPlugInList();
			if ( gPlugins == nil ) throw( (SInt32)eMemoryAllocError );
			gPlugins->ReadRecordTypeRestrictions();
		}

		if ( gLibinfoQueue == NULL )
		{
			gLibinfoQueue = dispatch_queue_create( "async_libinfo", NULL );
		}
		
		CreateDebugPrefFileIfNecessary();
		
		bool bOnce = false;
		if ( lstat("/Library/Preferences/DirectoryService/.DSLogAPIAtStartOnce", &statResult) == 0 ) {
			bOnce = true;
			unlink( "/Library/Preferences/DirectoryService/.DSLogAPIAtStartOnce" );
		}

		if ( bOnce == true || lstat("/Library/Preferences/DirectoryService/.DSLogAPIAtStart", &statResult) == 0)
		{
			if ( __sync_bool_compare_and_swap(&gLogAPICalls, false, true) == true ) {
				ToggleAPILogging( false );
				syslog( LOG_ALERT, "%s%s", "Logging of API Calls turned ON at Startup of DS Daemon", 
						(bOnce == true ? " (once)." : ".") );
			}
		}

		if ( lstat("/Library/Preferences/DirectoryService/.DSLogDebugAtStartOnce", &statResult) == 0 ) {
			bOnce = true;
			unlink( "/Library/Preferences/DirectoryService/.DSLogDebugAtStartOnce" );
		}
		else {
			bOnce = false;
		}
		
		// let's start the MIG listeners
		if ( lstat("/Library/Preferences/DirectoryService/.DSLogDebugAtStart", &statResult) == 0 )
		{
			if ( __sync_bool_compare_and_swap(&gDebugLogging, false, true) == true ) {
				ResetDebugging(); //ignore return status
				syslog( LOG_ALERT, "%s%s", "Debug Logging turned ON at Startup of DS Daemon",
						(bOnce == true ? " (once)." : ".") );
			}
		}
		
		if (!gDSLocalOnlyMode && !gDSInstallDaemonMode && !gDSDebugMode)
		{
			// see if we need TCP too
			if ( 	(	(::stat( "/Library/Preferences/DirectoryService/.DSTCPListening", &statResult ) == eDSNoErr) ||
						(gServerOS) ) &&
						(::stat( "/Library/Preferences/DirectoryService/.DSTCPNotListening", &statResult ) != eDSNoErr) )
			{
				// Start the TCP listener thread
				result = StartTCPListener(kDSDefaultListenPort);
				if ( result != eDSNoErr ) throw( result );
			}
		}

		if ( gPluginHandler == nil )
		{
			gPluginHandler = new CPluginHandler();
			if ( gPluginHandler == nil ) throw((SInt32)eMemoryAllocError);

			//this call could throw
			gPluginHandler->StartThread();
		}

		result = RegisterForSystemPower();
		if ( result != eDSNoErr ) throw( result );

		result = (SInt32)RegisterForNetworkChange();
		if ( result != eDSNoErr ) throw( result );
		
		dispatch_source_t source = dispatch_source_timer_create( DISPATCH_TIMER_INTERVAL, 
																 30ULL * NSEC_PER_SEC,
																 1ULL * NSEC_PER_SEC,
																 NULL,
																 dispatch_get_concurrent_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT),
																 ^(dispatch_source_t ds) {
																	 // we intentionally ignore errors, etc, this is never released or cancelled
																	 ServerControl::DoPeriodicTask( ds );
																 } );
		if ( source == NULL ) result = eMemoryAllocError;

		// let's start the MIG listeners
		dispatch_queue_t concurrentQueue = dispatch_get_concurrent_queue( DISPATCH_QUEUE_PRIORITY_DEFAULT );
		dispatch_async( concurrentQueue, 
                        ^(void) {
                            // if not localonly mode we wait for the authentication policy before bringing up listeners
                            if ( gDSLocalOnlyMode == false ) {
                                gNodeList->WaitForAuthenticationSearchNode();
                                gNodeList->WaitForLocalNode();
                                gNodeList->WaitForCacheNode();
                            }
                            else {
                                // for localonly we always wait for the local node since that's all we care about
                                gNodeList->WaitForLocalNode();
                            }
                            
                            if ( gAPIMachPort != MACH_PORT_NULL ) {
                                gAPIDispatchSource = CreateDispatchSourceForMachPort( gAPIMachPort, kMaxMIGMsg, 0, false );
                                SrvrLog( kLogApplication, "Listening for DirectoryService API mach messages" );
                                DbgLog( kLogDebug, "Created mig source for API calls" );
                            }
                            
                            // if libinfo port or membership port, initialize membership as a safety
                            // TODO: stop using shared demux routine because API port is the only listener that needs it
                            if ( gLibinfoMachPort != MACH_PORT_NULL || gMembershipMachPort != MACH_PORT_NULL ) {
                                CInternalDispatch::AddCapability();
                                Mbrd_Initialize();
                            }
                           
                            if ( gLibinfoMachPort != MACH_PORT_NULL ) {
                                gLibinfoDispatchSource = CreateDispatchSourceForMachPort( gLibinfoMachPort, kMaxMIGMsg, 0, false );
                                SrvrLog( kLogApplication, "Listening for Libinfo API mach messages" );
                                DbgLog( kLogDebug, "Created mig source for Libinfo calls" );
                            }
                           
                            if ( gMembershipMachPort != MACH_PORT_NULL ) {
                                // we don't enable kernel listener for localonly or debug mode
                                // only the official daemon can answer the kernel
                                if ( gDSLocalOnlyMode == false && gDSDebugMode == false ) {
                                    StartKernelListener();
                                }             
                                
                                // let's sweep the membership cache every 15 minutes to remove expired entries
                                // the validation stuff will take care of expired entries if they are touched
                                // this sweep is to remove stale entries
                                dispatch_source_t ::mbrSweep = dispatch_source_timer_create( DISPATCH_TIMER_INTERVAL, 
                                                                                             900ULL * NSEC_PER_SEC,
                                                                                             15ULL * NSEC_PER_SEC,
                                                                                             NULL,
                                                                                             concurrentQueue,
                                                                                             ^(dispatch_source_t) {
                                                                                                 // we intentionally ignore errors
                                                                                                 // this is never released or cancelled
                                                                                                 Mbrd_SweepCache();
                                                                                             } );
                                assert( mbrSweep != NULL );
                                
                                gMembershipDispatchSource = CreateDispatchSourceForMachPort( gMembershipMachPort, kMaxMIGMsg, 0, false );
                                SrvrLog( kLogApplication, "Listening for Membership API mach messages" );
                                DbgLog( kLogDebug, "Created mig source for Membership calls" );
                            }
                        } );
	}

	catch( SInt32 err )
	{
		result = err;
	}

	return( result );

} // StartUpServer


// ---------------------------------------------------------------------------
//	* ShutDownServer ()
//
// ---------------------------------------------------------------------------

SInt32 ServerControl::ShutDownServer ( void )
{
	SInt32				result		= eDSNoErr;

	try
	{

		result = (SInt32)UnRegisterForNetworkChange();
		if ( result != eDSNoErr ) throw( result );

//		result = UnRegisterForSystemPower();
//		if ( result != eDSNoErr ) throw( result );

		// we just NULL the ports because the source owns the reference
		gAPIMachPort = MACH_PORT_NULL;
		gLibinfoMachPort = MACH_PORT_NULL;
		gMembershipMachPort = MACH_PORT_NULL;
		
        if ( gLibinfoDispatchSource ) {
            dispatch_cancel( gLibinfoDispatchSource );
            dispatch_release( gLibinfoDispatchSource );
        }
        
        if ( gAPIDispatchSource ) {
            dispatch_cancel( gAPIDispatchSource );
            dispatch_release( gAPIDispatchSource );
        }
        
        if ( gMembershipDispatchSource ) {
            dispatch_cancel( gMembershipDispatchSource );
            dispatch_release( gMembershipDispatchSource );
        }
		
		// we are in a syscall, so we must just deregister and error out from there
		if ( gDSLocalOnlyMode == false && gDSDebugMode == false) {
			if ( syscall(SYS_identitysvc, KAUTH_EXTLOOKUP_DEREGISTER, 0) == 0 ) {
				SrvrLog( kLogApplication, "Deregistered Kernel identity service" );
			}
			else {
				DbgLog( kLogError, "Failed to deregister Kernel identity service" );
			}
		}
		
		//no need to delete the global objects as this process is going away and
		//we don't want to create a race condition on the threads dying that
		//could lead to a crash

		//no need to delete the global mutexes as this process is going away and
		//we don't want to create a reace condition on the threads dying that
		//could lead to a crash
		//delete(gTCPHandlerLock);
		//delete(gHandlerLock);
		//delete(gInternalHandlerLock);
		//delete(gCheckpwHandlerLock);
		//delete(gPerformanceLoggingLock);
		
		CLog::Deinitialize();
	}

	catch( SInt32 err )
	{
		result = err;
	}

	return( result );

} // ShutDownServer

struct sProxyHeader {
	char		tag[kDSTCPEndpointMessageTagSize];
	uint32_t	payloadLen;
};	

struct sProxyPacket {
	sProxyHeader	header;
	char			payload[];
};

struct sReadContext
{
	uint32_t	bufferSize;
	uint32_t	bufferLen;
	union {
		sProxyPacket	*packet;
		char			*buffer;
	} u;
};

static void __TCPListenerEventCallback( int listenFD )
{
	sockaddr_storage	address;
	socklen_t			aLen		= sizeof( address );
	char				addr_string[INET6_ADDRSTRLEN];

	int sock = accept( listenFD, (sockaddr *) &address, (socklen_t *)&aLen );
	if ( sock != -1 )
	{
		int	yes	= 1;
		
		setsockopt( sock, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes) );
		setsockopt( sock, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof(yes) );
		
		int fcntlFlags = fcntl( sock, F_GETFL, 0 );
		assert( fcntlFlags != -1 );
		assert( fcntl(sock, F_SETFL, fcntlFlags | O_NONBLOCK) != -1 );

		sReadContext	*context	= (sReadContext *) calloc( 1, sizeof(sReadContext) );
		DSTCPEndpoint	*endPoint	= new DSTCPEndpoint( kTCPOpenTimeout, kTCPRWTimeout, sock );
		in_port_t		tcpPort;
		
		switch ( address.ss_family ) {
			case AF_INET:
				tcpPort = ((sockaddr_in *) &address)->sin_port;
				break;
				
			case AF_INET6:
				tcpPort = ((sockaddr_in6 *) &address)->sin6_port;
				break;
				
			default:
				tcpPort = 0;
				break;
		}
		
		void (^readBlock)(dispatch_source_t) = ^(dispatch_source_t ds) {
			
			long err_code = 0;
			long err_domain = dispatch_source_get_error( ds, &err_code );
			switch ( err_domain )
			{
				case DISPATCH_ERROR_DOMAIN_NO_ERROR:
					break;
				case DISPATCH_ERROR_DOMAIN_POSIX:
					if ( err_code == ECANCELED )
						return;
				default:
					DbgLog( kLogError, "ServerControl::TCPReadEventCallback - dispatch timer receved error domain %ld error %ld", 
						   err_domain, err_code );
			}
			
			uint32_t bytesAvail = dispatch_source_get_data( ds );
			uint32_t newLen = context->bufferLen + bytesAvail;
			
			DbgLog( kLogDebug, "ServerControl::TCPReadEventCallback - event %d bytes available", bytesAvail );
			
			// assume synchronous transmission
			// will need adjustment if we ever support more than one in-flight transaction at a time on this socket
			if ( context->u.buffer == NULL ) {
				context->bufferSize = (bytesAvail ?: sizeof(sProxyHeader));
				context->u.buffer = (char *) malloc( context->bufferSize );
				assert( context->u.buffer != NULL );
			}
			else if ( newLen > context->bufferSize ) {
				uint32_t expectedLen = (context->bufferLen > sizeof(sProxyHeader) ? ntohl(context->u.packet->header.payloadLen) : 0);

				// if the expected length is greater than the bytes avail, let's allocate up front to miniminize allocs
				if ( expectedLen > newLen ) {
					newLen = expectedLen;
				}
				
				context->bufferSize = newLen;
				context->u.buffer = (char *) reallocf( context->u.buffer, context->bufferSize );
				assert( context->u.buffer != NULL );
			}
			
			uint32_t bytes = read( sock, context->u.buffer + context->bufferLen, context->bufferSize - context->bufferLen );
			if ( bytes < 0 ) {
				if ( errno == EAGAIN ) return; // no bytes
				
				// got an error
				DbgLog( kLogError, "ServerControl::TCPReadEventCallback - error %d - releasing the source for socket %d", errno, sock );
				dispatch_cancel( ds );
				dispatch_release( ds );
				return;
			}
			
			if ( bytes == 0 ) {
				DbgLog( kLogInfo, "ServerControl::TCPReadEventCallback - Disconnect - releasing the source for socket %d", sock );
				dispatch_cancel( ds );
				dispatch_release( ds );
				return;
			};				
			
			context->bufferLen += bytes;
			
			// return if we haven't read our header yet
			if ( context->bufferLen < sizeof(sProxyHeader) ) return;
			
			// check our our header tag
			if ( memcmp(context->u.packet->header.tag, "DSPX", kDSTCPEndpointMessageTagSize) != 0 ) {
				context->bufferLen = 0;
				return;
			};
			
			// grab the expected length and byte swap accordingly
			uint32_t expectedLen = ntohl( context->u.packet->header.payloadLen );
			if ( expectedLen > 0 && context->bufferLen >= (expectedLen + sizeof(sProxyHeader)) )
			{
				char	*payload	= context->u.packet->payload;
				char	*buffer		= context->u.buffer;
				
				context->u.buffer = NULL;
				context->bufferSize = 0;
				context->bufferLen = 0;

				dispatch_async( dispatch_get_current_queue(), 
								^(void) {
									SInt32	siResult	= eDSNoErr;
									
									if ( endPoint->Negotiated() == true )
									{
										void	*decryptedData		= NULL;
										UInt32	decryptedDataLen	= 0;
										
										siResult = endPoint->ProcessData( false, 
																		  payload, expectedLen,
																		  decryptedData, decryptedDataLen );
										if ( decryptedData != NULL )
										{
											sComProxyData *pProxyData = (sComProxyData *) decryptedData;
											decryptedData = NULL;
											
											if ( pProxyData->type.msgt_translate != 2 ) {
												SwapProxyMessage( pProxyData, kDSSwapNetworkToHostOrder );
											}
											
											if ( pProxyData->fDataSize <= (decryptedDataLen - sizeof(sComProxyData)) ) {
												CRequestHandler handler;
												sComData		*pComData;
												
												pComData = endPoint->AllocFromProxyStruct( pProxyData );
												DSFree( pProxyData );
												
												bcopy( &address, &pComData->fIPAddress, address.ss_len );
												pComData->fSocket = sock;
												
												CInternalDispatch::AddCapability();
												handler.HandleRequest( &pComData );
												
												endPoint->SendMessage( pComData );
												
												DSFree( pComData );
											}
										}
									}
									else
									{
										siResult = endPoint->ServerNegotiateKey( payload, expectedLen );
									}
									
									free( buffer );

									if ( siResult != eDSNoErr ) {
										char *status = dsCopyDirStatusName( siResult );
										DbgLog( kLogError, "ServerControl::TCPReadEventCallback - %s (%d) - releasing the source for socket %d", 
											   status, siResult, sock );
										DSFree( status );
										
										// release the source here
										gRefTable.CleanRefsForSocket( sock ); // clean here too to prevent race conditions of new sockets
										dispatch_release( ds );
									}
								} );
			}
		};

		char			q_name[64];
		static int32_t	sReqNum		= 0;

		snprintf( q_name, sizeof(q_name), "request #%d socket #%d", __sync_add_and_fetch(&sReqNum, 1), sock );
		
		dispatch_queue_t queue = dispatch_queue_create( q_name, NULL );
		dispatch_source_attr_t attr = dispatch_source_attr_create();
		dispatch_source_attr_set_finalizer( attr, 
										    ^(dispatch_source_t ds) {
												DSFree( context->u.buffer );
												free( context );
												gRefTable.CleanRefsForSocket( dispatch_source_get_descriptor(ds) );
												delete endPoint; // endPoint closes socket
											} );
		
		dispatch_source_t source = dispatch_source_read_create( sock, attr, queue, readBlock );
		assert( source != NULL );
		
		dispatch_release( attr );
		dispatch_release( queue );
	}
}

void ServerControl::TCPListenerEventCallback( int listenFD )
{
	__TCPListenerEventCallback( listenFD ); // 6453258
}

// ---------------------------------------------------------------------------
//	* StartTCPListener ()
//
// ---------------------------------------------------------------------------

SInt32 ServerControl::StartTCPListener ( UInt32 inPort )
{
	SInt32		result		= eDSNoErr;
	char		port[16];
	addrinfo	ai_hints;
	addrinfo	*my_addr	= NULL;
	int			sock		= socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
	
	bzero( &ai_hints, sizeof(ai_hints) );
	ai_hints.ai_flags = AI_PASSIVE;
	ai_hints.ai_family = PF_INET;
	ai_hints.ai_socktype = SOCK_STREAM;
	ai_hints.ai_protocol = IPPROTO_TCP;
	snprintf( port, sizeof(port), "%d", (int) inPort );
	
	if ( getaddrinfo(NULL, port, &ai_hints, &my_addr) == 0 )
	{
		if ( bind(sock, my_addr->ai_addr, my_addr->ai_addr->sa_len) == 0 )
		{
			if ( listen(sock, 25) == 0 )
			{
				DbgLog( kLogInfo, "ServerControl::StartTCPListener - started TCP Listener on port %d", inPort );
				dispatch_queue_t queue = dispatch_queue_create( "TCP listener", NULL );
				dispatch_source_attr_t attr = dispatch_source_attr_create();
				dispatch_source_attr_set_finalizer( attr, 
												    ^(dispatch_source_t ds) {
														DbgLog( kLogNotice, "ServerControl::StartTCPListener - exiting listener on socket %d", sock );
														close( sock );
													} );
				dispatch_source_t tempSource = dispatch_source_read_create( sock,
																		    attr,
																		    queue,
																		    ^(dispatch_source_t ds) {
																				if ( dispatch_source_get_error(ds, NULL) == 0 ) {
																					__TCPListenerEventCallback(sock);
																				}
																			} );
				dispatch_release( attr );
				dispatch_release( queue );
				
				if ( __sync_bool_compare_and_swap(&ServerControl::fTCPListener, NULL, tempSource) == false ) {
					dispatch_release( tempSource );
					tempSource = NULL;
				}
			}
			else
			{
				close( sock );
			}
		}
		
		freeaddrinfo( my_addr );
	}
	
	return result;
	
} // StartTCPListener

// ---------------------------------------------------------------------------
//	* StopTCPListener ()
//
// ---------------------------------------------------------------------------

SInt32 ServerControl::StopTCPListener ( void )
{
	if ( fTCPListener == nil ) return eMemoryAllocError;
	
	dispatch_source_t tempSource = fTCPListener;
	if ( __sync_bool_compare_and_swap(&fTCPListener, tempSource, NULL) == true )
	{
		DbgLog( kLogNotice, "ServerControl::StopTCPListener stopping listener" );
		dispatch_cancel( tempSource );
		dispatch_release( tempSource );
	}
	
	return eDSNoErr;
	
} // StopTCPListener


// ---------------------------------------------------------------------------
//	* RegisterForNetworkChange ()
//
// ---------------------------------------------------------------------------

SInt32 ServerControl:: RegisterForNetworkChange ( void )
{
	SInt32				scdStatus			= eDSNoErr;
	CFStringRef			ipKey				= 0;	//ip changes key
	CFMutableArrayRef	notifyKeys			= 0;
	CFMutableArrayRef	notifyPatterns		= 0;
	SCDynamicStoreRef	store				= NULL;
    CFRunLoopSourceRef	rls					= NULL;
	
	DbgLog( kLogApplication, "RegisterForNetworkChange(): " );

	notifyKeys		= CFArrayCreateMutable(	kCFAllocatorDefault,
											0,
											&kCFTypeArrayCallBacks);
	notifyPatterns	= CFArrayCreateMutable(	kCFAllocatorDefault,
											0,
											&kCFTypeArrayCallBacks);
											
	// watch for IPv4 configuration changes
	ipKey = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, kSCEntNetIPv4);
	CFArrayAppendValue(notifyKeys, ipKey);
	CFRelease(ipKey);
	
	// watch for IPv4 interface configuration changes
	ipKey = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL, kSCDynamicStoreDomainState, kSCCompAnyRegex, kSCEntNetIPv4);
	CFArrayAppendValue(notifyPatterns, ipKey);
	CFRelease(ipKey);

	// watch for IPv6 interface configuration changes
	ipKey = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL, kSCDynamicStoreDomainState, kSCCompAnyRegex, kSCEntNetIPv6);
	CFArrayAppendValue(notifyPatterns, ipKey);
	CFRelease(ipKey);
	
	//not checking bool return
	store = SCDynamicStoreCreate(NULL, fServiceNameString, NetworkChangeCallBack, NULL);
	if (store != NULL && notifyKeys != NULL && notifyPatterns != NULL)
	{
		SCDynamicStoreSetNotificationKeys(store, notifyKeys, notifyPatterns);
		rls = SCDynamicStoreCreateRunLoopSource(NULL, store, 0);
		if (rls != NULL)
		{
			// we watch for network changes on our plugin loop so they can't block other notifications
			CFRunLoopAddSource(gPluginRunLoop, rls, kCFRunLoopDefaultMode);
			CFRelease(rls);
			rls = NULL;
		}
		else
		{
			syslog(LOG_ALERT,"Unable to add source to RunLoop for SystemConfiguration registration for Network Notification");
		}
	}
	else
	{
		syslog(LOG_ALERT,"Unable to create DirectoryService store for SystemConfiguration registration for Network Notification");
	}

	DSCFRelease(notifyKeys);
	DSCFRelease(notifyPatterns);
	DSCFRelease(store);
	
	return scdStatus;
	
} // RegisterForNetworkChange

// ---------------------------------------------------------------------------
//	* UnRegisterForNetworkChange ()
//
// ---------------------------------------------------------------------------

SInt32 ServerControl:: UnRegisterForNetworkChange ( void )
{
	SInt32		scdStatus = eDSNoErr;

	DbgLog( kLogApplication, "UnRegisterForNetworkChange(): " );

	return scdStatus;

} // UnRegisterForNetworkChange


// ---------------------------------------------------------------------------
//	* RegisterForSystemPower ()
//
// ---------------------------------------------------------------------------

SInt32 ServerControl::RegisterForSystemPower ( void )
{
	IONotificationPortRef	pmNotificationPortRef;
	CFRunLoopSourceRef		pmNotificationRunLoopSource;
	
	DbgLog( kLogApplication, "RegisterForSystemPower(): " );

	gPMKernelPort = IORegisterForSystemPower(this, &pmNotificationPortRef, dsPMNotificationHandler, &gPMDeregisterNotifier);
	if (gPMKernelPort == 0 || pmNotificationPortRef == nil)
	{
		ErrLog( kLogApplication, "RegisterForSystemPower(): IORegisterForSystemPower failed" );
	}
	else
	{
		pmNotificationRunLoopSource = IONotificationPortGetRunLoopSource(pmNotificationPortRef);
		
		if (pmNotificationRunLoopSource == nil)
		{
			ErrLog( kLogApplication, "RegisterForSystemPower(): IONotificationPortGetRunLoopSource failed" );
			gPMKernelPort = nil;
		}
		else
		{
			// TODO: should this be on Plugin loop due to potential blockage? or is that ok?
			CFRunLoopAddSource( CFRunLoopGetMain(), pmNotificationRunLoopSource, kCFRunLoopCommonModes );
		}
	}
	
	return (gPMKernelPort != 0) ? eDSNoErr : -1;
} // RegisterForSystemPower

// ---------------------------------------------------------------------------
//	* UnRegisterForSystemPower ()
//
// ---------------------------------------------------------------------------

SInt32 ServerControl::UnRegisterForSystemPower ( void )
{
	SInt32 ioResult = eDSNoErr;

	DbgLog( kLogApplication, "UnRegisterForSystemPower(): " );

	if (gPMKernelPort != 0) {
		gPMKernelPort = 0;
        ioResult = (SInt32)IODeregisterForSystemPower(&gPMDeregisterNotifier);
		if (ioResult != eDSNoErr)
		{
			DbgLog( kLogApplication, "UnRegisterForSystemPower(): IODeregisterForSystemPower failed, error= %d", ioResult );
		}
    }
   return ioResult;
} // UnRegisterForSystemPower


// ---------------------------------------------------------------------------
//	* HandleSystemWillSleep ()
//
// ---------------------------------------------------------------------------

SInt32 ServerControl::HandleSystemWillSleep ( void )
{
	SInt32						siResult		= eDSNoErr;
	UInt32						iterator		= 0;
	CServerPlugin			   *pPlugin			= nil;
	sHeader						aHeader;
	CPlugInList::sTableData	   *pPIInfo			= nil;
	
	SrvrLog( kLogApplication, "Sleep Notification occurred.");
	
	aHeader.fType			= kHandleSystemWillSleep;
	aHeader.fResult			= eDSNoErr;
	aHeader.fContextData	= nil;

	if ( gPlugins != nil )
	{
		pPlugin = gPlugins->Next( &iterator );
		while (pPlugin != nil)
		{
			pPIInfo = gPlugins->GetPlugInInfo( iterator-1 );
			if (pPIInfo != NULL && pPIInfo->fState & kActive) //only notify Active plugins
			{
				siResult = eDSNoErr;
				siResult = pPlugin->ProcessRequest( (void*)&aHeader );
				if (siResult != eDSNoErr && siResult != eNotHandledByThisNode && siResult != eNotYetImplemented)
				{
					ErrLog( kLogApplication, "SystemWillSleep Notification in %s plugin returned error %d", pPIInfo->fName, siResult );
				}
			}
			pPlugin = gPlugins->Next( &iterator );
		}
	}
	
	return siResult;
}

// ---------------------------------------------------------------------------
//	* HandleSystemWillPowerOn ()
//
// ---------------------------------------------------------------------------

SInt32 ServerControl::HandleSystemWillPowerOn ( void )
{
	SInt32						siResult		= eDSNoErr;
	UInt32						iterator		= 0;
	CServerPlugin			   *pPlugin			= nil;
	sHeader						aHeader;
	CPlugInList::sTableData	   *pPIInfo			= nil;
	
	SrvrLog( kLogApplication, "Will Power On (Wake) Notification occurred.");
	
	aHeader.fType			= kHandleSystemWillPowerOn;
	aHeader.fResult			= eDSNoErr;
	aHeader.fContextData	= nil;

	if ( gPlugins != nil )
	{
		pPlugin = gPlugins->Next( &iterator );
		while (pPlugin != nil)
		{
			pPIInfo = gPlugins->GetPlugInInfo( iterator-1 );
			if (pPIInfo != NULL && pPIInfo->fState & kActive) //only notify Active plugins
			{
				siResult = eDSNoErr;
				siResult = pPlugin->ProcessRequest( (void*)&aHeader );
				if (siResult != eDSNoErr && siResult != eNotHandledByThisNode && siResult != eNotYetImplemented)
				{
					ErrLog( kLogApplication, "WillPowerOn Notification in %s plugin returned error %d", pPIInfo->fName, siResult );
				}
			}
			pPlugin = gPlugins->Next( &iterator );
		}
	}
	
	return siResult;
}

// ---------------------------------------------------------------------------
//	* HandleNetworkTransition ()
//
// ---------------------------------------------------------------------------

void ServerControl::HandleNetworkTransition ( void )
{
	SInt32						siResult		= eDSNoErr;
	UInt32						iterator		= 0;
	CServerPlugin			   *pPlugin			= nil;
	sHeader						aHeader;
	CPlugInList::sTableData	   *pPIInfo			= nil;
	CServerPlugin			   *searchPlugin	= nil;
	UInt32						searchIterator	= 0;
	
	aHeader.fType			= kHandleNetworkTransition;
	aHeader.fResult			= eDSNoErr;
	aHeader.fContextData	= nil;

	SrvrLog( kLogApplication, "Network transition occurred." );
	gFirstNetworkUpAtBoot = true;
	//call thru to each plugin
	if ( gPlugins != nil )
	{
		pPlugin = gPlugins->Next( &iterator );
		while (pPlugin != nil)
		{
			pPIInfo = gPlugins->GetPlugInInfo( iterator-1 );
			if (pPIInfo != NULL && pPIInfo->fState & kActive) //only notify Active plugins
			{
				if ( ::strcmp(pPIInfo->fName,"Search") != 0)
				{
					siResult = eDSNoErr;
					siResult = pPlugin->ProcessRequest( (void*)&aHeader );
					if (siResult != eDSNoErr)
					{
						ErrLog( kLogApplication, "Network transition in %s plugin returned error %d", pPIInfo->fName, siResult );
					}
				}
				else
				{
					searchIterator	= iterator;
					searchPlugin	= pPlugin;
				}
			}
			pPlugin = gPlugins->Next( &iterator );
		}
	}
	
	//handle the search plugin transition last to ensure at least Local and LDAPv3 have gone first
	if (searchPlugin != nil)
	{
		siResult = eDSNoErr;
		//now do the network transition itself
		aHeader.fType = kHandleNetworkTransition;
		siResult = searchPlugin->ProcessRequest( (void*)&aHeader );
		if (siResult != eDSNoErr)
		{
			ErrLog( kLogApplication, "Network transition in Search returned error %d", siResult );
		}
	}

} // HandleNetworkTransition

void
ServerControl::ToggleAPILogging( bool fromSignal )
{
	static dispatch_source_t	loggingTimer	= NULL;
	static pthread_mutex_t		localLock		= PTHREAD_MUTEX_INITIALIZER;

	pthread_mutex_lock( &localLock );
	if ( loggingTimer != NULL ) {
		dispatch_cancel( loggingTimer );
		dispatch_release( loggingTimer );
		loggingTimer = NULL;
	}

	if ( __sync_bool_compare_and_swap(&gLogAPICalls, false, true) == true ) {
		loggingTimer = dispatch_source_timer_create( DISPATCH_TIMER_ONESHOT, 
													 300ULL * NSEC_PER_SEC, 
													 1 * NSEC_PER_SEC,
													 NULL,
													 dispatch_get_concurrent_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT),
													 ^(dispatch_source_t ds) {
														 if ( dispatch_source_get_error(ds, NULL) == 0 && 
															  gIgnoreSunsetTime == false &&
															  __sync_bool_compare_and_swap(&gLogAPICalls, true, false) == true )
														 {
															 syslog( LOG_CRIT, "Logging of API Calls automatically turned OFF after reaching sunset duration of five minutes." );
														 }
													 } );
		if ( fromSignal == true ) syslog( LOG_ALERT, "Logging of API Calls turned ON after receiving USR2 signal." );
	}
	else if ( __sync_bool_compare_and_swap(&gLogAPICalls, true, false) == true ) {
		if ( fromSignal == true ) syslog( LOG_ALERT, "Logging of API Calls turned OFF after receiving USR2 signal." );
	}
	
	pthread_mutex_unlock( &localLock );
}

#ifdef BUILD_IN_PERFORMANCE
void ServerControl::DeletePerfStatTable( void )
{
	PluginPerformanceStats**	table = fPerfTable;
	UInt32						pluginCount = fPerfTableNumPlugins;

	fPerfTable = NULL;
	fPerfTableNumPlugins = 0;
	
	if ( table )
	{
		for ( UInt32 i=0; i<pluginCount+1; i++ )
		{
			if ( table[i] )
			{
				free( table[i] );
				table[i] = NULL;
			}
		}
		
		free( table );
	}
}

PluginPerformanceStats** ServerControl::CreatePerfStatTable( void )
{
DbgLog( kLogPerformanceStats, "ServerControl::CreatePerfStatTable called\n" );

	PluginPerformanceStats**	table = NULL;
	UInt32						pluginCount = gPlugins->GetPlugInCount();
	
	if ( fPerfTable )
		DeletePerfStatTable();
		
	// how many plugins?
	table = (PluginPerformanceStats**)calloc( sizeof(PluginPerformanceStats*), pluginCount+1 );	// create table for #plugins + 1 for server

	for ( UInt32 i=0; i<pluginCount; i++ )
	{
		table[i] = (PluginPerformanceStats*)calloc( sizeof(PluginPerformanceStats), 1 );
		table[i]->pluginSignature = gPlugins->GetPlugInInfo(i)->fKey;
		table[i]->pluginName = gPlugins->GetPlugInInfo(i)->fName;
	}
	
	table[pluginCount] = (PluginPerformanceStats*)calloc( sizeof(PluginPerformanceStats), 1 );
	table[pluginCount]->pluginSignature = 0;
	table[pluginCount]->pluginName = "Server";
	
	fPerfTableNumPlugins = pluginCount;
	fPerfTable = table;
	
	return table;
}

double gLastDump =0;
#define	kNumSecsBetweenDumps	60*2
void ServerControl::HandlePerformanceStats( UInt32 msgType, FourCharCode pluginSig, SInt32 siResult, SInt32 clientPID, double inTime, double outTime )
{
	// Since the number of plugins is so small, just doing an O(n)/2 search is probably fine...
	gPerformanceLoggingLock->WaitLock();
	PluginPerformanceStats*		curPluginStats = NULL;
	UInt32						pluginCount = gPlugins->GetPlugInCount();

	if ( !fPerfTable || fPerfTableNumPlugins != pluginCount )
	{
		// first api call, or number of plugins changed, (re)create the table
		fPerfTable = CreatePerfStatTable();
	}
	
	if ( !pluginSig )
		curPluginStats = fPerfTable[pluginCount];	// last entry in the table is reserved for the server
		
	if ( fPerfTable[fLastPluginCalled]->pluginSignature == pluginSig )
		curPluginStats = fPerfTable[fLastPluginCalled];
		
	for ( UInt32 i=0; !curPluginStats && i<pluginCount; i++ )
	{
		if ( pluginSig == fPerfTable[i]->pluginSignature )
		{
			curPluginStats = fPerfTable[i];
			fLastPluginCalled = i;
		}
	}
	
	if ( curPluginStats )
	{
		PluginPerformanceAPIStat*	curAPI = &(curPluginStats->apiStats[msgType]);
		double						duration = outTime-inTime;
		curAPI->msgCnt++;
		
		if ( siResult )
		{
			for( int i=kNumErrorsToTrack-1; i>0; i-- )
			{
				curAPI->lastNErrors[i].error = curAPI->lastNErrors[i-1].error;
				curAPI->lastNErrors[i].clientPID = curAPI->lastNErrors[i-1].clientPID;
			}
			
			curAPI->lastNErrors[0].error = siResult;
			curAPI->lastNErrors[0].clientPID = clientPID;
			curAPI->errCnt++;
		}
		
		if ( curAPI->minTime == 0 || curAPI->minTime > duration )
			curAPI->minTime = duration;
			
		if ( curAPI->maxTime == 0 || curAPI->maxTime < duration )
			curAPI->maxTime = duration;
		
		curAPI->totTime += duration;
	}
	
	gPerformanceLoggingLock->SignalLock();
}

#define USEC_PER_HOUR	(double)60*60*USEC_PER_SEC	/* microseconds per hour */
#define USEC_PER_DAY	(double)24*USEC_PER_HOUR	/* microseconds per day */

void ServerControl::LogStats( void )
{
	PluginPerformanceStats*		curPluginStats = NULL;
	UInt32						pluginCount = fPerfTableNumPlugins;
	char						logBuf[1024];
	char						totTimeStr[256];
	
	gPerformanceLoggingLock->WaitLock();

	syslog( LOG_CRIT, "**Usage Stats**\n");
	syslog( LOG_CRIT, "\tPlugin\tAPI\tMsgCnt\tErrCnt\tminTime (usec)\tmaxTime (usec)\taverageTime (usec)\ttotTime (usec|secs|hours|days)\tLast PID\tLast Error\tPrev PIDs/Errors\n" );
	
	for ( UInt32 i=0; i<pluginCount+1; i++ )	// server is at the end
	{
		if ( !fPerfTable[i] )
			continue;
			
		curPluginStats = fPerfTable[i];
				
		for ( UInt32 j=0; j<kDSPlugInCallsEnd; j++ )
		{
			if ( curPluginStats->apiStats[j].msgCnt > 0 )
			{
				if ( curPluginStats->apiStats[j].totTime < USEC_PER_SEC )
					sprintf( totTimeStr, "%0.f usecs", curPluginStats->apiStats[j].totTime );
				else if ( curPluginStats->apiStats[j].totTime < USEC_PER_HOUR )
				{
					double		time = curPluginStats->apiStats[j].totTime / USEC_PER_SEC;
					sprintf( totTimeStr, "%0.4f secs", time );
				}
				else if ( curPluginStats->apiStats[j].totTime < USEC_PER_DAY )
				{
					double		time = curPluginStats->apiStats[j].totTime / USEC_PER_HOUR;
					sprintf( totTimeStr, "%0.4f hours", time );
				}
				else
				{
					double		time = curPluginStats->apiStats[j].totTime / USEC_PER_DAY;
					sprintf( totTimeStr, "%0.4f days", time );
				}
				
				sprintf( logBuf, "\t%s\t%s\t%d\t%d\t%.0f\t%0.f\t%0.f\t%s\t%d/%d\t%d/%d\t%d/%d\t%d/%d\t%d/%d\n",
								curPluginStats->pluginName,
								CRequestHandler::GetCallName(j),
								curPluginStats->apiStats[j].msgCnt,
								curPluginStats->apiStats[j].errCnt,
								curPluginStats->apiStats[j].minTime,
								curPluginStats->apiStats[j].maxTime,
								(curPluginStats->apiStats[j].totTime/curPluginStats->apiStats[j].msgCnt),
								totTimeStr,
								curPluginStats->apiStats[j].lastNErrors[0].clientPID,
								curPluginStats->apiStats[j].lastNErrors[0].error,
								curPluginStats->apiStats[j].lastNErrors[1].clientPID,
								curPluginStats->apiStats[j].lastNErrors[1].error,
								curPluginStats->apiStats[j].lastNErrors[2].clientPID,
								curPluginStats->apiStats[j].lastNErrors[2].error,
								curPluginStats->apiStats[j].lastNErrors[3].clientPID,
								curPluginStats->apiStats[j].lastNErrors[3].error,
								curPluginStats->apiStats[j].lastNErrors[4].clientPID,
								curPluginStats->apiStats[j].lastNErrors[4].error );
				
				syslog( LOG_CRIT, logBuf );
			}
		}
	}
	
	gPerformanceLoggingLock->SignalLock();
}
#endif

// ---------------------------------------------------------------------------
//	* DoPeriodicTask ()
//
// ---------------------------------------------------------------------------

void ServerControl::DoPeriodicTask( dispatch_source_t )
{
	SInt32						siResult		= eDSNoErr;
	UInt32						iterator		= 0;
	CServerPlugin			   *pPlugin			= nil;
	CPlugInList::sTableData	   *pPIInfo			= nil;
	
	//call thru to each plugin
	if ( gPlugins != nil )
	{
		pPlugin = gPlugins->Next( &iterator );
		while (pPlugin != nil)
		{
			pPIInfo = gPlugins->GetPlugInInfo( iterator-1 );
			if (pPIInfo != NULL && pPIInfo->fState & kActive) //only pulse the Active plugins
			{
				siResult = pPlugin->PeriodicTask();
				if (siResult != eDSNoErr)
				{
					DbgLog( kLogApplication, "Periodic Task in %s plugin returned error %d", pPIInfo->fName, siResult );
				}
			}
			pPlugin = gPlugins->Next( &iterator );
		}
	}
	
	return;
} // DoPeriodicTask

void ServerControl::CreateDebugPrefFileIfNecessary( bool bForceCreate )
{
	UInt32					uiDataSize	= 0;
	CFile				   *pFile		= nil;
	struct stat				statbuf;
	CFDataRef				dataRef		= nil;

	if ( bForceCreate )
		unlink( kDSDebugConfigFilePath );
	
	if ( lstat(kDSDebugConfigFilePath, &statbuf) != 0 )
	{
		//write a default file and setup debugging
		SInt32 result = eDSNoErr;
		CLog::SetLoggingPriority(keDebugLog, 5);
		uiDataSize = ::strlen( kDefaultDebugConfig );
		dataRef = ::CFDataCreate( nil, (const UInt8 *)kDefaultDebugConfig, uiDataSize );
		if ( dataRef != nil )
		{
			result = dsCreatePrefsDirectory();
			
			CFIndex dataLen = CFDataGetLength( dataRef );
			const UInt8 *pUData = CFDataGetBytePtr( dataRef );
			if ( pUData != NULL && dataLen > 0 )
			{
				try
				{
					pFile = new CFile( kDSDebugConfigFilePath, true );
					if ( pFile != nil )
					{
						if ( pFile->is_open() )
						{
							pFile->seteof( 0 );
							pFile->write( pUData, dataLen );
							chmod( kDSDebugConfigFilePath, 0600 );
						}
						
						delete( pFile );
						pFile = nil;
					}
				}
				catch ( ... )
				{
				}
			}
			
			CFRelease( dataRef );
			dataRef = nil;
		}
	}
}

// ---------------------------------------------------------------------------
//	* ResetDebugging ()
//
// ---------------------------------------------------------------------------

void ServerControl::ResetDebugging( void )
{
	UInt32					uiDataSize	= 0;
	char				   *pData		= nil;
	CFile				   *pFile		= nil;
	struct stat				statbuf;
	CFDataRef				dataRef		= nil;
	CFBooleanRef			cfBool		= false;
	bool					bDebugging  = false;
	bool					bFileUsed   = false;
	

	if (gToggleDebugging)
	{
		gToggleDebugging = false;
		//here we turn everything off
		if (gDebugLogging)
		{
			CLog::StopDebugLog();
			gDebugLogging = false;
			syslog(LOG_ALERT,"Debug Logging turned OFF after receiving USR1 signal.");
			DbgLog( kLogNotice ,"Debug Logging turned OFF after receiving USR1 signal." );
		}
		gDSFWCSBPDebugLogging = false;
	}
	else
	{
		//next time this is called we turn everything off
		gToggleDebugging = true;
		
		// Does the debug config file exist
		if ( stat( kDSDebugConfigFilePath, &statbuf ) == 0 )
		{
			// Attempt to get config info from file
			pFile = new CFile( kDSDebugConfigFilePath );
			if (pFile != nil) 
			{
				if ( (pFile->is_open()) && (pFile->FileSize() > 0) )
				{
					// Allocate space for the file data
					pData = (char *)::calloc( 1, pFile->FileSize() + 1 );
					if ( pData != nil )
					{
						// Read from the config file
						uiDataSize = pFile->ReadBlock( pData, pFile->FileSize() );
						dataRef = ::CFDataCreate( nil, (const UInt8 *)pData, uiDataSize );
						if ( dataRef != nil )
						{
							CFPropertyListRef   aPlistRef   = 0;
							CFDictionaryRef		aDictRef	= 0;
							// Is it valid XML data
							aPlistRef = ::CFPropertyListCreateFromXMLData( kCFAllocatorDefault, dataRef, kCFPropertyListImmutable, nil );
							if ( aPlistRef != nil )
							{
								// Is it a plist type
								if ( ::CFDictionaryGetTypeID() == ::CFGetTypeID( aPlistRef ) )
								{
								
									bFileUsed = true;
									
									aDictRef = (CFDictionaryRef)aPlistRef;
									
									//now set up the debugging according to the plist settings
									
									if ( CFDictionaryContainsKey( aDictRef, CFSTR( kXMLDSIgnoreSunsetTimeKey ) ) )
									{
										cfBool= (CFBooleanRef)CFDictionaryGetValue( aDictRef, CFSTR( kXMLDSIgnoreSunsetTimeKey ) );
										if (cfBool != nil)
										{
											gIgnoreSunsetTime = CFBooleanGetValue( cfBool );
											//CFRelease( cfBool ); // no since pointer only from Get
										}
									}

									//debug logging boolean
									if ( CFDictionaryContainsKey( aDictRef, CFSTR( kXMLDSDebugLoggingKey ) ) )
									{
										cfBool= (CFBooleanRef)CFDictionaryGetValue( aDictRef, CFSTR( kXMLDSDebugLoggingKey ) );
										if (cfBool != nil)
										{
											bDebugging = CFBooleanGetValue( cfBool );
											//CFRelease( cfBool ); // no since pointer only from Get
											if (gDebugLogging && !bDebugging)
											{
												CLog::StopDebugLog();
												gDebugLogging = false;
												syslog(LOG_ALERT,"Debug Logging turned OFF after receiving USR1 signal.");
												DbgLog( kLogNotice, "Debug Logging turned OFF after receiving USR1 signal." );
											}
											else if (bDebugging)
											{
												if ( CFDictionaryContainsKey( aDictRef, CFSTR( kXMLDSDebugLoggingPriority ) ) )
												{
													CFNumberRef logPriority	= (CFNumberRef)CFDictionaryGetValue( aDictRef, CFSTR(kXMLDSDebugLoggingPriority) );
													UInt32 priorityValue = 0;
													CFNumberGetValue(logPriority, kCFNumberIntType, &priorityValue);
													if (priorityValue > 0 && priorityValue < 9) //1-5 is tiered and 6-8 are specific
													{
														CLog::SetLoggingPriority(keDebugLog, priorityValue);
														DbgLog( kLogNotice,"Debug Logging priority %u set.", priorityValue );
														syslog(LOG_ALERT,"Debug Logging priority %u set.", priorityValue);
													}
												}
												else
												{
													CLog::SetLoggingPriority(keDebugLog, 5);
													syslog(LOG_ALERT,"Debug Logging default priority 5 set.");
													DbgLog( kLogNotice, "Debug Logging default priority 5 set." );
												}

												CLog::StartDebugLog();
												if (!gDebugLogging)
												{
													syslog(LOG_ALERT,"Debug Logging turned ON after receiving USR1 signal.");
													DbgLog( kLogNotice, "Debug Logging turned ON after receiving USR1 signal." );
													gDebugLogging = true;
												}
											}
										}
									}
									else if (gDebugLogging)
									{
										CLog::StopDebugLog();
										gDebugLogging = false;
										syslog(kLogNotice,"Debug Logging turned OFF after receiving USR1 signal.");
										DbgLog( kLogNotice, "Debug Logging turned OFF after receiving USR1 signal." );
									}

									//FW CSBP debug logging boolean
									if ( CFDictionaryContainsKey( aDictRef, CFSTR( kXMLDSCSBPDebugLoggingKey ) ) )
									{
										cfBool= (CFBooleanRef)CFDictionaryGetValue( aDictRef, CFSTR( kXMLDSCSBPDebugLoggingKey ) );
										if (cfBool != nil)
										{
											gDSFWCSBPDebugLogging = CFBooleanGetValue( cfBool );
											//CFRelease( cfBool ); // no since pointer only from Get
										}
									}
									else
									{
										gDSFWCSBPDebugLogging = false;
									}

									aDictRef = 0;
								}
								//free the propertylist
								CFRelease(aPlistRef);
							}
							CFRelease( dataRef );
							dataRef = nil;
						}
						free( pData );
						pData = nil;
					}
				}
				delete( pFile );
				pFile = nil;
			}
		}
		
		if (!bFileUsed)
			CreateDebugPrefFileIfNecessary( true );
	}
} // ResetDebugging


