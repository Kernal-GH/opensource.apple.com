/*
 * Copyright (c) 2008-2010 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 ** @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1983, 1988, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
/*
static char sccsid[] = "@(#)inet.c	8.5 (Berkeley) 5/24/95";
*/
static const char rcsid[] =
	"$Id: inet.c,v 1.9 2006/04/04 04:36:27 lindak Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>

#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#ifdef INET6
#include <netinet/ip6.h>
#endif /* INET6 */
#include <netinet/in_pcb.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp_var.h>
#include <netinet/igmp_var.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_seq.h>
#define TCPSTATES
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

#include <arpa/inet.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "netstat.h"

#define ROUNDUP64(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(uint64_t) - 1))) : sizeof(uint64_t))
#define ADVANCE64(x, n) (((char *)x) += ROUNDUP64(n))

char	*inetname (struct in_addr *);
void	inetprint (struct in_addr *, int, char *, int);
#ifdef INET6
extern void	inet6print (struct in6_addr *, int, char *, int);
static int udp_done, tcp_done;
#endif /* INET6 */

#ifdef SRVCACHE
typedef struct __table_private table_t;

extern table_t *_nc_table_new(uint32_t n);
extern void _nc_table_free(table_t *tin);

extern void _nc_table_insert(table_t *t, const char *key, void *datum);
extern void *_nc_table_find(table_t *t, const char *key);
extern void _nc_table_delete(table_t *t, const char *key);

static table_t *_serv_cache = NULL;

/*
 * Read and cache all known services
 */
static void
_serv_cache_open()
{
	struct servent *s;
	char *key, *name, *test;

	if (_serv_cache != NULL) return;

	_serv_cache = _nc_table_new(8192);
	setservent(0);

	while (NULL != (s = getservent()))
	{
		if (s->s_name == NULL) continue;
		key = NULL;
		asprintf(&key, "%hu/%s", (unsigned short)ntohs(s->s_port), s->s_proto);
		name = strdup(s->s_name);
		test = _nc_table_find(_serv_cache, key);
		if (test == NULL) _nc_table_insert(_serv_cache, key, name);
		free(key);
	}

	endservent();
}

void
_serv_cache_close()
{
	_nc_table_free(_serv_cache);
	_serv_cache = NULL;
}

struct servent *
_serv_cache_getservbyport(int port, char *proto)
{
	static struct servent s;
	char *key;
	unsigned short p;

	_serv_cache_open();

	memset(&s, 0, sizeof(struct servent));
	asprintf(&key, "%u/%s", port, (proto == NULL) ? "udp" : proto);

	s.s_name = _nc_table_find(_serv_cache, key);
	free(key);
	if (s.s_name == NULL) return NULL;

	p = port;
	s.s_port = htons(p);
	s.s_proto = proto;
	return &s;
}

#endif SRVCACHE
	
/*
 * Print a summary of connections related to an Internet
 * protocol.  For TCP, also give state of connection.
 * Listening processes (aflag) are suppressed unless the
 * -a (all) flag is specified.
 */
#if !TARGET_OS_EMBEDDED

struct xgen_n {
	u_int32_t	xgn_len;			/* length of this structure */
	u_int32_t	xgn_kind;		/* number of PCBs at this time */
};

#define ALL_XGN_KIND_INP (XSO_SOCKET | XSO_RCVBUF | XSO_SNDBUF | XSO_STATS | XSO_INPCB)
#define ALL_XGN_KIND_TCP (ALL_XGN_KIND_INP | XSO_TCPCB)

