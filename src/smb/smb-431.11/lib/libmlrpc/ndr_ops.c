/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
/*
 * Portions Copyright 2008 Apple, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * MLRPC server-side NDR stream (PDU) operations. Stream operations
 * should return TRUE (non-zero) on success or FALSE (zero or a null
 * pointer) on failure. When an operation returns FALSE, including
 * mlndo_malloc() returning NULL, it should set the mlnds->error to
 * indicate what went wrong.
 *
 * When available, the relevant ndr_reference is passed to the
 * operation but keep in mind that it may be a null pointer.
 *
 * Functions mlndo_get_pdu(), mlndo_put_pdu(), and mlndo_pad_pdu()
 * must never grow the PDU data. A request for out-of-bounds data is
 * an error. The swap_bytes flag is 1 if NDR knows that the byte-
 * order in the PDU is different from the local system.
 */

#include <sys/types.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <assert.h>
#include <syslog.h>

#include <libmlrpc/mlrpc.h>
#include <libmlrpc/ndr.h>

#define	NDOBUFSZ		128

#define	NDR_PDU_BLOCK_SIZE	(4*1024)
#define	NDR_PDU_BLOCK_MASK	(NDR_PDU_BLOCK_SIZE - 1)
#define	NDR_PDU_ALIGN(N) \
	(((N) + NDR_PDU_BLOCK_SIZE) & ~NDR_PDU_BLOCK_MASK)
#define	NDR_PDU_MAX_SIZE		(64*1024*1024)

static char *mlndo_malloc(struct mlndr_stream *, size_t,
    struct ndr_reference *);
static int mlndo_free(struct mlndr_stream *, char *, struct ndr_reference *);
static int mlndo_grow_pdu(struct mlndr_stream *, unsigned long,
    struct ndr_reference *);
static int mlndo_pad_pdu(struct mlndr_stream *, unsigned long, unsigned long,
    struct ndr_reference *);
static int mlndo_get_pdu(struct mlndr_stream *, unsigned long, unsigned long,
    char *, int, struct ndr_reference *);
static int mlndo_put_pdu(struct mlndr_stream *, unsigned long, unsigned long,
    char *, int, struct ndr_reference *);
static void mlndo_tattle(struct mlndr_stream *, char *, struct ndr_reference *);
static void mlndo_tattle_error(struct mlndr_stream *, struct ndr_reference *);
static int mlndo_reset(struct mlndr_stream *);
static void mlndo_destruct(struct mlndr_stream *);

/*
 * The mlndr stream operations table.
 */
static struct mlndr_stream_ops mlnds_ops = {
    mlndo_malloc,
    mlndo_free,
    mlndo_grow_pdu,
    mlndo_pad_pdu,
    mlndo_get_pdu,
    mlndo_put_pdu,
    mlndo_tattle,
    mlndo_tattle_error,
    mlndo_reset,
    mlndo_destruct
};

/*
 * mlnds_bswap
 *
 * Copies len bytes from src to dst such that dst contains the bytes
 * from src in reverse order.
 *
 * We expect to be dealing with bytes, words, dwords etc. So the
 * length must be non-zero and a power of 2.
 */
void
mlnds_bswap(const void *srcbuf, void *dstbuf, size_t len)
{
	uint8_t *src = (uint8_t *)srcbuf;
	uint8_t *dst = (uint8_t *)dstbuf;

	if ((len != 0) && ((len & (len - 1)) == 0)) {
		src += len;

		while (len--)
			*dst++ = *(--src);
	}
}

/*
 * mlnds_initialize
 *
 * Initialize a stream. Sets up the PDU parameters and assigns the stream
 * operations and the reference to the heap. An external heap is provided
 * to the stream, rather than each stream creating its own heap.
 */
int
mlnds_initialize(struct mlndr_stream *mlnds, unsigned pdu_size_hint,
    int composite_op, mlrpc_heap_t *heap)
{
	unsigned size;

	assert(mlnds);
	assert(heap);

	bzero(mlnds, sizeof (*mlnds));

	if (pdu_size_hint > NDR_PDU_MAX_SIZE)
		return (0);

	size = (pdu_size_hint == 0) ? NDR_PDU_BLOCK_SIZE : pdu_size_hint;
	mlnds->pdu_base_addr = malloc(size);
	assert(mlnds->pdu_base_addr);

	mlnds->pdu_max_size = size;
	mlnds->pdu_size = 0;

	mlnds->mlndo = &mlnds_ops;
	mlnds->heap = heap;

	mlnds->m_op = composite_op & 0x0F;
	mlnds->dir  = composite_op & 0xF0;

	mlnds->outer_queue_tailp = &mlnds->outer_queue_head;
	return (1);
}

/*
 * mlnds_destruct
 *
 * Destroy a stream. This is an external interface to provide access to
 * the stream's destruct operation.
 */
void
mlnds_destruct(struct mlndr_stream *mlnds)
{
	MLNDS_DESTRUCT(mlnds);
}

/*
 * mlndo_malloc
 *
 * Allocate memory from the stream heap.
 */
