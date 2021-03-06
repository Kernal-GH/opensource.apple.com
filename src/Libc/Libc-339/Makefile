#	@(#)Makefile	8.2 (Berkeley) 2/3/94
# $FreeBSD: src/lib/libc/Makefile,v 1.31 2001/08/13 21:48:43 peter Exp $
#
# All library objects contain rcsid strings by default; they may be
# excluded as a space-saving measure.  To produce a library that does
# not contain these strings, delete -DLIBC_RCS and -DSYSLIBC_RCS
# from CFLAGS below.  To remove these strings from just the system call
# stubs, remove just -DSYSLIBC_RCS from CFLAGS.
#
# Yes, we build everything with -g, and strip it out later...
#
LIB=c
SHLIB_MAJOR= 1
SHLIB_MINOR= 0
CC_3_3_OR_GREATER != ${.CURDIR}/cc-3.3-or-greater
.if (${CC_3_3_OR_GREATER} != YES)
CC = gcc-3.3
.endif
.if (${MACHINE_ARCH} == unknown)
MACHINE_ARCH != /usr/bin/arch
.endif 
.if (${MACHINE_ARCH} == ppc) || (${MACHINE_ARCH} == ppc64)
CFLAGS += -faltivec -DALTIVEC
.endif
CFLAGS += -DNOID
.ifdef ALTLIBCHEADERS
CFLAGS += -I${ALTLIBCHEADERS}
.else
CFLAGS += -I${.CURDIR}/include
.endif
.ifdef ALTFRAMEWORKSPATH
PRIVINC = -F${ALTFRAMEWORKSPATH} -I${ALTFRAMEWORKSPATH}/System.framework/PrivateHeaders
.else
PRIVINC = -I${NEXT_ROOT}/System/Library/Frameworks/System.framework/PrivateHeaders
.endif
CFLAGS += ${PRIVINC}
CFLAGS += -DLIBC_MAJOR=${SHLIB_MAJOR} -no-cpp-precomp -force_cpusubtype_ALL
CFLAGS += -arch ${MACHINE_ARCH} -fno-common -pipe -Wmost -g
CFLAGS += -finline-limit=5000 -D__FBSDID=__RCSID -Wno-long-double
AINC=	-I${.CURDIR}/${MACHINE_ARCH} -no-cpp-precomp -force_cpusubtype_ALL
AINC+=-arch ${MACHINE_ARCH} -g
CLEANFILES+=tags
INSTALL_PIC_ARCHIVE=	yes
PRECIOUSLIB=	yes

# If these aren't set give it expected defaults
DSTROOT ?= /
OBJROOT ?= .
SRCROOT ?= ${.CURDIR}
.ifndef SYMROOT
SYMROOT = ${.CURDIR}/SYMROOT
_x_ != test -d ${SYMROOT} || mkdir -p ${SYMROOT}
.endif
DESTDIR ?= ${DSTROOT}
MAKEOBJDIR ?= ${OBJROOT}

CFLAGS += -I${SYMROOT}
.include "${.CURDIR}/Makefile.inc"
.PATH: ${SYMROOT}
.include "Makefile.xbs"
.include <bsd.man.mk>
