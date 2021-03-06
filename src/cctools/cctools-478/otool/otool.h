/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#import <stuff/bool.h>

/* Name of this program for error messages (argv[0]) */
extern char *progname;

/*
 * The flags to indicate the actions to perform.
 */
extern enum bool fflag; /* print the fat headers */
extern enum bool aflag; /* print the archive header */
extern enum bool hflag; /* print the exec or mach header */
extern enum bool lflag; /* print the load commands */
extern enum bool Lflag; /* print the shared library names */
extern enum bool Dflag; /* print the shared library id name */
extern enum bool tflag; /* print the text */
extern enum bool dflag; /* print the data */
extern enum bool oflag; /* print the objctive-C info */
extern enum bool Oflag; /* print the objctive-C selector strings only */
extern enum bool rflag; /* print the relocation entries */
extern enum bool Sflag; /* print the contents of the __.SYMDEF file */
extern enum bool vflag; /* print verbosely (symbolicly) when possible */
extern enum bool Vflag; /* print dissassembled operands verbosely */
extern enum bool cflag; /* print the argument and environ strings of a core file */
extern enum bool iflag; /* print the shared library initialization table */
extern enum bool Wflag; /* print the mod time of an archive as an decimal number */
extern enum bool Xflag; /* don't print leading address in disassembly */
extern enum bool Zflag; /* don't use simplified ppc mnemonics in disassembly */
extern char *pflag; 	/* procedure name to start disassembling from */
extern char *segname,
     *sectname;	    /* name of the section to print the contents of */

unsigned long m68k_usrstack(void);
unsigned long m88k_usrstack(void);
unsigned long i386_usrstack(void);
unsigned long hppa_usrstack(void);
unsigned long sparc_usrstack(void);
