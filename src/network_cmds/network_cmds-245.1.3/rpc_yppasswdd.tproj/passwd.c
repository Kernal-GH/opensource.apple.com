/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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
/*	$NetBSD: passwd.c,v 1.11 1997/12/31 05:47:15 thorpej Exp $	*/

/*
 * Copyright (c) 1987, 1993, 1994, 1995
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

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: passwd.c,v 1.11 1997/12/31 05:47:15 thorpej Exp $");
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <paths.h>
#include <signal.h>
#include <limits.h>
#include <util.h>

static void	pw_cont __P((int sig));
static int	pw_equal __P((char *buf, struct passwd *old_pw));

int
pw_lock(retries)
	int retries;
{
	int i, fd;
	mode_t old_mode;

	/* Acquire the lock file. */
	old_mode = umask(0);
	fd = open(_PATH_MASTERPASSWD_LOCK, O_WRONLY|O_CREAT|O_EXCL, 0600);
	for (i = 0; i < retries && fd < 0 && errno == EEXIST; i++) {
		sleep(1);
		fd = open(_PATH_MASTERPASSWD_LOCK, O_WRONLY|O_CREAT|O_EXCL,
			  0600);
	}
	umask(old_mode);
	return(fd);
}

int
pw_mkdb()
{
	int pstat;
	pid_t pid;
	struct stat sb;

	/* A zero length passwd file is never ok */
	if (stat(_PATH_MASTERPASSWD_LOCK, &sb) == 0) {
		if (sb.st_size == 0) {
			warnx("%s is zero length", _PATH_MASTERPASSWD_LOCK);
			return (-1);
		}
	}

	pid = vfork();
	if (pid == 0) {
		execl(_PATH_PWD_MKDB, "pwd_mkdb", "-p",
		      _PATH_MASTERPASSWD_LOCK, NULL);
		_exit(1);
	}
	pid = waitpid(pid, &pstat, 0);
	if (pid == -1 || !WIFEXITED(pstat) || WEXITSTATUS(pstat) != 0)
		return(-1);
	return(0);
}

int
pw_abort()
{
	return(unlink(_PATH_MASTERPASSWD_LOCK));
}

/* Everything below this point is intended for the convenience of programs
 * which allow a user to interactively edit the passwd file.  Errors in the
 * routines below will cause the process to abort. */

static pid_t editpid = -1;

static void
pw_cont(sig)
	int sig;
{

	if (editpid != -1)
		kill(editpid, sig);
}

void
pw_init()
{
	struct rlimit rlim;

	/* Unlimited resource limits. */
	rlim.rlim_cur = rlim.rlim_max = RLIM_INFINITY;
	(void)setrlimit(RLIMIT_CPU, &rlim);
	(void)setrlimit(RLIMIT_FSIZE, &rlim);
	(void)setrlimit(RLIMIT_STACK, &rlim);
	(void)setrlimit(RLIMIT_DATA, &rlim);
	(void)setrlimit(RLIMIT_RSS, &rlim);

	/* Don't drop core (not really necessary, but GP's). */
	rlim.rlim_cur = rlim.rlim_max = 0;
	(void)setrlimit(RLIMIT_CORE, &rlim);

	/* Turn off signals. */
	(void)signal(SIGALRM, SIG_IGN);
	(void)signal(SIGHUP, SIG_IGN);
	(void)signal(SIGINT, SIG_IGN);
	(void)signal(SIGPIPE, SIG_IGN);
	(void)signal(SIGQUIT, SIG_IGN);
	(void)signal(SIGTERM, SIG_IGN);
	(void)signal(SIGCONT, pw_cont);
}