/*ARGSUSED*/
static char *
mlndo_malloc(struct mlndr_stream *mlnds, size_t len,
    struct ndr_reference *ref)
{
	return (mlrpc_heap_malloc((mlrpc_heap_t *)mlnds->heap, len));
}

/*
 * mlndo_free
 *
 * Always succeeds: cannot free individual stream allocations.
 */
/*ARGSUSED*/
static int
mlndo_free(struct mlndr_stream *mlnds, char *p, struct ndr_reference *ref)
{
	return (1);
}

/*
 * mlndo_grow_pdu
 *
 * This is the only place that should change the size of the PDU. If the
 * desired offset is beyond the current PDU size, we realloc the PDU
 * buffer to accommodate the request. For efficiency, the PDU is always
 * extended to a NDR_PDU_BLOCK_SIZE boundary. Requests to grow the PDU
 * beyond NDR_PDU_MAX_SIZE are rejected.
 *
 * Returns 1 to indicate success. Otherwise 0 to indicate failure.
 */
static int
mlndo_grow_pdu(struct mlndr_stream *mlnds, unsigned long want_end_offset,
    struct ndr_reference *ref)
{
	unsigned char *pdu_addr;
	unsigned pdu_max_size;

	pdu_max_size = mlnds->pdu_max_size;

	if (want_end_offset > pdu_max_size) {
		pdu_max_size = NDR_PDU_ALIGN(want_end_offset);

		if (pdu_max_size >= NDR_PDU_MAX_SIZE)
			return (0);

		pdu_addr = realloc(mlnds->pdu_base_addr, pdu_max_size);
		if (pdu_addr == 0)
			return (0);

		mlnds->pdu_max_size = pdu_max_size;
		mlnds->pdu_base_addr = pdu_addr;
	}

	return (1);
}

static int
mlndo_pad_pdu(struct mlndr_stream *mlnds, unsigned long pdu_offset,
    unsigned long n_bytes, struct ndr_reference *ref)
{
	unsigned char *data;

	data = mlnds->pdu_base_addr;
	data += pdu_offset;

	bzero(data, n_bytes);
	return (1);
}

/*
 * mlndo_get_pdu
 *
 * The swap flag is 1 if NDR knows that the byte-order in the PDU
 * is different from the local system.
 *
 * Returns 1 on success or 0 to indicate failure.
 */
static int
mlndo_get_pdu(struct mlndr_stream *mlnds, unsigned long pdu_offset,
    unsigned long n_bytes, char *buf, int swap_bytes,
    struct ndr_reference *ref)
{
	unsigned char *data;

	data = mlnds->pdu_base_addr;
	data += pdu_offset;

	if (!swap_bytes)
		bcopy(data, buf, n_bytes);
	else
		mlnds_bswap(data, (unsigned char *)buf, n_bytes);

	return (1);
}

/*
 * mlndo_put_pdu
 *
 * This is a receiver makes right protocol. So we do not need
 * to be concerned about the byte-order of an outgoing PDU.
 */
/*ARGSUSED*/
static int
mlndo_put_pdu(struct mlndr_stream *mlnds, unsigned long pdu_offset,
    unsigned long n_bytes, char *buf, int swap_bytes,
    struct ndr_reference *ref)
{
	unsigned char *data;

	data = mlnds->pdu_base_addr;
	data += pdu_offset;

	bcopy(buf, data, n_bytes);
	return (1);
}

static void
mlndo_tattle(struct mlndr_stream *mlnds, char *what,
    struct ndr_reference *ref)
{
	return;
}

static void
mlndo_tattle_error(struct mlndr_stream *mlnds, struct ndr_reference *ref)
{
	return;
}

/*
 * mlndo_reset
 *
 * Reset a stream: zap the outer_queue. We don't need to tamper
 * with the stream heap: it's handled externally to the stream.
 */
static int
mlndo_reset(struct mlndr_stream *mlnds)
{
	mlnds->pdu_size = 0;
	mlnds->pdu_scan_offset = 0;
	mlnds->outer_queue_head = 0;
	mlnds->outer_current = 0;
	mlnds->outer_queue_tailp = &mlnds->outer_queue_head;

	return (1);
}

/*
 * mlndo_destruct
 *
 * Destruct a stream: zap the outer_queue. Currently, this operation isn't
 * required because the heap management (creation/destruction) is external
 * to the stream but it provides a place-holder for stream cleanup.
 */
static void
mlndo_destruct(struct mlndr_stream *mlnds)
{
	if (mlnds->pdu_base_addr != 0) {
		free(mlnds->pdu_base_addr);
		mlnds->pdu_base_addr = 0;
	}

	mlnds->outer_queue_head = 0;
	mlnds->outer_current = 0;
	mlnds->outer_queue_tailp = &mlnds->outer_queue_head;
}

/*ARGSUSED*/
void
mlndo_trace(const char *s)
{
	/*
	 * Temporary fbt for dtrace until user space sdt enabled.
	 */
	syslog(LOG_DEBUG, "%s", s);
}