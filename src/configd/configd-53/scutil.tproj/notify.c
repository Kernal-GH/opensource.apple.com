/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * Modification History
 *
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * November 9, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "scutil.h"

#include <SystemConfiguration/SCPrivate.h>
#include "v1Compatibility.h"


static int			osig;
static struct sigaction		*oact	= NULL;


static CFComparisonResult
sort_keys(const void *p1, const void *p2, void *context) {
	CFStringRef key1 = (CFStringRef)p1;
	CFStringRef key2 = (CFStringRef)p2;
	return CFStringCompare(key1, key2, 0);
}


void
storeCallback(SCDynamicStoreRef store, CFArrayRef changedKeys, void *info)
{
	int		i;
	CFIndex		n;

	SCPrint(TRUE, stdout, CFSTR("notification callback (store address = %p).\n"), store);

	n = CFArrayGetCount(changedKeys);
	if (n > 0) {
		for (i=0; i<n; i++) {
			SCPrint(TRUE,
				stdout,
				CFSTR("  changed key [%d] = %@\n"),
				i,
				CFArrayGetValueAtIndex(changedKeys, i));
		}
	} else {
		SCPrint(TRUE, stdout, CFSTR("  no changed key's.\n"));
	}

	return;
}


void
do_notify_list(int argc, char **argv)
{
	int			i;
	CFArrayRef		list;
	CFIndex			listCnt;
	Boolean			isRegex = FALSE;
	CFMutableArrayRef	sortedList;

	if (argc == 1)
		isRegex = TRUE;

	list = SCDynamicStoreCopyWatchedKeyList(store, isRegex);
	if (!list) {
		if (SCError() != kSCStatusOK) {
			SCPrint(TRUE, stdout, CFSTR("  %s\n"), SCErrorString(SCError()));
		} else {
			SCPrint(TRUE,
				stdout,
				CFSTR("  no notifier %s.\n"),
				isRegex ? "patterns" : "keys");
		}
		return;
	}

	listCnt = CFArrayGetCount(list);
	sortedList = CFArrayCreateMutableCopy(NULL, listCnt, list);
	CFRelease(list);
	CFArraySortValues(sortedList,
			  CFRangeMake(0, listCnt),
			  sort_keys,
			  NULL);

	if (listCnt > 0) {
		for (i=0; i<listCnt; i++) {
			SCPrint(TRUE,
				stdout,
				CFSTR("  notifier %s [%d] = %@\n"),
				isRegex ? "pattern" : "key",
				i,
				CFArrayGetValueAtIndex(sortedList, i));
		}
	} else {
		SCPrint(TRUE,
			stdout,
			CFSTR("  no notifier %s.\n"),
			isRegex ? "patterns" : "keys");
	}
	CFRelease(sortedList);

	return;
}


void
do_notify_add(int argc, char **argv)
{
	CFStringRef	key;
	Boolean		isRegex = FALSE;

	key = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingMacRoman);

	if (argc == 2)
		isRegex = TRUE;

	if (!SCDynamicStoreAddWatchedKey(store, key, isRegex)) {
		SCPrint(TRUE, stdout, CFSTR("  %s\n"), SCErrorString(SCError()));
	}
	CFRelease(key);
	return;
}


void
do_notify_remove(int argc, char **argv)
{
	CFStringRef	key;
	Boolean		isRegex = FALSE;

	key   = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingMacRoman);

	if (argc == 2)
		isRegex = TRUE;

	if (!SCDynamicStoreRemoveWatchedKey(store, key, isRegex)) {
		SCPrint(TRUE, stdout, CFSTR("  %s\n"), SCErrorString(SCError()));
	}
	CFRelease(key);
	return;
}


void
do_notify_changes(int argc, char **argv)
{
	CFArrayRef	list;
	CFIndex		listCnt;
	int		i;

	list = SCDynamicStoreCopyNotifiedKeys(store);
	if (!list) {
		SCPrint(TRUE, stdout, CFSTR("  %s\n"), SCErrorString(SCError()));
		return;
	}

	listCnt = CFArrayGetCount(list);
	if (listCnt > 0) {
		for (i=0; i<listCnt; i++) {
			SCPrint(TRUE,
				stdout,
				CFSTR("  changedKey [%d] = %@\n"),
				i,
				CFArrayGetValueAtIndex(list, i));
		}
	} else {
		SCPrint(TRUE, stdout, CFSTR("  no changedKey's.\n"));
	}
	CFRelease(list);

	return;
}


void
do_notify_watch(int argc, char **argv)
{
	notifyRls = SCDynamicStoreCreateRunLoopSource(NULL, store, 0);
	if (!notifyRls) {
		SCPrint(TRUE, stdout, CFSTR("  %s\n"), SCErrorString(SCError()));
		return;
	}

	CFRunLoopAddSource(CFRunLoopGetCurrent(), notifyRls, kCFRunLoopDefaultMode);
	return;
}


void
do_notify_wait(int argc, char **argv)
{
	if (!SCDynamicStoreNotifyWait(store)) {
		SCPrint(TRUE, stdout, CFSTR("  %s\n"), SCErrorString(SCError()));
		return;
	}

	return;
}


static boolean_t
notificationWatcher(SCDynamicStoreRef store, void *arg)
{
	SCPrint(TRUE, stdout, CFSTR("notification callback (store address = %p).\n"), store);
	SCPrint(TRUE, stdout, CFSTR("  arg = %s.\n"), (char *)arg);
	return TRUE;
}


