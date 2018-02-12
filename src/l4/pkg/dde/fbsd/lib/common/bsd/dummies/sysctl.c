/**
 * FreeBSD implementation in kern/kern_sysctl.c
 */
/*-
 * Copyright (c) 1982, 1986, 1989, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Karels at Berkeley Software Design, Inc.
 *
 * Quite extensively rewritten by Poul-Henning Kamp of the FreeBSD
 * project, to make these variables more userfriendly.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 *
 *      @(#)kern_sysctl.c       8.4 (Berkeley) 4/14/94
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/queue.h>
#include <sys/malloc.h>

static MALLOC_DEFINE(M_SYSCTL, "sysctl", "sysctl internal magic");
static MALLOC_DEFINE(M_SYSCTLOID, "sysctloid", "sysctl dynamic oids");
static MALLOC_DEFINE(M_SYSCTLTMP, "sysctltmp", "sysctl temp output buffer");

int sysctl_handle_long(SYSCTL_HANDLER_ARGS) {
	printf("sysctl_handle_long()");
	return EPERM;
}

int sysctl_handle_int(SYSCTL_HANDLER_ARGS) {
	printf("sysctl_handle_int()");
	return EPERM;
}

int sysctl_handle_string(SYSCTL_HANDLER_ARGS) {
	printf("sysctl_handle_string()");
	return EPERM;
}

struct sysctl_oid *
sysctl_add_oid(struct sysctl_ctx_list *clist, struct sysctl_oid_list *parent,
        int number, const char *name, int kind, void *arg1, int arg2,
        int (*handler)(SYSCTL_HANDLER_ARGS), const char *fmt, const char *descr)
{
	struct sysctl_oid *oidp;
	ssize_t len;
	char *newname;

	if (0) printf("sysctl_add_oid(name=%s fmt=%s desc=%s)", name, fmt, descr);

	oidp = malloc(sizeof(struct sysctl_oid), M_SYSCTLOID, M_WAITOK|M_ZERO);
	oidp->oid_parent = parent;
	SLIST_NEXT(oidp, oid_link) = NULL;
	oidp->oid_number = number;
	oidp->oid_refcnt = 1;
	len = strlen(name);
	newname = malloc(len + 1, M_SYSCTLOID, M_WAITOK);
	bcopy(name, newname, len + 1);
	newname[len] = '\0';
	oidp->oid_name = newname;
	oidp->oid_handler = handler;
	oidp->oid_kind = CTLFLAG_DYN | kind;
	oidp->oid_fmt = fmt;
	if (descr) {
		int len = strlen(descr) + 1;
		oidp->descr = malloc(len, M_SYSCTLOID, M_WAITOK);
		if (oidp->descr)
			strcpy((char *)(uintptr_t)(const void *)oidp->descr, descr);
	}

	return (oidp);
}

int sysctl_ctx_init(struct sysctl_ctx_list *c) {
	if (c == NULL) {
		return (EINVAL);
	}
	TAILQ_INIT(c);
	return 0;
}

int sysctl_ctx_free(struct sysctl_ctx_list *clist) {
	printf("sysctl_ctx_free()");

	return 0;
}

struct sysctl_oid_list sysctl__children;

int sysctl_handle_opaque(SYSCTL_HANDLER_ARGS) {
	printf("sysctl_handle_opaque()");
	return 0;
}