void
protopr(uint32_t proto,		/* for sysctl version we pass proto # */
		char *name, int af)
{
	int istcp;
	static int first = 1;
	char *buf, *next;
	const char *mibvar;
	struct xinpgen *xig, *oxig;
	struct xgen_n *xgn;
	size_t len;
	struct xtcpcb_n *tp = NULL;
	struct xinpcb_n *inp = NULL;
	struct xsocket_n *so = NULL;
	struct xsockbuf_n *so_rcv = NULL;
	struct xsockbuf_n *so_snd = NULL;
	struct xsockstat_n *so_stat = NULL;
	int which = 0;
	
	istcp = 0;
	switch (proto) {
		case IPPROTO_TCP:
#ifdef INET6
			if (tcp_done != 0)
				return;
			else
				tcp_done = 1;
#endif
			istcp = 1;
			mibvar = "net.inet.tcp.pcblist_n";
			break;
		case IPPROTO_UDP:
#ifdef INET6
			if (udp_done != 0)
				return;
			else
				udp_done = 1;
#endif
			mibvar = "net.inet.udp.pcblist_n";
			break;
		case IPPROTO_DIVERT:
			mibvar = "net.inet.divert.pcblist_n";
			break;
		default:
			mibvar = "net.inet.raw.pcblist_n";
			break;
	}
	len = 0;
	if (sysctlbyname(mibvar, 0, &len, 0, 0) < 0) {
		if (errno != ENOENT)
			warn("sysctl: %s", mibvar);
		return;
	}        
	if ((buf = malloc(len)) == 0) {
		warn("malloc %lu bytes", (u_long)len);
		return;
	}
	if (sysctlbyname(mibvar, buf, &len, 0, 0) < 0) {
		warn("sysctl: %s", mibvar);
		free(buf);
		return;
	}
	
	/*
	 * Bail-out to avoid logic error in the loop below when
	 * there is in fact no more control block to process
	 */
	if (len <= sizeof(struct xinpgen)) {
		free(buf);
		return;
	}
	
	oxig = xig = (struct xinpgen *)buf;
	for (next = buf + ROUNDUP64(xig->xig_len); next < buf + len; next += ROUNDUP64(xgn->xgn_len)) {
		
		xgn = (struct xgen_n*)next;
		if (xgn->xgn_len <= sizeof(struct xinpgen))
			break;
		
		if ((which & xgn->xgn_kind) == 0) {
			which |= xgn->xgn_kind;
			switch (xgn->xgn_kind) {
				case XSO_SOCKET:
					so = (struct xsocket_n *)xgn;
					break;
				case XSO_RCVBUF:
					so_rcv = (struct xsockbuf_n *)xgn;
					break;
				case XSO_SNDBUF:
					so_snd = (struct xsockbuf_n *)xgn;
					break;
				case XSO_STATS:
					so_stat = (struct xsockstat_n *)xgn;
					break;
				case XSO_INPCB:
					inp = (struct xinpcb_n *)xgn;
					break;
				case XSO_TCPCB:
					tp = (struct xtcpcb_n *)xgn;
					break;
				default:
					printf("unexpected kind %d\n", xgn->xgn_kind);
					break;
			} 		
		} else {
			printf("got %d twice\n", xgn->xgn_kind);
		}
		
		if ((istcp && which != ALL_XGN_KIND_TCP) || (!istcp && which != ALL_XGN_KIND_INP))
			continue;
		which = 0;
		
		/* Ignore sockets for protocols other than the desired one. */
		if (so->xso_protocol != (int)proto)
			continue;
		
		/* Ignore PCBs which were freed during copyout. */
		if (inp->inp_gencnt > oxig->xig_gen)
			continue;
		
		if ((af == AF_INET && (inp->inp_vflag & INP_IPV4) == 0)
#ifdef INET6
		    || (af == AF_INET6 && (inp->inp_vflag & INP_IPV6) == 0)
#endif /* INET6 */
		    || (af == AF_UNSPEC && ((inp->inp_vflag & INP_IPV4) == 0
#ifdef INET6
									&& (inp->inp_vflag &
										INP_IPV6) == 0
#endif /* INET6 */
									))
		    )
			continue;
		
		/*
		 * Local address is not an indication of listening socket or
		 * server sockey but just rather the socket has been bound.
		 * That why many UDP sockets were not displayed in the original code.
		 */
		if (!aflag && istcp && tp->t_state <= TCPS_LISTEN)
			continue;
		
		if (Lflag && !so->so_qlimit)
			continue;
		
		if (first) {
			if (!Lflag) {
				printf("Active Internet connections");
				if (aflag)
					printf(" (including servers)");
			} else
				printf(
					   "Current listen queue sizes (qlen/incqlen/maxqlen)");
			putchar('\n');
			if (Aflag)
#if !TARGET_OS_EMBEDDED
				printf("%-16.16s ", "Socket");
#else
			printf("%-8.8s ", "Socket");
#endif
			if (Lflag)
				printf("%-14.14s %-22.22s\n",
					   "Listen", "Local Address");
			else {
				printf((Aflag && !Wflag) ?
					   "%-5.5s %-6.6s %-6.6s  %-18.18s %-18.18s %-11.11s" :
					   "%-5.5s %-6.6s %-6.6s  %-22.22s %-22.22s %-11.11s",
					   "Proto", "Recv-Q", "Send-Q",
					   "Local Address", "Foreign Address",
					   "(state)");
				if (bflag > 0)
					printf(" %10.10s %10.10s", "rxbytes", "txbytes");
				if (prioflag >= 0)
					printf(" %7.7s[%1d] %7.7s[%1d]", "rxbytes", prioflag, "txbytes", prioflag);
				printf("\n");
			}
			first = 0;
		}
		if (Aflag) {
			if (istcp)
#if !TARGET_OS_EMBEDDED
				printf("%16lx ", (u_long)inp->inp_ppcb);
#else
			printf("%8lx ", (u_long)inp->inp_ppcb);
			
#endif
			else
#if !TARGET_OS_EMBEDDED
				printf("%16lx ", (u_long)so->so_pcb);
#else
			printf("%8lx ", (u_long)so->so_pcb);
#endif
		}
		if (Lflag) {
			char buf[15];
			
			snprintf(buf, 15, "%d/%d/%d", so->so_qlen,
					 so->so_incqlen, so->so_qlimit);
			printf("%-14.14s ", buf);
		}
		else {
			const char *vchar;
			
#ifdef INET6
			if ((inp->inp_vflag & INP_IPV6) != 0)
				vchar = ((inp->inp_vflag & INP_IPV4) != 0)
				? "46" : "6 ";
			else
#endif
				vchar = ((inp->inp_vflag & INP_IPV4) != 0)
				? "4 " : "  ";
			
			printf("%-3.3s%-2.2s %6u %6u  ", name, vchar,
			       so_rcv->sb_cc,
			       so_snd->sb_cc);
		}
		if (nflag) {
			if (inp->inp_vflag & INP_IPV4) {
				inetprint(&inp->inp_laddr, (int)inp->inp_lport,
						  name, 1);
				if (!Lflag)
					inetprint(&inp->inp_faddr,
							  (int)inp->inp_fport, name, 1);
			}
#ifdef INET6
			else if (inp->inp_vflag & INP_IPV6) {
				inet6print(&inp->in6p_laddr,
						   (int)inp->inp_lport, name, 1);
				if (!Lflag)
					inet6print(&inp->in6p_faddr,
							   (int)inp->inp_fport, name, 1);
			} /* else nothing printed now */
#endif /* INET6 */
		} else if (inp->inp_flags & INP_ANONPORT) {
			if (inp->inp_vflag & INP_IPV4) {
				inetprint(&inp->inp_laddr, (int)inp->inp_lport,
						  name, 1);
				if (!Lflag)
					inetprint(&inp->inp_faddr,
							  (int)inp->inp_fport, name, 0);
			}
#ifdef INET6
			else if (inp->inp_vflag & INP_IPV6) {
				inet6print(&inp->in6p_laddr,
						   (int)inp->inp_lport, name, 1);
				if (!Lflag)
					inet6print(&inp->in6p_faddr,
							   (int)inp->inp_fport, name, 0);
			} /* else nothing printed now */
#endif /* INET6 */
		} else {
			if (inp->inp_vflag & INP_IPV4) {
				inetprint(&inp->inp_laddr, (int)inp->inp_lport,
						  name, 0);
				if (!Lflag)
					inetprint(&inp->inp_faddr,
							  (int)inp->inp_fport, name,
							  inp->inp_lport !=
							  inp->inp_fport);
			}
#ifdef INET6
			else if (inp->inp_vflag & INP_IPV6) {
				inet6print(&inp->in6p_laddr,
						   (int)inp->inp_lport, name, 0);
				if (!Lflag)
					inet6print(&inp->in6p_faddr,
							   (int)inp->inp_fport, name,
							   inp->inp_lport !=
							   inp->inp_fport);
			} /* else nothing printed now */
#endif /* INET6 */
		}
		if (istcp && !Lflag) {
			if (tp->t_state < 0 || tp->t_state >= TCP_NSTATES)
				printf("%-11d", tp->t_state);
			else {
				printf("%-11s", tcpstates[tp->t_state]);
#if defined(TF_NEEDSYN) && defined(TF_NEEDFIN)
				/* Show T/TCP `hidden state' */
				if (tp->t_flags & (TF_NEEDSYN|TF_NEEDFIN))
					putchar('*');
#endif /* defined(TF_NEEDSYN) && defined(TF_NEEDFIN) */
			}
		}
		if (!istcp)
			printf("%-11s", "           ");
		if (bflag > 0) {
			int i;
			u_int64_t rxbytes = 0;
			u_int64_t txbytes = 0;
			
			for (i = 0; i < SO_TC_MAX; i++) {
				rxbytes += so_stat->xst_tc_stats[i].rxbytes;
				txbytes += so_stat->xst_tc_stats[i].txbytes;
			}
			
			printf(" %10llu %10llu", rxbytes, txbytes);
		}
		if (prioflag >= 0) {
			printf(" %10llu %10llu", 
				   prioflag < SO_TC_MAX ? so_stat->xst_tc_stats[prioflag].rxbytes : 0, 
				   prioflag < SO_TC_MAX ? so_stat->xst_tc_stats[prioflag].txbytes : 0);
		}
		putchar('\n');
	}
	if (xig != oxig && xig->xig_gen != oxig->xig_gen) {
		if (oxig->xig_count > xig->xig_count) {
			printf("Some %s sockets may have been deleted.\n",
			       name);
		} else if (oxig->xig_count < xig->xig_count) {
			printf("Some %s sockets may have been created.\n",
			       name);
		} else {
			printf("Some %s sockets may have been created or deleted",
			       name);
		}
	}
	free(buf);
}

