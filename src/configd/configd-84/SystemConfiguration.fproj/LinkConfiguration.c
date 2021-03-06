/*
 * Copyright (c) 2002-2003 Apple Computer, Inc. All rights reserved.
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

/*
 * Modification History
 *
 * October 21, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */


#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>		// for SCLog()
#include <SystemConfiguration/SCValidation.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/network/IONetworkController.h>
#include "dy_framework.h"


static const struct ifmedia_description ifm_subtype_shared_descriptions[] =
    IFM_SUBTYPE_SHARED_DESCRIPTIONS;

static const struct ifmedia_description ifm_subtype_ethernet_descriptions[] =
    IFM_SUBTYPE_ETHERNET_DESCRIPTIONS;

static const struct ifmedia_description ifm_shared_option_descriptions[] =
    IFM_SHARED_OPTION_DESCRIPTIONS;

static const struct ifmedia_description ifm_subtype_ethernet_option_descriptions[] =
    IFM_SUBTYPE_ETHERNET_OPTION_DESCRIPTIONS;


static CFDictionaryRef
__createMediaDictionary(int media_options, Boolean filter)
{
	CFMutableDictionaryRef	dict	= NULL;
	int			i;
	CFMutableArrayRef	options	= NULL;
	CFStringRef		val;

	if (IFM_TYPE(media_options) != IFM_ETHER) {
		return NULL;
	}

	if (filter && (IFM_SUBTYPE(media_options) == IFM_NONE)) {
		return NULL;	/* filter */
	}

	dict = CFDictionaryCreateMutable(NULL,
					 0,
					 &kCFTypeDictionaryKeyCallBacks,
					 &kCFTypeDictionaryValueCallBacks);

	/* subtype */

	val = NULL;
	for (i = 0; !val && ifm_subtype_shared_descriptions[i].ifmt_string; i++) {
		if (IFM_SUBTYPE(media_options) == ifm_subtype_shared_descriptions[i].ifmt_word) {
			val = CFStringCreateWithCString(NULL,
							ifm_subtype_shared_descriptions[i].ifmt_string,
							kCFStringEncodingASCII);
			break;
		}
	}

	for (i = 0; !val && ifm_subtype_ethernet_descriptions[i].ifmt_string; i++) {
		if (IFM_SUBTYPE(media_options) == ifm_subtype_ethernet_descriptions[i].ifmt_word) {
			val = CFStringCreateWithCString(NULL,
							ifm_subtype_ethernet_descriptions[i].ifmt_string,
							kCFStringEncodingASCII);
			break;
		}
	}

	if (val) {
		CFDictionaryAddValue(dict, kSCPropNetEthernetMediaSubType, val);
		CFRelease(val);
	}

	/* options */

	options = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	while (IFM_OPTIONS(media_options) != 0) {
		if (filter && (IFM_OPTIONS(media_options) & IFM_LOOP)) {
			media_options &= ~IFM_LOOP;	/* filter */
			continue;
		}

		val = NULL;
		for (i = 0; !val && ifm_shared_option_descriptions[i].ifmt_string; i++) {
			if (IFM_OPTIONS(media_options) & ifm_shared_option_descriptions[i].ifmt_word) {
				val = CFStringCreateWithCString(NULL,
								ifm_shared_option_descriptions[i].ifmt_string,
								kCFStringEncodingASCII);
				media_options &= ~ifm_shared_option_descriptions[i].ifmt_word;
				break;
			}
		}

		for (i = 0; !val && ifm_subtype_ethernet_option_descriptions[i].ifmt_string; i++) {
			if (IFM_OPTIONS(media_options) & ifm_subtype_ethernet_option_descriptions[i].ifmt_word) {
				val = CFStringCreateWithCString(NULL,
								ifm_subtype_ethernet_option_descriptions[i].ifmt_string,
								kCFStringEncodingASCII);
				media_options &= ~ifm_shared_option_descriptions[i].ifmt_word;
				break;
			}
		}

		if (val) {
			CFArrayAppendValue(options, val);
			CFRelease(val);
		}
	}

	CFDictionaryAddValue(dict, kSCPropNetEthernetMediaOptions, options);
	CFRelease(options);

	return dict;
}