void
pw_edit(notsetuid, filename)
	int notsetuid;
	const char *filename;
{
	int i, xargc, pstat;
	char *p, *editor;
	char **xargv;
#ifdef __GNUC__
	(void) &editor;
#endif

	if (filename == NULL)
		filename = _PATH_MASTERPASSWD_LOCK;
	if ((editor = getenv("EDITOR")) == NULL)
		editor = strdup(_PATH_VI);
	else
		editor = strdup(editor);
	if ((p = strrchr(editor, '/')))
		++p;
	else
		p = editor;

	/* Scan editor string, count spaces, allocate arg vector. */
	for (i = 0, xargc = 0; p[i] != '\0'; i++) {
		if (isspace(p[i])) {
			while (isspace(p[i++]))
				/* skip white space */ ;
			if (p[i] == '\0')
				break;
			xargc++;
		}
	}

	/* argv[0] + <xargc args> + filename + NULL */
	xargv = (char **)malloc(sizeof(char *) * (xargc + 3));
	if (xargv == NULL)
		pw_error("malloc failed", 1, 1);

	i = 0;
	xargv[i++] = p;
	for (; *p != '\0'; p++) {
		if (isspace(*p)) {
			while(isspace(*p))
				*p++ = '\0';	/* blast whitespace */
			if (*p == '\0')
				break;
			xargv[i++] = p;
		}
	}

	xargv[i++] = (char *)filename;
	xargv[i] = NULL;

	if (!(editpid = vfork())) {
		if (notsetuid) {
			setgid(getgid());
			setuid(getuid());
		}
		execvp(editor, xargv);
		_exit(1);
	}
	for (;;) {
		editpid = waitpid(editpid, (int *)&pstat, WUNTRACED);
		if (editpid == -1)
			pw_error(editor, 1, 1);
		else if (WIFSTOPPED(pstat))
			raise(WSTOPSIG(pstat));
		else if (WIFEXITED(pstat) && WEXITSTATUS(pstat) == 0)
			break;
		else
			pw_error(editor, 1, 1);
	}
	editpid = -1;
	free(editor);
	free(xargv);
}

void
pw_prompt()
{
	int c;

	(void)printf("re-edit the password file? [y]: ");
	(void)fflush(stdout);
	c = getchar();
	if (c != EOF && c != '\n')
		while (getchar() != '\n');
	if (c == 'n')
		pw_error(NULL, 0, 0);
}

/* for use in pw_copy(). Compare a pw entry to a pw struct. */
static int
pw_equal (buf, pw)
	char *buf;
	struct passwd *pw;
{
	struct passwd buf_pw;
	int len = strlen (buf);
	if (buf[len-1] == '\n')
		buf[len-1] = '\0';
	if (!pw_scan(buf, &buf_pw, NULL))
		return 0;
	return !strcmp(pw->pw_name, buf_pw.pw_name)
		&& pw->pw_uid == buf_pw.pw_uid
		&& pw->pw_gid == buf_pw.pw_gid
		&& !strcmp(pw->pw_class, buf_pw.pw_class)
		&& (long)pw->pw_change == (long)buf_pw.pw_change
		&& (long)pw->pw_expire == (long)buf_pw.pw_expire
		&& !strcmp(pw->pw_gecos, buf_pw.pw_gecos)
		&& !strcmp(pw->pw_dir, buf_pw.pw_dir)
		&& !strcmp(pw->pw_shell, buf_pw.pw_shell);
}

void
pw_copy(ffd, tfd, pw, old_pw)
	int ffd, tfd;
	struct passwd *pw, *old_pw;
{
	FILE *from, *to;
	int done;
	char *p, buf[8192];

	if (!(from = fdopen(ffd, "r")))
		pw_error(_PATH_MASTERPASSWD, 1, 1);
	if (!(to = fdopen(tfd, "w")))
		pw_error(_PATH_MASTERPASSWD_LOCK, 1, 1);

