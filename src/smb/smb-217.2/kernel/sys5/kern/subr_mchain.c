/*
 * Copyright (c) 2000, 2001 Boris Popov
 * All rights reserved.
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
 * $FreeBSD: src/sys/kern/subr_mchain.c,v 1.1 2001/02/24 15:44:29 bp Exp $
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/mbuf.h>
#include <sys/uio.h>

#include <sys/smb_apple.h>
#include <sys/mchain.h>

#include <netsmb/smb_compat4.h>

MODULE_VERSION(libmchain, 1);

#define MBERROR(format, args...) printf("%s(%d): "format, __FUNCTION__ , \
				    __LINE__ ,## args)

#define MBPANIC(format, args...) printf("%s(%d): "format, __FUNCTION__ , \
				    __LINE__ ,## args)

/*
 * Various helper functions
 */
PRIVSYM int
m_fixhdr(struct mbuf *m0)
{
	struct mbuf *m = m0;
	int len = 0;

	while (m) {
		len += m->m_len;
		m = m->m_next;
	}
	m0->m_pkthdr.len = len;
	return len;
}

PRIVSYM int
mb_init(struct mbchain *mbp)
{
	struct mbuf *m;

	m = m_gethdr(M_TRYWAIT, MT_DATA);
	if (m == NULL) 
		return ENOBUFS;
	m->m_pkthdr.rcvif = NULL;
	m->m_len = 0;
	mb_initm(mbp, m);
	return 0;
}

PRIVSYM void
mb_initm(struct mbchain *mbp, struct mbuf *m)
{
	bzero(mbp, sizeof(*mbp));
	mbp->mb_top = mbp->mb_cur = m;
	mbp->mb_mleft = M_TRAILINGSPACE(m);
}

PRIVSYM void
mb_done(struct mbchain *mbp)
{
	if (mbp->mb_top) {
		m_freem(mbp->mb_top);
		mbp->mb_top = NULL;
	}
}

PRIVSYM struct mbuf *
mb_detach(struct mbchain *mbp)
{
	struct mbuf *m;

	m = mbp->mb_top;
	mbp->mb_top = NULL;
	return m;
}

PRIVSYM int
mb_fixhdr(struct mbchain *mbp)
{
	return mbp->mb_top->m_pkthdr.len = m_fixhdr(mbp->mb_top);
}

/*
 * Check if object of size 'size' fit to the current position and
 * allocate new mbuf if not. Advance pointers and increase length of mbuf(s).
 * Return pointer to the object placeholder or NULL if any error occured.
 * Note: size should be <= MLEN 
 */
PRIVSYM caddr_t
mb_reserve(struct mbchain *mbp, int size)
{
	struct mbuf *m, *mn;
	caddr_t bpos;

	if (size > (int)MLEN)
		panic("mb_reserve: size = %d\n", size);
	m = mbp->mb_cur;
	if (mbp->mb_mleft < size) {
		mn = m_get(M_TRYWAIT, MT_DATA);
		if (mn == NULL)
			return NULL;
		mbp->mb_cur = m->m_next = mn;
		m = mn;
		m->m_len = 0;
		mbp->mb_mleft = M_TRAILINGSPACE(m);
	}
	mbp->mb_mleft -= size;
	mbp->mb_count += size;
	bpos = mtod(m, caddr_t) + m->m_len;
	m->m_len += size;
	return bpos;
}

PRIVSYM int
mb_put_padbyte(struct mbchain *mbp)
{
	caddr_t dst;
	char x = 0;

	dst = mtod(mbp->mb_cur, caddr_t) + mbp->mb_cur->m_len;

	/* only add padding if address is odd */
	if ((long)dst & 1)
		return mb_put_mem(mbp, (caddr_t)&x, 1, MB_MSYSTEM);
	else
		return 0;
}

PRIVSYM int
mb_put_uint8(struct mbchain *mbp, u_int8_t x)
{
	return mb_put_mem(mbp, (caddr_t)&x, sizeof(x), MB_MSYSTEM);
}

PRIVSYM int
mb_put_uint16be(struct mbchain *mbp, u_int16_t x)
{
	x = htobes(x);
	return mb_put_mem(mbp, (caddr_t)&x, sizeof(x), MB_MSYSTEM);
}

PRIVSYM int
mb_put_uint16le(struct mbchain *mbp, u_int16_t x)
{
	x = htoles(x);
	return mb_put_mem(mbp, (caddr_t)&x, sizeof(x), MB_MSYSTEM);
}

PRIVSYM int
mb_put_uint32be(struct mbchain *mbp, u_int32_t x)
{
	x = htobel(x);
	return mb_put_mem(mbp, (caddr_t)&x, sizeof(x), MB_MSYSTEM);
}

PRIVSYM int
mb_put_uint32le(struct mbchain *mbp, u_int32_t x)
{
	x = htolel(x);
	return mb_put_mem(mbp, (caddr_t)&x, sizeof(x), MB_MSYSTEM);
}