int
__createMediaOptions(CFDictionaryRef media_options)
{
	CFIndex		i;
	Boolean		match;
	int		ifm_new	= IFM_ETHER;
	CFIndex		n;
	CFArrayRef	options;
	char		*str;
	CFStringRef	val;

	/* set subtype */

	val = CFDictionaryGetValue(media_options, kSCPropNetEthernetMediaSubType);
	if (!isA_CFString(val)) {
		return -1;
	}

	str = _SC_cfstring_to_cstring(val, NULL, 0, kCFStringEncodingASCII);
	if (str == NULL) {
		return -1;
	}

	match = FALSE;
	for (i = 0; !match && ifm_subtype_shared_descriptions[i].ifmt_string; i++) {
		if (strcasecmp(str, ifm_subtype_shared_descriptions[i].ifmt_string) == 0) {
			ifm_new |= ifm_subtype_shared_descriptions[i].ifmt_word;
			match = TRUE;
			break;
		}
	}

	for (i = 0; !match && ifm_subtype_ethernet_descriptions[i].ifmt_string; i++) {
		if (strcasecmp(str, ifm_subtype_ethernet_descriptions[i].ifmt_string) == 0) {
			ifm_new |= ifm_subtype_ethernet_descriptions[i].ifmt_word;
			match = TRUE;
			break;
		}
	}

	CFAllocatorDeallocate(NULL, str);

	if (!match) {
		return -1;	/* if no subtype */
	}

	/* set options */

	options = CFDictionaryGetValue(media_options, kSCPropNetEthernetMediaOptions);
	if (!isA_CFArray(options)) {
		return -1;
	}

	n = CFArrayGetCount(options);
	for (i = 0; i < n; i++) {
		CFIndex		j;

		val = CFArrayGetValueAtIndex(options, i);
		if (!isA_CFString(val)) {
			return -1;
		}

		str = _SC_cfstring_to_cstring(val, NULL, 0, kCFStringEncodingASCII);
		if (str == NULL) {
			return -1;
		}


		match = FALSE;
		for (j = 0; !match && ifm_shared_option_descriptions[j].ifmt_string; j++) {
			if (strcasecmp(str, ifm_shared_option_descriptions[j].ifmt_string) == 0) {
				ifm_new |= ifm_shared_option_descriptions[j].ifmt_word;
				match = TRUE;
				break;
			}
		}

		for (j = 0; !match && ifm_subtype_ethernet_option_descriptions[j].ifmt_string; j++) {
			if (strcasecmp(str, ifm_subtype_ethernet_option_descriptions[j].ifmt_string) == 0) {
				ifm_new |= ifm_subtype_ethernet_option_descriptions[j].ifmt_word;
				match = TRUE;
				break;
			}
		}

		CFAllocatorDeallocate(NULL, str);

		if (!match) {
			return -1;	/* if no option */
		}
	}

	return ifm_new;
}


