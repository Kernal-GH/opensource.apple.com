#!/bin/sh
# Copyright (C) John H Terpstra 1998-2002
#               Gerald (Jerry) Carter 2003

# The following allows environment variables to override the target directories
#   the alternative is to have a file in your home directory calles .rpmmacros
#   containing the following:
#   %_topdir  /home/mylogin/redhat
#
# Note: Under this directory rpm expects to find the same directories that are under the
#   /usr/src/redhat directory
#

SPECDIR=`rpm --eval %_specdir`
SRCDIR=`rpm --eval %_sourcedir`

# At this point the SPECDIR and SRCDIR vaiables must have a value!

USERID=`id -u`
GRPID=`id -g`
VERSION='3.0.10'
SPECFILE="samba.spec"
RPMVER=`rpm --version | awk '{print $3}'`
RPM="rpmbuild"

##
## Check the RPM version (paranoid)
##
case $RPMVER in
    4*)
       echo "Supported RPM version [$RPMVER]"
       ;;
    *)
       echo "Unknown RPM version: `rpm --version`"
       exit 1
       ;;
esac

( cd ../../source; if [ -f Makefile ]; then make distclean; fi )
( cd ../../.. ; chown -R ${USERID}.${GRPID} samba-${VERSION} )

( cd ../../.. ; tar --exclude=CVS -cf - samba-${VERSION}/. | bzip2 > ${SRCDIR}/samba-${VERSION}.tar.bz2 )

##
## copy additional source files
##
for file in samba.pamd samba.sysconfig samba.spec \
	smb.init swat.desktop filter-requires-samba.sh \
	samba.log samba.xinetd smbprint winbind.init \
	smb.conf smbusers
do
	cp -p $file ${SRCDIR}

done

chmod 755 ${SRCDIR}/filter-requires-samba.sh

cp -p ${SPECFILE} ${SPECDIR}

##
## Build
##
echo Getting Ready to build release package
cd ${SPECDIR}
${RPM} -ba --clean --rmsource $SPECFILE

echo Done.