PRIVSYM int
mb_put_uint64be(struct mbchain *mbp, u_int64_t x)
{
	x = htobeq(x);
	return mb_put_mem(mbp, (caddr_t)&x, sizeof(x), MB_MSYSTEM);
}

PRIVSYM int
mb_put_uint64le(struct mbchain *mbp, u_int64_t x)
{
	x = htoleq(x);
	return mb_put_mem(mbp, (caddr_t)&x, sizeof(x), MB_MSYSTEM);
}

PRIVSYM int
mb_put_mem(struct mbchain *mbp, c_caddr_t source, int size, int type)
{
	struct mbuf *m;
	caddr_t dst;
	c_caddr_t src;
	int cplen, error, mleft, count;

	m = mbp->mb_cur;
	mleft = mbp->mb_mleft;

	while (size > 0) {
		if (mleft == 0) {
			if (m->m_next == NULL) {
				m = m_getm(m, size, M_TRYWAIT, MT_DATA);
				if (m == NULL)
					return ENOBUFS;
			}
			m = m->m_next;
			mleft = M_TRAILINGSPACE(m);
			continue;
		}
		cplen = mleft > size ? size : mleft;
		dst = mtod(m, caddr_t) + m->m_len;
		switch (type) {
		    case MB_MCUSTOM:
			error = mbp->mb_copy(mbp, source, dst, cplen);
			if (error)
				return error;
			break;
		    case MB_MINLINE:
			for (src = source, count = cplen; count; count--)
				*dst++ = *src++;
			break;
		    case MB_MSYSTEM:
			bcopy(source, dst, cplen);
			break;
		    case MB_MUSER:
			error = copyin(CAST_USER_ADDR_T(source), dst, cplen);
			if (error)
				return error;
			break;
		    case MB_MZERO:
			bzero(dst, cplen);
			break;
		}
		size -= cplen;
		source += cplen;
		m->m_len += cplen;
		mleft -= cplen;
		mbp->mb_count += cplen;
	}
	mbp->mb_cur = m;
	mbp->mb_mleft = mleft;
	return 0;
}

PRIVSYM int
mb_put_mbuf(struct mbchain *mbp, struct mbuf *m)
{
	mbp->mb_cur->m_next = m;
	while (m) {
		mbp->mb_count += m->m_len;
		if (m->m_next == NULL)
			break;
		m = m->m_next;
	}
	mbp->mb_mleft = M_TRAILINGSPACE(m);
	mbp->mb_cur = m;
	return 0;
}

/*
 * copies a uio scatter/gather list to an mbuf chain.
 */
PRIVSYM int
mb_put_uio(struct mbchain *mbp, uio_t uiop, int size)
{
	user_size_t left;
	int mtype, error;

	mtype = (uio_isuserspace(uiop) ? MB_MUSER : MB_MSYSTEM);

	while (size > 0 && uio_resid(uiop)) {
		if (uio_iovcnt(uiop) <= 0 || uio_curriovbase(uiop) == USER_ADDR_NULL)
			return EFBIG;
		left = uio_curriovlen(uiop);
		if (left > size)
			left = size;
		// LP64todo - address could be 64-bit value
#ifdef __ppc__
		if (uio_curriovbase(uiop) > 0xFFFFFFFFULL)
			panic("%s - need LP64 support\n", __FUNCTION__);
#endif /* __ppc__ */
		error = mb_put_mem(mbp, CAST_DOWN(caddr_t, uio_curriovbase(uiop)), left, mtype);
		if (error)
			return error;
		uio_update(uiop, left);
		size -= left;
	}
	return 0;
}

/*
 * Routines for fetching data from an mbuf chain
 */
PRIVSYM int
md_init(struct mdchain *mdp)
{
	struct mbuf *m;

	m = m_gethdr(M_TRYWAIT, MT_DATA);
	if (m == NULL) 
		return ENOBUFS;
	m->m_pkthdr.rcvif = NULL;
	m->m_len = 0;
	md_initm(mdp, m);
	return 0;
}

PRIVSYM void
md_initm(struct mdchain *mdp, struct mbuf *m)
{
	bzero(mdp, sizeof(*mdp));
	mdp->md_top = mdp->md_cur = m;
	mdp->md_pos = mtod(m, u_char*);
}

PRIVSYM void
md_done(struct mdchain *mdp)
{
	if (mdp->md_top) {
		m_freem(mdp->md_top);
		mdp->md_top = NULL;
	}
}

/*
 * Append a separate mbuf chain. It is caller responsibility to prevent
 * multiple calls to fetch/record routines.
 */
PRIVSYM void
md_append_record(struct mdchain *mdp, struct mbuf *top)
{
	struct mbuf *m;

	if (mdp->md_top == NULL) {
		md_initm(mdp, top);
		return;
	}
	m = mdp->md_top;
	while (m->m_nextpkt)
		m = m->m_nextpkt;
	m->m_nextpkt = top;
	top->m_nextpkt = NULL;
	return;
}

/*
 * Put next record in place of existing
 */
