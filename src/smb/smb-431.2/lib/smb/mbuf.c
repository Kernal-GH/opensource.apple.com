/*
 * Copyright (c) 2000, Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2004 - 2008 Apple Inc. All rights reserved.
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
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: mbuf.c,v 1.4.218.1 2006/04/14 23:49:37 gcolley Exp $
 */

#include <sys/types.h>
#include <sys/mchain.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <asl.h>

#include <netsmb/smb_lib.h>

#define MBERROR(format, args...) asl_log(NULL, NULL, ASL_LEVEL_ERR, "%s(%d): "format, __FUNCTION__ , __LINE__ ,## args)

/* Acts like the kernel call, but only in user space. */
static int
mbuf_get(size_t len, struct smb_lib_mbuf **mpp)
{
	struct smb_lib_mbuf *m;

	len = SMB_LIB_M_ALIGN(len);
	if (len < SMB_LIB_M_MINSIZE)
		len = SMB_LIB_M_MINSIZE;
	m = malloc(SMB_LIB_M_BASESIZE + len);
	if (m == NULL)
		return ENOMEM;
	bzero(m, SMB_LIB_M_BASESIZE + len);
	m->m_maxlen = len;
	m->m_data = SMB_LIB_M_TOP(m);
	*mpp = m;
	return 0;
}

static void
free_all(struct smb_lib_mbuf *m0)
{
	struct smb_lib_mbuf *m;

	while (m0) {
		m = m0->m_next;
		free(m0);
		m0 = m;
	}
}

static size_t
m_totlen(struct smb_lib_mbuf *m0)
{
	struct smb_lib_mbuf *m = m0;
	size_t len = 0;

	while (m) {
		len += m->m_len;
		m = m->m_next;
	}
	return len;
}

int
smb_lib_m_lineup(struct smb_lib_mbuf *m0, struct smb_lib_mbuf **mpp)
{
	struct smb_lib_mbuf *nm, *m;
	char *dp;
	size_t len;
	int error;

	if (m0->m_next == NULL) {
		*mpp = m0;
		return 0;
	}
	if ((error = mbuf_get(m_totlen(m0), &nm)) != 0)
		return error;
	dp = SMB_LIB_MTODATA(nm, char *);
	while (m0) {
		len = m0->m_len;
		bcopy(m0->m_data, dp, len);
		dp += len;
		m = m0->m_next;
		free(m0);
		m0 = m;
	}
	*mpp = nm;
	return 0;
}

int
mb_init(struct mbdata *mbp, size_t size)
{
	struct smb_lib_mbuf *m;
	int error;

	if ((error = mbuf_get(size, &m)) != 0)
		return error;
	return mb_initm(mbp, m);
}

int
mb_initm(struct mbdata *mbp, struct smb_lib_mbuf *m)
{
	bzero(mbp, sizeof(*mbp));
	mbp->mb_top = mbp->mb_cur = m;
	mbp->mb_pos = SMB_LIB_MTODATA(m, char *);
	return 0;
}

int
mb_done(struct mbdata *mbp)
{
	if (mbp->mb_top) {
		free_all(mbp->mb_top);
		mbp->mb_top = NULL;
	}
	return 0;
}

int
smb_lib_mbuf_getm(struct smb_lib_mbuf *top, size_t len, struct smb_lib_mbuf **mpp)
{
	struct smb_lib_mbuf *m, *mp;
	int error;
	
	for (mp = top; ; mp = mp->m_next) {
		len -= SMB_LIB_M_TRAILINGSPACE(mp);
		if (mp->m_next == NULL)
			break;
		
	}
	if (len > 0) {
		if ((error = mbuf_get(len, &m)) != 0)
			return error;
		mp->m_next = m;
	}
	*mpp = top;
	return 0;
}

/*
 * Routines to put data in a buffer
 */
#define	MB_PUT(t)	int error; t *p; \
			if ((error = mb_fit(mbp, sizeof(t), (char**)&p)) != 0) \
				return error

/*
 * Check if object of size 'size' fit to the current position and
 * allocate new mbuf if not. Advance pointers and increase length of mbuf(s).
 * Return pointer to the object placeholder or NULL if any error occured.
 */
int
mb_fit(struct mbdata *mbp, size_t size, char **pp)
{
	struct smb_lib_mbuf *m, *mn;
	int error;

	m = mbp->mb_cur;
	if ((size_t)SMB_LIB_M_TRAILINGSPACE(m) < size) {
		if ((error = mbuf_get(size, &mn)) != 0)
			return error;
		mbp->mb_pos = SMB_LIB_MTODATA(mn, char *);
		mbp->mb_cur = m->m_next = mn;
		m = mn;
	}
	m->m_len += size;
	*pp = mbp->mb_pos;
	mbp->mb_pos += size;
	mbp->mb_count += size;
	return 0;
}

int
mb_put_uint8(struct mbdata *mbp, u_int8_t x)
{
	MB_PUT(u_int8_t);
	*p = x;
	return 0;
}

int
mb_put_uint16be(struct mbdata *mbp, u_int16_t x)
{
	MB_PUT(u_int16_t);
	setwbe(p, 0, x);
	return 0;
}

int
mb_put_uint16le(struct mbdata *mbp, u_int16_t x)
{
	MB_PUT(u_int16_t);
	setwle(p, 0, x);
	return 0;
}