static boolean_t
notificationWatcherVerbose(SCDynamicStoreRef store, void *arg)
{
	SCPrint(TRUE, stdout, CFSTR("notification callback (store address = %p).\n"), store);
	SCPrint(TRUE, stdout, CFSTR("  arg = %s.\n"), (char *)arg);
	do_notify_changes(0, NULL);	/* report the keys which changed */
	return TRUE;
}


void
do_notify_callback(int argc, char **argv)
{
	SCDynamicStoreCallBack_v1	func  = notificationWatcher;

	if ((argc == 1) && (strcmp(argv[0], "verbose") == 0)) {
		func = notificationWatcherVerbose;
	}

	if (!SCDynamicStoreNotifyCallback(store, CFRunLoopGetCurrent(), func, "Changed detected by callback handler!")) {
		SCPrint(TRUE, stdout, CFSTR("  %s\n"), SCErrorString(SCError()));
		return;
	}

	return;
}


void
do_notify_file(int argc, char **argv)
{
	int32_t		reqID = 0;
	int		fd;
	union {
		char	data[4];
		int32_t	gotID;
	} buf;
	char		*bufPtr;
	int		needed;

	if (argc == 1) {
		if ((sscanf(argv[0], "%d", &reqID) != 1)) {
			SCPrint(TRUE, stdout, CFSTR("invalid identifier.\n"));
			return;
		}
	}

	if (!SCDynamicStoreNotifyFileDescriptor(store, reqID, &fd)) {
		SCPrint(TRUE, stdout, CFSTR("  %s\n"), SCErrorString(SCError()));
		return;
	}

	bzero(buf.data, sizeof(buf.data));
	bufPtr = &buf.data[0];
	needed = sizeof(buf.gotID);
	while (needed > 0) {
		int	got;

		got = read(fd, bufPtr, needed);
		if (got == -1) {
			/* if error detected */
			SCPrint(TRUE, stdout, CFSTR("read() failed: %s.\n"), strerror(errno));
			break;
		}

		if (got == 0) {
			/* if end of file detected */
			SCPrint(TRUE, stdout, CFSTR("read(): detected end of file.\n"));
			break;
		}

		SCPrint(TRUE, stdout, CFSTR("Received %d bytes.\n"), got);
		bufPtr += got;
		needed -= got;
	}

	if (needed != sizeof(buf.gotID)) {
		SCPrint(TRUE, stdout, CFSTR("  Received notification, identifier = %d.\n"), buf.gotID);
	}

	/* this utility only allows processes one notification per "n.file" request */
	(void) SCDynamicStoreNotifyCancel(store);

	(void) close(fd);	/* close my side of the file descriptor */

	return;
}


static void
signalCatcher(int signum)
{
	static int	n = 0;

	SCPrint(TRUE, stdout, CFSTR("Received sig%s (#%d).\n"), sys_signame[signum], n++);
	return;
}


void
do_notify_signal(int argc, char **argv)
{
	int			sig;
	pid_t			pid;
	struct sigaction	nact;
	int			ret;

	if (isdigit(*argv[0])) {
		if ((sscanf(argv[0], "%d", &sig) != 1) || (sig <= 0) || (sig >= NSIG)) {
			SCPrint(TRUE, stdout, CFSTR("signal must be in the range of 1 .. %d.\n"), NSIG-1);
			return;
		}
	} else {
		for (sig=1; sig<NSIG; sig++) {
			if (strcasecmp(argv[0], sys_signame[sig]) == 0)
				break;
		}
		if (sig >= NSIG) {
			CFMutableStringRef	str;

			SCPrint(TRUE, stdout, CFSTR("Signal must be one of the following:\n"));

			str = CFStringCreateMutable(NULL, 0);
			for (sig=1; sig<NSIG; sig++) {
				CFStringAppendFormat(str, NULL, CFSTR(" %-6s"), sys_signame[sig]);
				if ((sig % 10) == 0) {
					CFStringAppendFormat(str, NULL, CFSTR("\n"));
				}
			}
			if ((sig % 10) != 0) {
				CFStringAppendFormat(str, NULL, CFSTR("\n"));
			}
			SCPrint(TRUE, stdout, CFSTR("%@"), str);
			CFRelease(str);
			return;
		}

	}

	if ((argc != 2) || (sscanf(argv[1], "%d", &pid) != 1)) {
		pid = getpid();
	}

	if (oact != NULL) {
		ret = sigaction(osig, oact, NULL);	/* restore original signal handler */
	} else {
		oact = malloc(sizeof(struct sigaction));
	}

	nact.sa_handler = signalCatcher;
	sigemptyset(&nact.sa_mask);
	nact.sa_flags = SA_RESTART;
	ret = sigaction(sig, &nact, oact);
	osig = sig;
	SCPrint(TRUE, stdout, CFSTR("signal handler started.\n"));

	if (!SCDynamicStoreNotifySignal(store, pid, sig)) {
		SCPrint(TRUE, stdout, CFSTR("  %s\n"), SCErrorString(SCError()));
		return;
	}

	return;
}


void
do_notify_cancel(int argc, char **argv)
{
	int			ret;

	if (notifyRls) {
		CFRunLoopRemoveSource(CFRunLoopGetCurrent(), notifyRls, kCFRunLoopDefaultMode);
		CFRelease(notifyRls);
		notifyRls = NULL;
	}

	if (!SCDynamicStoreNotifyCancel(store)) {
		SCPrint(TRUE, stdout, CFSTR("  %s\n"), SCErrorString(SCError()));
		return;
	}

	if (oact != NULL) {
		ret = sigaction(osig, oact, NULL);	/* restore original signal handler */
		free(oact);
		oact = NULL;
	}

	return;
}