#else /* TARGET_OS_EMBEDDED */

void
protopr(uint32_t proto,		/* for sysctl version we pass proto # */
	char *name, int af)
{
	int istcp;
	static int first = 1;
	char *buf;
	const char *mibvar;
	struct xinpgen *xig, *oxig;
	struct tcpcb *tp = NULL;
	struct inpcb *inp;
	struct xsocket *so;
	size_t len;

	istcp = 0;
	switch (proto) {
	case IPPROTO_TCP:
#ifdef INET6
		if (tcp_done != 0)
			return;
		else
			tcp_done = 1;
#endif
		istcp = 1;
		mibvar = "net.inet.tcp.pcblist";
			break;
	case IPPROTO_UDP:
#ifdef INET6
		if (udp_done != 0)
			return;
		else
			udp_done = 1;
#endif
		mibvar = "net.inet.udp.pcblist";
			break;
	case IPPROTO_DIVERT:
		mibvar = "net.inet.divert.pcblist";
			break;
	default:
		mibvar = "net.inet.raw.pcblist";
			break;
	}
	len = 0;
	if (sysctlbyname(mibvar, 0, &len, 0, 0) < 0) {
		if (errno != ENOENT)
			warn("sysctl: %s", mibvar);
		return;
	}        
	if ((buf = malloc(len)) == 0) {
		warn("malloc %lu bytes", (u_long)len);
		return;
	}
	if (sysctlbyname(mibvar, buf, &len, 0, 0) < 0) {
		warn("sysctl: %s", mibvar);
		free(buf);
		return;
	}
        
	/*
	 * Bail-out to avoid logic error in the loop below when
	 * there is in fact no more control block to process
	 */
	if (len <= sizeof(struct xinpgen)) {
		free(buf);
		return;
	}
		
	oxig = xig = (struct xinpgen *)buf;
	for (xig = (struct xinpgen *)((char *)xig + xig->xig_len);
	     xig->xig_len > sizeof(struct xinpgen);
	     xig = (struct xinpgen *)((char *)xig + xig->xig_len)) {
		if (istcp) {
			tp = &((struct xtcpcb *)xig)->xt_tp;
			inp = &((struct xtcpcb *)xig)->xt_inp;
			so = &((struct xtcpcb *)xig)->xt_socket;
		} else {
			inp = &((struct xinpcb *)xig)->xi_inp;
			so = &((struct xinpcb *)xig)->xi_socket;
		}

		/* Ignore sockets for protocols other than the desired one. */
		if (so->xso_protocol != (int)proto)
			continue;

		/* Ignore PCBs which were freed during copyout. */
		if (inp->inp_gencnt > oxig->xig_gen)
			continue;

		if ((af == AF_INET && (inp->inp_vflag & INP_IPV4) == 0)
#ifdef INET6
		    || (af == AF_INET6 && (inp->inp_vflag & INP_IPV6) == 0)
#endif /* INET6 */
		    || (af == AF_UNSPEC && ((inp->inp_vflag & INP_IPV4) == 0
#ifdef INET6
					    && (inp->inp_vflag &
						INP_IPV6) == 0
#endif /* INET6 */
			))
		    )
			continue;

                /*
                 * Local address is not an indication of listening socket or
                 * server sockey but just rather the socket has been bound.
                 * That why many UDP sockets were not displayed in the original code.
                 */
                if (!aflag && istcp && tp->t_state <= TCPS_LISTEN)
                    continue;

                if (Lflag && !so->so_qlimit)
                    continue;

                if (first) {
			if (!Lflag) {
				printf("Active Internet connections");
				if (aflag)
					printf(" (including servers)");
			} else
				printf(
	"Current listen queue sizes (qlen/incqlen/maxqlen)");
			putchar('\n');
			if (Aflag)
				printf("%-8.8s ", "Socket");
			if (Lflag)
				printf("%-14.14s %-22.22s\n",
					"Listen", "Local Address");
			else
				printf((Aflag && !Wflag) ?
		"%-5.5s %-6.6s %-6.6s  %-18.18s %-18.18s %s\n" :
		"%-5.5s %-6.6s %-6.6s  %-22.22s %-22.22s %s\n",
					"Proto", "Recv-Q", "Send-Q",
					"Local Address", "Foreign Address",
					"(state)");
			first = 0;
		}
                if (Aflag) {
                        if (istcp)
                                printf("%8lx ", (u_long)inp->inp_ppcb);
                        else
                                printf("%8lx ", (u_long)so->so_pcb);
                }
		if (Lflag) {
				char buf[15];

				snprintf(buf, 15, "%d/%d/%d", so->so_qlen,
					 so->so_incqlen, so->so_qlimit);
				printf("%-14.14s ", buf);
                }
		else {
			const char *vchar;

#ifdef INET6
			if ((inp->inp_vflag & INP_IPV6) != 0)
				vchar = ((inp->inp_vflag & INP_IPV4) != 0)
					? "46" : "6 ";
			else
#endif
			vchar = ((inp->inp_vflag & INP_IPV4) != 0)
					? "4 " : "  ";

			printf("%-3.3s%-2.2s %6u %6u  ", name, vchar,
			       so->so_rcv.sb_cc,
			       so->so_snd.sb_cc);
		}
		if (nflag) {
			if (inp->inp_vflag & INP_IPV4) {
				inetprint(&inp->inp_laddr, (int)inp->inp_lport,
					  name, 1);
				if (!Lflag)
					inetprint(&inp->inp_faddr,
						  (int)inp->inp_fport, name, 1);
			}
#ifdef INET6
			else if (inp->inp_vflag & INP_IPV6) {
				inet6print(&inp->in6p_laddr,
					   (int)inp->inp_lport, name, 1);
				if (!Lflag)
					inet6print(&inp->in6p_faddr,
						   (int)inp->inp_fport, name, 1);
			} /* else nothing printed now */
#endif /* INET6 */
		} else if (inp->inp_flags & INP_ANONPORT) {
			if (inp->inp_vflag & INP_IPV4) {
				inetprint(&inp->inp_laddr, (int)inp->inp_lport,
					  name, 1);
				if (!Lflag)
					inetprint(&inp->inp_faddr,
						  (int)inp->inp_fport, name, 0);
			}
#ifdef INET6
			else if (inp->inp_vflag & INP_IPV6) {
				inet6print(&inp->in6p_laddr,
					   (int)inp->inp_lport, name, 1);
				if (!Lflag)
					inet6print(&inp->in6p_faddr,
						   (int)inp->inp_fport, name, 0);
			} /* else nothing printed now */
#endif /* INET6 */
		} else {
			if (inp->inp_vflag & INP_IPV4) {
				inetprint(&inp->inp_laddr, (int)inp->inp_lport,
					  name, 0);
				if (!Lflag)
					inetprint(&inp->inp_faddr,
						  (int)inp->inp_fport, name,
						  inp->inp_lport !=
							inp->inp_fport);
			}
#ifdef INET6
			else if (inp->inp_vflag & INP_IPV6) {
				inet6print(&inp->in6p_laddr,
					   (int)inp->inp_lport, name, 0);
				if (!Lflag)
					inet6print(&inp->in6p_faddr,
						   (int)inp->inp_fport, name,
						   inp->inp_lport !=
							inp->inp_fport);
			} /* else nothing printed now */
#endif /* INET6 */
		}
		if (istcp && !Lflag) {
			if (tp->t_state < 0 || tp->t_state >= TCP_NSTATES)
				printf("%d", tp->t_state);
                      else {
				printf("%s", tcpstates[tp->t_state]);
#if defined(TF_NEEDSYN) && defined(TF_NEEDFIN)
                              /* Show T/TCP `hidden state' */
                              if (tp->t_flags & (TF_NEEDSYN|TF_NEEDFIN))
                                      putchar('*');
#endif /* defined(TF_NEEDSYN) && defined(TF_NEEDFIN) */
                      }
		}
		putchar('\n');
	}
	if (xig != oxig && xig->xig_gen != oxig->xig_gen) {
		if (oxig->xig_count > xig->xig_count) {
			printf("Some %s sockets may have been deleted.\n",
			       name);
		} else if (oxig->xig_count < xig->xig_count) {
			printf("Some %s sockets may have been created.\n",
			       name);
		} else {
			printf("Some %s sockets may have been created or deleted",
			       name);
		}
	}
	free(buf);
}
#endif /* TARGET_OS_EMBEDDED */