int
mb_put_uint32be(struct mbdata *mbp, u_int32_t x)
{
	MB_PUT(u_int32_t);
	setdbe(p, 0, x);
	return 0;
}

int
mb_put_uint32le(struct mbdata *mbp, u_int32_t x)
{
	MB_PUT(u_int32_t);
	setdle(p, 0, x);
	return 0;
}

#ifdef SMB_CURRENTLY_NOTE_USED
int mb_put_uint64be(struct mbdata *mbp, u_int64_t x)
{
	MB_PUT(u_int64_t);
	*p = htobeq(x);
	return 0;
}
#endif // SMB_CURRENTLY_NOTE_USED

int
mb_put_uint64le(struct mbdata *mbp, u_int64_t x)
{
	MB_PUT(u_int64_t);
	*p = htoleq(x);
	return 0;
}

int
mb_put_mem(struct mbdata *mbp, const char *source, size_t size)
{
	struct smb_lib_mbuf *m;
	char * dst;
	size_t cplen;
	int error;

	if (size == 0)
		return 0;
	m = mbp->mb_cur;
	if ((error = smb_lib_mbuf_getm(m, size, &m)) != 0)
		return error;
	while (size > 0) {
		cplen = SMB_LIB_M_TRAILINGSPACE(m);
		if (cplen == 0) {
			m = m->m_next;
			continue;
		}
		if (cplen > size)
			cplen = size;
		dst = SMB_LIB_MTODATA(m, char *) + m->m_len;
		if (source) {
			bcopy(source, dst, cplen);
			source += cplen;
		} else
			bzero(dst, cplen);
		size -= cplen;
		m->m_len += cplen;
		mbp->mb_count += cplen;
	}
	mbp->mb_pos = SMB_LIB_MTODATA(m, char *) + m->m_len;
	mbp->mb_cur = m;
	return 0;
}

int
mb_put_mbuf(struct mbdata *mbp, struct smb_lib_mbuf *m)
{
	mbp->mb_cur->m_next = m;
	while (m) {
		mbp->mb_count += m->m_len;
		if (m->m_next == NULL)
			break;
		m = m->m_next;
	}
	mbp->mb_pos = SMB_LIB_MTODATA(m, char *) + m->m_len;
	mbp->mb_cur = m;
	return 0;
}

/*
 * Routines for fetching data from an mbuf chain
 */
#define mb_left(m,p)	(SMB_LIB_MTODATA(m, char *) + (m)->m_len - (p))

int
mb_get_uint8(struct mbdata *mbp, u_int8_t *x)
{
	return mb_get_mem(mbp, (char *)x, 1);
}

int
mb_get_uint16(struct mbdata *mbp, u_int16_t *x)
{
	return mb_get_mem(mbp, (char *)x, 2);
}

int
mb_get_uint16le(struct mbdata *mbp, u_int16_t *x)
{
	u_int16_t v;
	int error = mb_get_uint16(mbp, &v);

	if (x != NULL)
		*x = letohs(v);
	return error;
}

int
mb_get_uint16be(struct mbdata *mbp, u_int16_t *x) {
	u_int16_t v;
	int error = mb_get_uint16(mbp, &v);

	if (x != NULL)
		*x = betohs(v);
	return error;
}

static int mb_get_uint32(struct mbdata *mbp, u_int32_t *x)
{
	return mb_get_mem(mbp, (char *)x, 4);
}

int
mb_get_uint32be(struct mbdata *mbp, u_int32_t *x)
{
	u_int32_t v;
	int error;

	error = mb_get_uint32(mbp, &v);
	if (x != NULL)
		*x = betohl(v);
	return error;
}

int
mb_get_uint32le(struct mbdata *mbp, u_int32_t *x)
{
	u_int32_t v;
	int error;

	error = mb_get_uint32(mbp, &v);
	if (x != NULL)
		*x = letohl(v);
	return error;
}

static int
mb_get_uint64(struct mbdata *mbp, u_int64_t *x)
{
	return mb_get_mem(mbp, (char *)x, 8);
}

#ifdef SMB_CURRENTLY_NOTE_USED
int mb_get_uint64be(struct mbdata *mbp, u_int64_t *x)
{
	u_int64_t v;
	int error;
	
	error = mb_get_uint64(mbp, &v);
	if (x != NULL)
		*x = betohq(v);
	return error;
}
#endif // SMB_CURRENTLY_NOTE_USED

int
mb_get_uint64le(struct mbdata *mbp, u_int64_t *x)
{
	u_int64_t v;
	int error;

	error = mb_get_uint64(mbp, &v);
	if (x != NULL)
		*x = letohq(v);
	return error;
}

int
mb_get_mem(struct mbdata *mbp, char * target, u_int32_t size)
{
	struct smb_lib_mbuf *m = mbp->mb_cur;
	u_int32_t count;
	
	while (size > 0) {
		if (m == NULL) {
			MBERROR("incomplete copy\n");
			return EBADRPC;
		}
		count = (int32_t)mb_left(m, mbp->mb_pos);
		if (count == 0) {
			mbp->mb_cur = m = m->m_next;
			if (m)
				mbp->mb_pos = SMB_LIB_MTODATA(m, char *);
			continue;
		}
		if (count > size)
			count = size;
		size -= count;
		if (target) {
			if (count == 1) {
				*target++ = *mbp->mb_pos;
			} else {
				bcopy(mbp->mb_pos, target, count);
				target += count;
			}
		}
		mbp->mb_pos += count;
	}
	return 0;
}