PRIVSYM int
md_next_record(struct mdchain *mdp)
{
	struct mbuf *m;

	if (mdp->md_top == NULL)
		return ENOENT;
	m = mdp->md_top->m_nextpkt;
	md_done(mdp);
	if (m == NULL)
		return ENOENT;
	md_initm(mdp, m);
	return 0;
}

PRIVSYM int
md_get_uint8(struct mdchain *mdp, u_int8_t *x)
{
	return md_get_mem(mdp, x, 1, MB_MINLINE);
}

PRIVSYM int
md_get_uint16(struct mdchain *mdp, u_int16_t *x)
{
	return md_get_mem(mdp, (caddr_t)x, 2, MB_MINLINE);
}

PRIVSYM int
md_get_uint16le(struct mdchain *mdp, u_int16_t *x)
{
	u_int16_t v;
	int error = md_get_uint16(mdp, &v);

	if (x)
		*x = letohs(v);
	return error;
}

PRIVSYM int
md_get_uint16be(struct mdchain *mdp, u_int16_t *x) {
	u_int16_t v;
	int error = md_get_uint16(mdp, &v);

	if (x)
		*x = betohs(v);
	return error;
}

PRIVSYM int
md_get_uint32(struct mdchain *mdp, u_int32_t *x)
{
	return md_get_mem(mdp, (caddr_t)x, 4, MB_MINLINE);
}

PRIVSYM int
md_get_uint32be(struct mdchain *mdp, u_int32_t *x)
{
	u_int32_t v;
	int error;

	error = md_get_uint32(mdp, &v);
	if (x)
		*x = betohl(v);
	return error;
}

PRIVSYM int
md_get_uint32le(struct mdchain *mdp, u_int32_t *x)
{
	u_int32_t v;
	int error;

	error = md_get_uint32(mdp, &v);
	if (x)
		*x = letohl(v);
	return error;
}

PRIVSYM int
md_get_uint64(struct mdchain *mdp, u_int64_t *x)
{
	return md_get_mem(mdp, (caddr_t)x, 8, MB_MINLINE);
}

PRIVSYM int
md_get_uint64be(struct mdchain *mdp, u_int64_t *x)
{
	u_int64_t v;
	int error;

	error = md_get_uint64(mdp, &v);
	if (x)
		*x = betohq(v);
	return error;
}

PRIVSYM int
md_get_uint64le(struct mdchain *mdp, u_int64_t *x)
{
	u_int64_t v;
	int error;

	error = md_get_uint64(mdp, &v);
	if (x)
		*x = letohq(v);
	return error;
}

PRIVSYM int
md_get_mem(struct mdchain *mdp, caddr_t target, int size, int type)
{
	struct mbuf *m = mdp->md_cur;
	int error;
	u_int count;
	u_char *s;
	
	while (size > 0) {
		if (m == NULL) {
			MBERROR("incomplete copy\n");
			return EBADRPC;
		}
		s = mdp->md_pos;
		count = mtod(m, u_char*) + m->m_len - s;
		if (count == 0) {
			mdp->md_cur = m = m->m_next;
			if (m)
				s = mdp->md_pos = mtod(m, caddr_t);
			continue;
		}
		if ((int)count > size)
			count = size;
		size -= count;
		mdp->md_pos += count;
		if (target == NULL)
			continue;
		switch (type) {
		    case MB_MUSER:
			error = copyout(s, CAST_USER_ADDR_T(target), count);
			if (error)
				return error;
			break;
		    case MB_MSYSTEM:
			bcopy(s, target, count);
			break;
		    case MB_MINLINE:
			while (count--)
				*target++ = *s++;
			continue;
		}
		target += count;
	}
	return 0;
}

PRIVSYM int
md_get_mbuf(struct mdchain *mdp, int size, struct mbuf **ret)
{
	struct mbuf *m = mdp->md_cur, *rm;

	rm = m_copym(m, mdp->md_pos - mtod(m, u_char*), size, M_TRYWAIT);
	if (rm == NULL)
		return EBADRPC;
	md_get_mem(mdp, NULL, size, MB_MZERO);
	*ret = rm;
	return 0;
}

PRIVSYM int
md_get_uio(struct mdchain *mdp, uio_t uiop, int size)
{
	user_size_t left;
	int mtype, error;

	mtype = (uio_isuserspace(uiop) ? MB_MUSER : MB_MSYSTEM);
	while (size > 0 && uio_resid(uiop)) {
		if (uio_iovcnt(uiop) <= 0 || uio_curriovbase(uiop) == USER_ADDR_NULL)
			return EFBIG;
		left = uio_curriovlen(uiop);
		if (left > size)
			left = size;
		// LP64todo - address could be 64-bit value
#ifdef __ppc__
		if (uio_curriovbase(uiop) > 0xFFFFFFFFULL)
			panic("%s - need LP64 support\n", __FUNCTION__);
#endif /* __ppc__ */
		error = md_get_mem(mdp, CAST_DOWN(caddr_t, uio_curriovbase(uiop)), left, mtype);
		if (error)
			return error;
		uio_update(uiop, left);
		size -= left;
	}
	return 0;
}