/*
 * Dump TCP statistics structure.
 */
void
tcp_stats(uint32_t off , char *name, int af )
{
	static struct tcpstat ptcpstat;
	struct tcpstat tcpstat;
	size_t len = sizeof tcpstat;

	if (sysctlbyname("net.inet.tcp.stats", &tcpstat, &len, 0, 0) < 0) {
		warn("sysctl: net.inet.tcp.stats");
		return;
	}

#ifdef INET6
	if (tcp_done != 0 && interval == 0)
		return;
	else
		tcp_done = 1;
#endif

	printf ("%s:\n", name);

#define	TCPDIFF(f) (tcpstat.f - ptcpstat.f)
#define	p(f, m) if (TCPDIFF(f) || sflag <= 1) \
    printf(m, TCPDIFF(f), plural(TCPDIFF(f)))
#define	p1a(f, m) if (TCPDIFF(f) || sflag <= 1) \
    printf(m, TCPDIFF(f))
#define	p2(f1, f2, m) if (TCPDIFF(f1) || TCPDIFF(f2) || sflag <= 1) \
    printf(m, TCPDIFF(f1), plural(TCPDIFF(f1)), TCPDIFF(f2), plural(TCPDIFF(f2)))
#define	p2a(f1, f2, m) if (TCPDIFF(f1) || TCPDIFF(f2) || sflag <= 1) \
    printf(m, TCPDIFF(f1), plural(TCPDIFF(f1)), TCPDIFF(f2))
#define	p3(f, m) if (TCPDIFF(f) || sflag <= 1) \
    printf(m, TCPDIFF(f), plurales(TCPDIFF(f)))

	p(tcps_sndtotal, "\t%u packet%s sent\n");
	p2(tcps_sndpack,tcps_sndbyte,
		"\t\t%u data packet%s (%u byte%s)\n");
	p2(tcps_sndrexmitpack, tcps_sndrexmitbyte,
		"\t\t%u data packet%s (%u byte%s) retransmitted\n");
	p(tcps_mturesent, "\t\t%u resend%s initiated by MTU discovery\n");
	p2a(tcps_sndacks, tcps_delack,
		"\t\t%u ack-only packet%s (%u delayed)\n");
	p(tcps_sndurg, "\t\t%u URG only packet%s\n");
	p(tcps_sndprobe, "\t\t%u window probe packet%s\n");
	p(tcps_sndwinup, "\t\t%u window update packet%s\n");
	p(tcps_sndctrl, "\t\t%u control packet%s\n");
	p(tcps_rcvtotal, "\t%u packet%s received\n");
	p2(tcps_rcvackpack, tcps_rcvackbyte, "\t\t%u ack%s (for %u byte%s)\n");
	p(tcps_rcvdupack, "\t\t%u duplicate ack%s\n");
	p(tcps_rcvacktoomuch, "\t\t%u ack%s for unsent data\n");
	p2(tcps_rcvpack, tcps_rcvbyte,
		"\t\t%u packet%s (%u byte%s) received in-sequence\n");
	p2(tcps_rcvduppack, tcps_rcvdupbyte,
		"\t\t%u completely duplicate packet%s (%u byte%s)\n");
	p(tcps_pawsdrop, "\t\t%u old duplicate packet%s\n");
	p2(tcps_rcvpartduppack, tcps_rcvpartdupbyte,
		"\t\t%u packet%s with some dup. data (%u byte%s duped)\n");
	p2(tcps_rcvoopack, tcps_rcvoobyte,
		"\t\t%u out-of-order packet%s (%u byte%s)\n");
	p2(tcps_rcvpackafterwin, tcps_rcvbyteafterwin,
		"\t\t%u packet%s (%u byte%s) of data after window\n");
	p(tcps_rcvwinprobe, "\t\t%u window probe%s\n");
	p(tcps_rcvwinupd, "\t\t%u window update packet%s\n");
	p(tcps_rcvafterclose, "\t\t%u packet%s received after close\n");
	p(tcps_badrst, "\t\t%u bad reset%s\n");
	p(tcps_rcvbadsum, "\t\t%u discarded for bad checksum%s\n");
	p(tcps_rcvbadoff, "\t\t%u discarded for bad header offset field%s\n");
	p1a(tcps_rcvshort, "\t\t%u discarded because packet too short\n");
	p(tcps_connattempt, "\t%u connection request%s\n");
	p(tcps_accepts, "\t%u connection accept%s\n");
	p(tcps_badsyn, "\t%u bad connection attempt%s\n");
	p(tcps_listendrop, "\t%u listen queue overflow%s\n");
	p(tcps_connects, "\t%u connection%s established (including accepts)\n");
	p2(tcps_closed, tcps_drops,
		"\t%u connection%s closed (including %u drop%s)\n");
	p(tcps_cachedrtt, "\t\t%u connection%s updated cached RTT on close\n");
	p(tcps_cachedrttvar, 
	  "\t\t%u connection%s updated cached RTT variance on close\n");
	p(tcps_cachedssthresh,
	  "\t\t%u connection%s updated cached ssthresh on close\n");
	p(tcps_conndrops, "\t%u embryonic connection%s dropped\n");
	p2(tcps_rttupdated, tcps_segstimed,
		"\t%u segment%s updated rtt (of %u attempt%s)\n");
	p(tcps_rexmttimeo, "\t%u retransmit timeout%s\n");
	p(tcps_timeoutdrop, "\t\t%u connection%s dropped by rexmit timeout\n");
	p(tcps_rxtfindrop, "\t\t%u connection%s dropped after retransmitting FIN\n");
	p(tcps_persisttimeo, "\t%u persist timeout%s\n");
	p(tcps_persistdrop, "\t\t%u connection%s dropped by persist timeout\n");
	p(tcps_keeptimeo, "\t%u keepalive timeout%s\n");
	p(tcps_keepprobe, "\t\t%u keepalive probe%s sent\n");
	p(tcps_keepdrops, "\t\t%u connection%s dropped by keepalive\n");
	p(tcps_predack, "\t%u correct ACK header prediction%s\n");
	p(tcps_preddat, "\t%u correct data packet header prediction%s\n");
#ifdef TCP_MAX_SACK
	/* TCP_MAX_SACK indicates the header has the SACK structures */
	p(tcps_sack_recovery_episode, "\t%u SACK recovery episode%s\n"); 
	p(tcps_sack_rexmits,
		"\t%u segment rexmit%s in SACK recovery episodes\n");
	p(tcps_sack_rexmit_bytes,
		"\t%u byte rexmit%s in SACK recovery episodes\n"); 
	p(tcps_sack_rcv_blocks,
		"\t%u SACK option%s (SACK blocks) received\n"); 
	p(tcps_sack_send_blocks, "\t%u SACK option%s (SACK blocks) sent\n"); 
	p1a(tcps_sack_sboverflow, "\t%u SACK scoreboard overflow\n"); 
#endif /* TCP_MAX_SACK */

	if (interval > 0)
		bcopy(&tcpstat, &ptcpstat, len);

#undef TCPDIFF
#undef p
#undef p1a
#undef p2
#undef p2a
#undef p3
}