Boolean
NetworkInterfaceCopyMediaOptions(CFStringRef		interface,
				 CFDictionaryRef	*current,
				 CFDictionaryRef	*active,
				 CFArrayRef		*available,
				 Boolean		filter)
{
	int			i;
	struct ifmediareq	ifm;
	int			*media_list	= NULL;
	Boolean			ok		= FALSE;
	int			sock		= -1;

	bzero((void *)&ifm, sizeof(ifm));

	if (_SC_cfstring_to_cstring(interface, ifm.ifm_name, sizeof(ifm.ifm_name), kCFStringEncodingASCII) == NULL) {
		SCLog(TRUE, LOG_ERR, CFSTR("could not convert inteface name"));
		goto done;
	}

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		SCLog(TRUE, LOG_ERR, CFSTR("socket() failed: %s"), strerror(errno));
		goto done;
	}

	if (ioctl(sock, SIOCGIFMEDIA, (caddr_t)&ifm) < 0) {
//		SCLog(TRUE, LOG_DEBUG, CFSTR("ioctl(SIOCGIFMEDIA) failed: %s"), strerror(errno));
		goto done;
	}

	if (ifm.ifm_count > 0) {
		media_list = (int *)CFAllocatorAllocate(NULL, ifm.ifm_count * sizeof(int), 0);
		ifm.ifm_ulist = media_list;
		if (ioctl(sock, SIOCGIFMEDIA, (caddr_t)&ifm) < 0) {
			SCLog(TRUE, LOG_DEBUG, CFSTR("ioctl(SIOCGIFMEDIA) failed: %s"), strerror(errno));
			goto done;
		}
	}

	if (active)	*active    = NULL;
	if (current)	*current   = NULL;
	if (available) {
		CFMutableArrayRef	media_options;

		media_options = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		for (i = 0; i < ifm.ifm_count; i++) {
			CFDictionaryRef	options;

			options = __createMediaDictionary(media_list[i], filter);
			if (!options) {
				continue;
			}

			if (active  && (*active == NULL)  && (ifm.ifm_active == media_list[i])) {
				*active  = CFRetain(options);
			}

			if (current && (*current == NULL) && (ifm.ifm_current == media_list[i])) {
				*current = CFRetain(options);
			}

			if (!CFArrayContainsValue(media_options, CFRangeMake(0, CFArrayGetCount(media_options)), options)) {
				CFArrayAppendValue(media_options, options);
			}

			CFRelease(options);
		}
		*available = (CFArrayRef)media_options;
	}

	if (active  && (*active == NULL)) {
		*active = __createMediaDictionary(ifm.ifm_active, FALSE);
	}

	if (current && (*current == NULL)) {
		if (active && (ifm.ifm_active == ifm.ifm_current)) {
			if (*active)	*current = CFRetain(active);
		} else {
			*current = __createMediaDictionary(ifm.ifm_current, FALSE);
		}
	}

	ok = TRUE;

    done :

	if (sock >= 0)	(void)close(sock);
	if (media_list)	CFAllocatorDeallocate(NULL, media_list);

	return ok;
}


CFArrayRef
NetworkInterfaceCopyMediaSubTypes(CFArrayRef	available)
{
	CFIndex			i;
	CFIndex			n;
	CFMutableArrayRef	subTypes;

	if (!isA_CFArray(available)) {
		return NULL;
	}

	subTypes = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	n = CFArrayGetCount(available);
	for (i = 0; i < n; i++) {
		CFDictionaryRef	options;
		CFStringRef	subType;

		options = CFArrayGetValueAtIndex(available, i);
		if (!isA_CFDictionary(options)) {
			continue;
		}

		subType = CFDictionaryGetValue(options, kSCPropNetEthernetMediaSubType);
		if (!isA_CFString(subType)) {
			continue;
		}

		if (!CFArrayContainsValue(subTypes, CFRangeMake(0, CFArrayGetCount(subTypes)), subType)) {
			CFArrayAppendValue(subTypes, subType);
		}
	}

	if (CFArrayGetCount(subTypes) == 0) {
		CFRelease(subTypes);
		subTypes = NULL;
	}

	return subTypes;
}


CFArrayRef
NetworkInterfaceCopyMediaSubTypeOptions(CFArrayRef	available,
					CFStringRef	subType)
{
	CFIndex			i;
	CFIndex			n;
	CFMutableArrayRef	subTypeOptions;

	if (!isA_CFArray(available)) {
		return NULL;
	}

	subTypeOptions = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	n = CFArrayGetCount(available);
	for (i = 0; i < n; i++) {
		CFDictionaryRef	options;
		CFArrayRef	mediaOptions;
		CFStringRef	mediaSubType;

		options = CFArrayGetValueAtIndex(available, i);
		if (!isA_CFDictionary(options)) {
			continue;
		}

		mediaSubType = CFDictionaryGetValue(options, kSCPropNetEthernetMediaSubType);
		if (!isA_CFString(mediaSubType) || !CFEqual(subType, mediaSubType)) {
			continue;
		}

		mediaOptions = CFDictionaryGetValue(options, kSCPropNetEthernetMediaOptions);
		if (!isA_CFArray(mediaOptions)) {
			continue;
		}

		if (!CFArrayContainsValue(subTypeOptions, CFRangeMake(0, CFArrayGetCount(subTypeOptions)), mediaOptions)) {
			CFArrayAppendValue(subTypeOptions, mediaOptions);
		}
	}

	if (CFArrayGetCount(subTypeOptions) == 0) {
		CFRelease(subTypeOptions);
		subTypeOptions = NULL;
	}

	return subTypeOptions;
}


