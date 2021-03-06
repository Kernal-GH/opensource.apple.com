/*
 * Compatibility shims with RELENG_4
 *
 * Portions Copyright (C) 2001 - 2007 Apple Inc. All rights reserved.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kpi_mbuf.h>
#include <sys/lock.h>

#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/unistd.h>

#include <stdarg.h>

#include <sys/smb_apple.h>
#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>

#include <netsmb/smb_compat4.h>

/*
 * The only way to get min cluster size is to make a 
 * mbuf_stat call. We really only need to do this once
 * since minclsize is a compile time option.
 */
static int minclsize = 0;

static u_long mbuf_minclsize() 
{
	struct mbuf_stat stats;
	
	if (! minclsize) {
		mbuf_stats(&stats);
		minclsize = stats.minclsize;
	}
	return minclsize;
}
#define MINCLSIZE mbuf_minclsize()
		
/*
 * This will allocate len-worth of mbufs and/or mbuf clusters (whatever fits
 * best) and return a pointer to the top of the allocated chain. If m is
 * non-null, then we assume that it is a single mbuf or an mbuf chain to
 * which we want len bytes worth of mbufs and/or clusters attached, and so
 * if we succeed in allocating it, we will just return a pointer to m.
 *
 * If we happen to fail at any point during the allocation, we will free
 * up everything we have already allocated and return NULL.
 *
 */
PRIVSYM mbuf_t
smb_mbuf_getm(mbuf_t m, int len, int how, int type)
{
	mbuf_t top, tail, mp, mtail = NULL;

	KASSERT(len >= 0, ("len is < 0 in smb_mbuf_getm"));

	if (mbuf_get(how, type, &mp))
		return (NULL);
	else if (len > (int)MINCLSIZE) {
		if ((mbuf_mclget(how, type, &mp)) ||
			((mbuf_flags(mp) & MBUF_EXT) == 0)) {
			mbuf_free(mp);			
			return (NULL);
		}
	}
	mbuf_setlen(mp, 0);
	len -= mbuf_trailingspace(mp);

	if (m != NULL)
		for (mtail = m; mbuf_next(mtail) != NULL; mtail = mbuf_next(mtail));
	else
		m = mp;

	top = tail = mp;
	while (len > 0) {
		if (mbuf_get(how, type, &mp))
			goto failed;

		mbuf_setnext(tail, mp);
		tail = mp;
		if (len > (int)MINCLSIZE) {
			if ((mbuf_mclget(how, type, &mp)) ||
				((mbuf_flags(mp) & MBUF_EXT) == 0))
				goto failed;
		}
		mbuf_setlen(mp, 0);
		len -= mbuf_trailingspace(mp);
	}

	if (mtail != NULL)
		mbuf_setnext(mtail, top);
	return (m);

failed:
	mbuf_freem(top);
	return (NULL);
}