/*
 * Dump UDP statistics structure.
 */
void
udp_stats(uint32_t off , char *name, int af )
{
	static struct udpstat pudpstat;
	struct udpstat udpstat;
	size_t len = sizeof udpstat;
	uint32_t delivered;

	if (sysctlbyname("net.inet.udp.stats", &udpstat, &len, 0, 0) < 0) {
		warn("sysctl: net.inet.udp.stats");
		return;
	}

#ifdef INET6
	if (udp_done != 0 && interval == 0)
		return;
	else
		udp_done = 1;
#endif

	printf("%s:\n", name);

#define	UDPDIFF(f) (udpstat.f - pudpstat.f)
#define	p(f, m) if (UDPDIFF(f) || sflag <= 1) \
    printf(m, UDPDIFF(f), plural(UDPDIFF(f)))
#define	p1a(f, m) if (UDPDIFF(f) || sflag <= 1) \
    printf(m, UDPDIFF(f))
	p(udps_ipackets, "\t%u datagram%s received\n");
	p1a(udps_hdrops, "\t%u with incomplete header\n");
	p1a(udps_badlen, "\t%u with bad data length field\n");
	p1a(udps_badsum, "\t%u with bad checksum\n");
	p1a(udps_noport, "\t%u dropped due to no socket\n");
	p(udps_noportbcast,
	    "\t%u broadcast/multicast datagram%s dropped due to no socket\n");
	p1a(udps_fullsock, "\t%u dropped due to full socket buffers\n");
	p1a(udpps_pcbhashmiss, "\t%u not for hashed pcb\n");
	delivered = UDPDIFF(udps_ipackets) -
		    UDPDIFF(udps_hdrops) -
		    UDPDIFF(udps_badlen) -
		    UDPDIFF(udps_badsum) -
		    UDPDIFF(udps_noport) -
		    UDPDIFF(udps_noportbcast) -
		    UDPDIFF(udps_fullsock);
	if (delivered || sflag <= 1)
		printf("\t%u delivered\n", delivered);
	p(udps_opackets, "\t%u datagram%s output\n");

	if (interval > 0)
		bcopy(&udpstat, &pudpstat, len);

#undef UDPDIFF
#undef p
#undef p1a
}

