.PATH: ${.CURDIR}/ppc/sys

MDSRCS+= OSAtomic.s \
	ldbl64.s \
	_longjmp.s \
	longjmp.s \
	ppc_gettimeofday.s \
	_setjmp.s \
	setjmp.s \
	_sigtramp.s

MDCOPYFILES+= ${.CURDIR}/ppc/sys/libc.syscall.ppc
