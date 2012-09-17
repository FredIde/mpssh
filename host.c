/*-
 * Copyright (c) 2005-2012 Nikolay Denev <ndenev@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "mpssh.h"
#include "host.h"

/*
 * routine for allocating a new host element in the
 * linked list containing the hosts read from the file.
 * it is used internally by host_add().
 */
static struct host*
host_new(char *user, char *host, uint16_t port)
{
	static struct host *hst;

	if (!(hst = calloc(1, sizeof(struct host)))) goto fail;

	if (user) {
		if (!(hst->user = calloc(1, strlen(user)+1))) goto fail;
		strncpy(hst->user, user, strlen(user));
	} else {
		hst->user = NULL;
	}

	if (host) {
		if (!(hst->host = calloc(1, strlen(host)+1))) goto fail;
		strncpy(hst->host, host, strlen(host));
	} else {
		hst->host = NULL;
	}

	hst->port = port;

	hst->next = NULL;

	return(hst);
fail:
	fprintf(stderr, "%s\n", strerror(errno));
	return(NULL);
}

/*
 * routine for adding elements in the existing hostlist
 * linked list.
 */
static struct host*
host_add(struct host *hst, char *user, char *host, uint16_t port)
{
	if (hst == NULL)
		return(host_new(user, host, port));

	hst->next = host_add(hst->next, user, host, port);
	return(hst->next);
}

/*
 * routine that reads the host from a file and puts them
 * in the hostlist linked list using the above two routines
 */
struct host*
host_readlist(char *fname)
{
	FILE		*hstlist;
	struct host	*hst;
	struct host	*hst_head;
	char		line[MAXNAME*3];
	int		i;
	int 		linelen;
	unsigned long	port;
	int		fnamelen;
	char		*login = NULL;
	char		*hostname = NULL;
	char		*llabel = NULL;
	char		*home;

	if (fname == NULL) {
		home = getenv("HOME");
		if (!home)
			return(NULL);

		fnamelen = strlen(home) + strlen("/"HSTLIST) + 1;

		fname = calloc(1, fnamelen);

		if (!fname)
			return(NULL);

		sprintf(fname, "%s/"HSTLIST, home);
	}

	hstlist = fopen(fname, "r");

	if (hstlist == NULL) {
		fprintf(stderr, "Can't open file: %s (%s)\n",
			fname, strerror(errno));
		return(NULL);
	}

	hst_head = hst = host_add(NULL, NULL, NULL, 0);

	while (fgets(line, sizeof(line), hstlist)) {
		if (sscanf(line, "%[A-Za-z0-9-.@:%]", line) != 1) {
			continue;
		}

		linelen = strlen(line);

		if (line[0] == '%') {
			if (llabel)
				free(llabel);
			llabel = calloc(1, linelen + 1);
			if (llabel == NULL) {
				fprintf(stderr, "%s\n", strerror(errno));
				free(llabel);
				return(NULL);
			}
			strncpy(llabel, &line[1], linelen);
			continue;
		}

		hostname = line;
		login = NULL;
		port = 0;

		for (i=0; i < linelen; i++) {
			switch (line[i]) {
				case '@':
					if (login)
						break;
					line[i] = '\0';
					login = line;
					hostname = &line[i+1];
					break;
				case ':':
					line[i] = '\0';
					port = strtol(&line[i+1], (char **)NULL, 10);
					break;
				default:
					break;
			}
		}

		if (!hostname)
			continue;

		errno = 0;

		if (!login)
			login = user;

		/* check if labels match */
		if (label && llabel) {
			if (strcmp(llabel, label))
				continue;
		}

		/* add the host record */
		hst = host_add(hst, login, hostname, (uint16_t)port?(uint16_t)port:SSHDEFPORT);

		port = 0;

		if (hst == NULL)
			return(NULL);

		/* keep track of the longest username */
		if (login && strlen(login) > user_len_max)
			user_len_max = strlen(login);

		/* keep track of the longest line */
		if (strlen(hostname) > host_len_max)
			host_len_max = strlen(hostname);

		hostcount++;
	}
	if (llabel)
		free(llabel);
	fclose(hstlist);
	if (maxchld > hostcount) maxchld = hostcount;
	hst = hst_head->next;
	free(hst_head);
	return(hst);
}