/*
 * Dump IP statistics structure.
 */
void
ip_stats(uint32_t off , char *name, int af )
{
	static struct ipstat pipstat;
	struct ipstat ipstat;
	size_t len = sizeof ipstat;

	if (sysctlbyname("net.inet.ip.stats", &ipstat, &len, 0, 0) < 0) {
		warn("sysctl: net.inet.ip.stats");
		return;
	}

	printf("%s:\n", name);

#define	IPDIFF(f) (ipstat.f - pipstat.f)
#define	p(f, m) if (IPDIFF(f) || sflag <= 1) \
    printf(m, IPDIFF(f), plural(IPDIFF(f)))
#define	p1a(f, m) if (IPDIFF(f) || sflag <= 1) \
    printf(m, IPDIFF(f))

	p(ips_total, "\t%u total packet%s received\n");
	p(ips_badsum, "\t%u bad header checksum%s\n");
	p1a(ips_toosmall, "\t%u with size smaller than minimum\n");
	p1a(ips_tooshort, "\t%u with data size < data length\n");
	p1a(ips_toolong, "\t%u with ip length > max ip packet size\n");
	p1a(ips_badhlen, "\t%u with header length < data size\n");
	p1a(ips_badlen, "\t%u with data length < header length\n");
	p1a(ips_badoptions, "\t%u with bad options\n");
	p1a(ips_badvers, "\t%u with incorrect version number\n");
	p(ips_fragments, "\t%u fragment%s received\n");
	p(ips_fragdropped, "\t%u fragment%s dropped (dup or out of space)\n");
	p(ips_fragtimeout, "\t%u fragment%s dropped after timeout\n");
	p(ips_reassembled, "\t%u packet%s reassembled ok\n");
	p(ips_delivered, "\t%u packet%s for this host\n");
	p(ips_noproto, "\t%u packet%s for unknown/unsupported protocol\n");
	p(ips_forward, "\t%u packet%s forwarded");
	p(ips_fastforward, " (%u packet%s fast forwarded)");
	if (IPDIFF(ips_forward) || sflag <= 1)
		putchar('\n');
	p(ips_cantforward, "\t%u packet%s not forwardable\n");
	p(ips_notmember,
	  "\t%u packet%s received for unknown multicast group\n");
	p(ips_redirectsent, "\t%u redirect%s sent\n");
	p(ips_localout, "\t%u packet%s sent from this host\n");
	p(ips_rawout, "\t%u packet%s sent with fabricated ip header\n");
	p(ips_odropped,
	  "\t%u output packet%s dropped due to no bufs, etc.\n");
	p(ips_noroute, "\t%u output packet%s discarded due to no route\n");
	p(ips_fragmented, "\t%u output datagram%s fragmented\n");
	p(ips_ofragments, "\t%u fragment%s created\n");
	p(ips_cantfrag, "\t%u datagram%s that can't be fragmented\n");
	p(ips_nogif, "\t%u tunneling packet%s that can't find gif\n");
	p(ips_badaddr, "\t%u datagram%s with bad address in header\n");
	p(ips_pktdropcntrl, "\t%u packet%s dropped due to no bufs for control data\n");

	if (interval > 0)
		bcopy(&ipstat, &pipstat, len);

#undef IPDIFF
#undef p
#undef p1a
}