	for (done = 0; fgets(buf, sizeof(buf), from);) {
		if (!strchr(buf, '\n')) {
			warnx("%s: line too long", _PATH_MASTERPASSWD);
			pw_error(NULL, 0, 1);
		}
		if (done) {
			(void)fprintf(to, "%s", buf);
			if (ferror(to))
				goto err;
			continue;
		}
		if (buf[0] == '#') {
			/* skip comments for Rhapsody. */
			continue;
		}
		if (!(p = strchr(buf, ':'))) {
			warnx("%s: corrupted entry", _PATH_MASTERPASSWD);
			pw_error(NULL, 0, 1);
		}
		*p = '\0';
		if (strcmp(buf, pw->pw_name)) {
			*p = ':';
			(void)fprintf(to, "%s", buf);
			if (ferror(to))
				goto err;
			continue;
		}
		*p = ':';
		if (old_pw && !pw_equal(buf, old_pw)) {
			warnx("%s: entry inconsistent",
			      _PATH_MASTERPASSWD);
			pw_error(NULL, 0, 1);
		}
		(void)fprintf(to, "%s:%s:%d:%d:%s:%ld:%ld:%s:%s:%s\n",
		    pw->pw_name, pw->pw_passwd, pw->pw_uid, pw->pw_gid,
		    pw->pw_class, (long)pw->pw_change, (long)pw->pw_expire,
		    pw->pw_gecos, pw->pw_dir, pw->pw_shell);
		done = 1;
		if (ferror(to))
			goto err;
	}
	/* Only append a new entry if real uid is root! */
	if (!done) 
		if (getuid() == 0)
			(void)fprintf(to, "%s:%s:%d:%d:%s:%ld:%ld:%s:%s:%s\n",
			    pw->pw_name, pw->pw_passwd, pw->pw_uid, pw->pw_gid,
			    pw->pw_class, (long)pw->pw_change,
			    (long)pw->pw_expire, pw->pw_gecos, pw->pw_dir,
			    pw->pw_shell);
		else
			warnx("%s: changes not made, no such entry",
		      	    _PATH_MASTERPASSWD);

	if (ferror(to))
err:		pw_error(NULL, 1, 1);
	(void)fclose(to);
}

int
pw_scan(bp, pw, flags)
	char *bp;
	struct passwd *pw;
	int *flags;
{
	unsigned long id;
	int root;
	char *p, *sh, *ep;

	if (flags != (int *)NULL)
		*flags = 0;

	if (!(pw->pw_name = strsep(&bp, ":")))		/* login */
		goto fmt;
	root = !strcmp(pw->pw_name, "root");

	if (!(pw->pw_passwd = strsep(&bp, ":")))	/* passwd */
		goto fmt;

	if (!(p = strsep(&bp, ":")))			/* uid */
		goto fmt;
	id = strtoul(p, &ep, 10);
	if (root && id) {
		warnx("root uid should be 0");
		return (0);
	}
	if (id > UID_MAX || *ep != '\0') {
		warnx("invalid uid '%s'", p);
		return (0);
	}
	pw->pw_uid = (uid_t)id;
	if ((*p == '\0') && (flags != (int *)NULL))
		*flags |= _PASSWORD_NOUID;

	if (!(p = strsep(&bp, ":")))			/* gid */
		goto fmt;
	id = strtoul(p, &ep, 10);
	if (id > GID_MAX || *ep != '\0') {
		warnx("invalid gid '%s'", p);
		return (0);
	}
	pw->pw_gid = (gid_t)id;
	if ((*p == '\0') && (flags != (int *)NULL))
		*flags |= _PASSWORD_NOGID;

	pw->pw_class = strsep(&bp, ":");		/* class */
	if (!(p = strsep(&bp, ":")))			/* change */
		goto fmt;
	pw->pw_change = atol(p);
	if ((*p == '\0') && (flags != (int *)NULL))
		*flags |= _PASSWORD_NOCHG;
	if (!(p = strsep(&bp, ":")))			/* expire */
		goto fmt;
	pw->pw_expire = atol(p);
	if ((*p == '\0') && (flags != (int *)NULL))
		*flags |= _PASSWORD_NOEXP;
	pw->pw_gecos = strsep(&bp, ":");		/* gecos */
	pw->pw_dir = strsep(&bp, ":");			/* directory */
	if (!(pw->pw_shell = strsep(&bp, ":")))		/* shell */
		goto fmt;

	p = pw->pw_shell;
	if (root && *p)					/* empty == /bin/sh */
		for (setusershell();;) {
			if (!(sh = getusershell())) {
				warnx("warning, unknown root shell");
				break;
			}
			if (!strcmp(p, sh))
				break;	
		}

	if ((p = strsep(&bp, ":"))) {			/* too many */
fmt:		warnx("corrupted entry");
		return (0);
	}

	return (1);
}

void
pw_error(name, err, eval)
	const char *name;
	int err, eval;
{
	if (err)
		warn(name);

	warnx("%s: unchanged", _PATH_MASTERPASSWD);
	pw_abort();
	exit(eval);
}