Boolean
NetworkInterfaceCopyMTU(CFStringRef	interface,
			int		*mtu_cur,
			int		*mtu_min,
			int		*mtu_max)
{
	struct ifreq	ifr;
	Boolean		ok	= FALSE;
	int		sock	= -1;

	bzero((void *)&ifr, sizeof(ifr));
	if (_SC_cfstring_to_cstring(interface, ifr.ifr_name, sizeof(ifr.ifr_name), kCFStringEncodingASCII) == NULL) {
		SCLog(TRUE, LOG_ERR, CFSTR("could not convert inteface name"));
		goto done;
	}

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		SCLog(TRUE, LOG_ERR, CFSTR("socket() failed: %s"), strerror(errno));
		goto done;
	}

	if (ioctl(sock, SIOCGIFMTU, (caddr_t)&ifr) < 0) {
//		SCLog(TRUE, LOG_DEBUG, CFSTR("ioctl(SIOCGIFMTU) failed: %s"), strerror(errno));
		goto done;
	}

	if (mtu_cur)	*mtu_cur = ifr.ifr_mtu;

	/* get valid MTU range */

	if (mtu_min || mtu_max) {
		int			ifType		= 0;
		io_iterator_t		io_iter		= 0;
		io_registry_entry_t	io_interface	= 0;
		io_registry_entry_t	io_controller	= 0;
		kern_return_t		kr;
		mach_port_t		masterPort	= MACH_PORT_NULL;
		CFMutableDictionaryRef	matchingDict;

		/* assume that we don't know */

		if (mtu_min) *mtu_min = -1;
		if (mtu_max) *mtu_max = -1;

		/* look for a matching interface in the IORegistry */

		matchingDict = IOBSDNameMatching(masterPort, 0, ifr.ifr_name);
		if (matchingDict) {
			/* Note: IOServiceGetMatchingServices consumes a reference on the 'matchingDict' */
			kr = IOServiceGetMatchingServices(masterPort, matchingDict, &io_iter);
			if ((kr == KERN_SUCCESS) && io_iter) {
			    /* should only have a single match */
			    io_interface = IOIteratorNext(io_iter);
			}
			if (io_iter)	IOObjectRelease(io_iter);
		}

		/* found an interface with the given BSD name, get its parent */

		if (io_interface) {
			CFNumberRef	num;

			/*
			 * get the interface type
			 */
			num = IORegistryEntryCreateCFProperty(io_interface, CFSTR(kIOInterfaceType), NULL, kNilOptions);
			if (num) {
				if (isA_CFNumber(num)) {
					CFNumberGetValue(num, kCFNumberIntType, &ifType);
				}
				CFRelease(num);
		       }

			/*
			 * ...and the property we are REALLY interested is in the controller,
			 * which is the parent of the interface object.
			 */
			(void)IORegistryEntryGetParentEntry(io_interface, kIOServicePlane, &io_controller);

			IOObjectRelease(io_interface);
		}

		if (io_controller) {
			CFNumberRef	num;

			num = IORegistryEntryCreateCFProperty(io_controller, CFSTR(kIOMaxPacketSize), NULL, kNilOptions);
			if (num) {
				if (isA_CFNumber(num)) {
					int	value;

					/*
					 * Get the value and subtract the FCS bytes and Ethernet header
					 * sizes from the maximum frame size reported by the controller
					 * to get the MTU size. The 14 byte media header can be found
					 * in the registry, but not the size for the trailing FCS bytes.
					 */
					CFNumberGetValue(num, kCFNumberIntType, &value);

					if (ifType == IFT_ETHER) {
						value -= (ETHER_HDR_LEN + ETHER_CRC_LEN);
					}

					if (mtu_min)	*mtu_min = IF_MINMTU;
					if (mtu_max)	*mtu_max = value;
				}
				CFRelease(num);
			}

			IOObjectRelease(io_controller);
		}
	}

	ok = TRUE;

    done :

	if (sock >= 0)	(void)close(sock);

	return ok;
}