static	char *icmpnames[] = {
	"echo reply",
	"#1",
	"#2",
	"destination unreachable",
	"source quench",
	"routing redirect",
	"#6",
	"#7",
	"echo",
	"router advertisement",
	"router solicitation",
	"time exceeded",
	"parameter problem",
	"time stamp",
	"time stamp reply",
	"information request",
	"information request reply",
	"address mask request",
	"address mask reply",
};

/*
 * Dump ICMP statistics.
 */
void
icmp_stats(uint32_t off , char *name, int af )
{
	static struct icmpstat picmpstat;
	struct icmpstat icmpstat;
	int i, first;
	int mib[4];		/* CTL_NET + PF_INET + IPPROTO_ICMP + req */
	size_t len;

	mib[0] = CTL_NET;
	mib[1] = PF_INET;
	mib[2] = IPPROTO_ICMP;
	mib[3] = ICMPCTL_STATS;

	len = sizeof icmpstat;
	memset(&icmpstat, 0, len);
	if (sysctl(mib, 4, &icmpstat, &len, (void *)0, 0) < 0)
		return;		/* XXX should complain, but not traditional */

	printf("%s:\n", name);

#define	ICMPDIFF(f) (icmpstat.f - picmpstat.f)
#define	p(f, m) if (ICMPDIFF(f) || sflag <= 1) \
    printf(m, ICMPDIFF(f), plural(ICMPDIFF(f)))
#define	p1a(f, m) if (ICMPDIFF(f) || sflag <= 1) \
    printf(m, ICMPDIFF(f))

	p(icps_error, "\t%u call%s to icmp_error\n");
	p(icps_oldicmp,
	    "\t%u error%s not generated 'cuz old message was icmp\n");
	for (first = 1, i = 0; i < ICMP_MAXTYPE + 1; i++)
		if (ICMPDIFF(icps_outhist[i]) != 0) {
			if (first) {
				printf("\tOutput histogram:\n");
				first = 0;
			}
			printf("\t\t%s: %u\n", icmpnames[i],
				ICMPDIFF(icps_outhist[i]));
		}
	p(icps_badcode, "\t%u message%s with bad code fields\n");
	p(icps_tooshort, "\t%u message%s < minimum length\n");
	p(icps_checksum, "\t%u bad checksum%s\n");
	p(icps_badlen, "\t%u message%s with bad length\n");
	p1a(icps_bmcastecho, "\t%u multicast echo requests ignored\n");
	p1a(icps_bmcasttstamp, "\t%u multicast timestamp requests ignored\n");
	for (first = 1, i = 0; i < ICMP_MAXTYPE + 1; i++)
		if (ICMPDIFF(icps_inhist[i]) != 0) {
			if (first) {
				printf("\tInput histogram:\n");
				first = 0;
			}
			printf("\t\t%s: %u\n", icmpnames[i],
				ICMPDIFF(icps_inhist[i]));
		}
	p(icps_reflect, "\t%u message response%s generated\n");

#undef ICMPDIFF
#undef p
#undef p1a
	mib[3] = ICMPCTL_MASKREPL;
	len = sizeof i;
	if (sysctl(mib, 4, &i, &len, (void *)0, 0) < 0)
		return;
	printf("\tICMP address mask responses are %sabled\n", 
	       i ? "en" : "dis");

	if (interval > 0)
		bcopy(&icmpstat, &picmpstat, sizeof (icmpstat));
}

/*
 * Dump IGMP statistics structure.
 */
void
igmp_stats(uint32_t off , char *name, int af )
{
	static struct igmpstat_v3 pigmpstat;
	struct igmpstat_v3 igmpstat;
	size_t len = sizeof igmpstat;

	if (sysctlbyname("net.inet.igmp.v3stats", &igmpstat, &len, 0, 0) < 0) {
		warn("sysctl: net.inet.igmp.v3stats");
		return;
	}

	if (igmpstat.igps_version != IGPS_VERSION_3) {
		warnx("%s: version mismatch (%d != %d)", __func__,
		    igmpstat.igps_version, IGPS_VERSION_3);
	}
	if (igmpstat.igps_len != IGPS_VERSION3_LEN) {
		warnx("%s: size mismatch (%d != %d)", __func__,
		    igmpstat.igps_len, IGPS_VERSION3_LEN);
	}

	printf("%s:\n", name);

#define	IGMPDIFF(f) ((uintmax_t)(igmpstat.f - pigmpstat.f))
#define	p64(f, m) if (IGMPDIFF(f) || sflag <= 1) \
    printf(m, IGMPDIFF(f), plural(IGMPDIFF(f)))
#define	py64(f, m) if (IGMPDIFF(f) || sflag <= 1) \
    printf(m, IGMPDIFF(f), IGMPDIFF(f) != 1 ? "ies" : "y")

	p64(igps_rcv_total, "\t%ju message%s received\n");
	p64(igps_rcv_tooshort, "\t%ju message%s received with too few bytes\n");
	p64(igps_rcv_badttl, "\t%ju message%s received with wrong TTL\n");
	p64(igps_rcv_badsum, "\t%ju message%s received with bad checksum\n");
	py64(igps_rcv_v1v2_queries, "\t%ju V1/V2 membership quer%s received\n");
	py64(igps_rcv_v3_queries, "\t%ju V3 membership quer%s received\n");
	py64(igps_rcv_badqueries,
	    "\t%ju membership quer%s received with invalid field(s)\n");
	py64(igps_rcv_gen_queries, "\t%ju general quer%s received\n");
	py64(igps_rcv_group_queries, "\t%ju group quer%s received\n");
	py64(igps_rcv_gsr_queries, "\t%ju group-source quer%s received\n");
	py64(igps_drop_gsr_queries, "\t%ju group-source quer%s dropped\n");
	p64(igps_rcv_reports, "\t%ju membership report%s received\n");
	p64(igps_rcv_badreports,
	    "\t%ju membership report%s received with invalid field(s)\n");
	p64(igps_rcv_ourreports,
"\t%ju membership report%s received for groups to which we belong\n");
        p64(igps_rcv_nora, "\t%ju V3 report%s received without Router Alert\n");
        p64(igps_snd_reports, "\t%ju membership report%s sent\n");

	if (interval > 0)
		bcopy(&igmpstat, &pigmpstat, len);

#undef IGMPDIFF
#undef p64
#undef py64
}

/*
 * Pretty print an Internet address (net address + port).
 */
void
inetprint(struct in_addr *in, int port, char *proto, int numeric_port)
{
	struct servent *sp = 0;
	char line[80], *cp;
	int width;

	if (Wflag)
	    snprintf(line, sizeof(line), "%s.", inetname(in));
	else
	    snprintf(line, sizeof(line), "%.*s.", (Aflag && !numeric_port) ? 12 : 16, inetname(in));
	cp = index(line, '\0');
	if (!numeric_port && port)
#ifdef _SERVICE_CACHE_
		sp = _serv_cache_getservbyport(port, proto);
#else
		sp = getservbyport((int)port, proto);
#endif
	if (sp || port == 0)
		snprintf(cp, sizeof(line) - (cp - line), "%.15s ", sp ? sp->s_name : "*");
	else
		snprintf(cp, sizeof(line) - (cp - line), "%d ", ntohs((u_short)port));
	width = (Aflag && !Wflag) ? 18 : 22;
	if (Wflag)
	    printf("%-*s ", width, line);
	else
	    printf("%-*.*s ", width, width, line);
}

/*
 * Construct an Internet address representation.
 * If the nflag has been supplied, give
 * numeric value, otherwise try for symbolic name.
 */
char *
inetname(struct in_addr *inp)
{
	register char *cp;
	static char line[MAXHOSTNAMELEN];
	struct hostent *hp;
	struct netent *np;

	cp = 0;
	if (!nflag && inp->s_addr != INADDR_ANY) {
		int net = inet_netof(*inp);
		int lna = inet_lnaof(*inp);

		if (lna == INADDR_ANY) {
			np = getnetbyaddr(net, AF_INET);
			if (np)
				cp = np->n_name;
		}
		if (cp == 0) {
			hp = gethostbyaddr((char *)inp, sizeof (*inp), AF_INET);
			if (hp) {
				cp = hp->h_name;
				 //### trimdomain(cp, strlen(cp));
			}
		}
	}
	if (inp->s_addr == INADDR_ANY)
		strlcpy(line, "*", sizeof(line));
	else if (cp) {
		strncpy(line, cp, sizeof(line) - 1);
		line[sizeof(line) - 1] = '\0';
	} else {
		inp->s_addr = ntohl(inp->s_addr);
#define C(x)	((u_int)((x) & 0xff))
		snprintf(line, sizeof(line), "%u.%u.%u.%u", C(inp->s_addr >> 24),
		    C(inp->s_addr >> 16), C(inp->s_addr >> 8), C(inp->s_addr));
	}
	return (line);
}
