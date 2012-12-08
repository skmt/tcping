/*
 * Copyright (c) 2006  Tsuyoshi SAKAMOTO <skmt@japan.email.ne.jp>,
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
*/


/* TODO:
 * (1) mss option
*/


/*----------------------------------------------------------------------------
 * include file
 *----------------------------------------------------------------------------
*/
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <stdlib.h>
#include <unistd.h>

#include <signal.h>
#include <ctype.h>
#include <sys/time.h>

#include <stdarg.h>


/*----------------------------------------------------------------------------
 * macro
 *----------------------------------------------------------------------------
*/

#define NOTDIE	0
#define DIE	1



/*----------------------------------------------------------------------------
 * type definition
 *----------------------------------------------------------------------------
*/

/*
 * for short-cut
*/
typedef struct sockaddr SA;
typedef struct sockaddr_in SA_IN;
typedef struct in_addr ADDR;


/*
 * option
*/
typedef struct {
	int debug;
	int verbose;
	int quiet;
	int loop;	/* loop count: 0 endless, 0<n time(s) */
	int decrement;
	int mss;
	char *psrc;
	char *srcport;
	SA *src;
	socklen_t srclen;
	char *pdst;
	char *dstport;
	SA *dst;
	socklen_t dstlen;
} Opt;


/*----------------------------------------------------------------------------
 * global variable
 *----------------------------------------------------------------------------
*/
Opt *opt;

static long pinging = 0;	/* total pinging counter */
static long success = 0;



/*----------------------------------------------------------------------------
 * proto type
 *----------------------------------------------------------------------------
*/



/*============================================================================
 * program section
 *============================================================================
*/

/*----------------------------------------------------------------------------
 * print error and die
 *----------------------------------------------------------------------------
*/
void
tcp_printf(int die, const char *format, ...) {
	va_list ap;
	va_start(ap, format);

	if (!opt->quiet) {
		vfprintf(stderr, format, ap);
		fprintf(stderr, "\n");
	}

	va_end(ap);

	if (die)
		exit (1);

	return;
}


/*----------------------------------------------------------------------------
 * print usage
 *----------------------------------------------------------------------------
*/
void
tcp_print_usage() {
	fprintf(stderr,
		"This is TCP pinging program version 0.1,\n"
		"tcping'll open TCP session by 3-way handshake and send "
		"nothing more of that.\n");
	fprintf(stderr,
		"----------------------------------------------------------\n");
	fprintf(stderr,
		" usage: tcping [OPTIONS] target port\n");
	fprintf(stderr,
		" options: [-q ] [-i souce ] [-s | -c count] [-l mss] [-v]\n");
	fprintf(stderr,
		"  -q     quiet mode, don't print progress and statistics\n");
	fprintf(stderr,
		"  -i     binded source ip\n");
	fprintf(stderr,
		"  -s     endless loop mode\n");
	fprintf(stderr,
		"  -c     pinging n times\n");
	fprintf(stderr,
		"  -l     tcp segment size (MSS) ***not implemeted***\n");
	fprintf(stderr,
		"  -v     verbose mode, print reply if it's printable\n\n");

	exit(1);
}


/*----------------------------------------------------------------------------
 * parse option
 *----------------------------------------------------------------------------
*/
int
tcp_getaddrinfo(void) {
	struct addrinfo dst_hints, src_hints;
	struct addrinfo *dst, *src;

	memset(&dst_hints, 0, sizeof(dst_hints));
	dst_hints.ai_flags     = AI_CANONNAME;
	dst_hints.ai_family    = AF_INET;
	dst_hints.ai_socktype  = SOCK_STREAM;
	dst_hints.ai_protocol  = IPPROTO_TCP;
	if (getaddrinfo(opt->pdst, opt->dstport, &dst_hints, &dst) != 0)
		tcp_printf(DIE, "invalid target host");
	opt->dst = dst->ai_addr;
	opt->dstlen = dst->ai_addrlen;

	if (opt->psrc == NULL)
		return (0);
	memset(&src_hints, 0, sizeof(src_hints));
	src_hints.ai_flags     = AI_CANONNAME;
	src_hints.ai_family    = AF_INET;
	src_hints.ai_socktype  = SOCK_STREAM;
	src_hints.ai_protocol  = IPPROTO_TCP;
	if (getaddrinfo(opt->psrc, NULL, &src_hints, &src) != 0)
		tcp_printf(DIE, "invalid source host");
	opt->src = src->ai_addr;
	opt->srclen = src->ai_addrlen;

	return (0);
}

int
tcp_parse_arg(char **argv) {
	int i;
	char *p1 = *argv;
	char *p2 = *(argv + 1);
	int len1, len2;

	opt->pdst = NULL;
	opt->dstport = NULL;

	len1 = strlen(p1);
	for (i = 0; i < len1; ++i) {
		if (!isdigit(p1[i])) {
			opt->pdst = strdup(p1);
			break;
		}
	}
	if (i == len1)
		opt->dstport = strdup(p1);

	len2 = strlen(p2);
	for (i = 0; i < len2; ++i) {
		if (!isdigit(p2[i])) {
			opt->pdst = strdup(p2);
			break;
		}
	}
	if (i == len2)
		opt->dstport = strdup(p2);

	if (opt->pdst == NULL || opt->dstport == NULL)
		tcp_printf(DIE, "invalid host and port");
	
	return (0);
}


void
tcp_opt(int argc, char **argv) {
	int ch;
	int cflag = 0, sflag = 0;
	int count;
	SA_IN *psa;
	int mss;

	if ((opt = calloc(1, sizeof(Opt))) == NULL)
		tcp_printf(DIE, "can not alloc opt\n");

	opt->loop       = 1;
	opt->decrement  = 1;
	opt->mss        = 0;

	while ((ch = getopt(argc, argv, "dhi:c:ql:sv")) != -1) {
		switch(ch) {
		case 'c':
			if ((count = atoi(optarg)) == 0 || count > 30000)
				tcp_printf(DIE, "invalid count");
			cflag = 1;
			opt->loop = count;
			break;
		case 'd':
			opt->debug = 1;
			break;
		case 'i':
			opt->psrc = strdup(optarg);
			break;
		case 'l':
			tcp_printf(NOTDIE, "not implement so far");
			if ((mss = atoi(optarg)) > 1460 || mss < 576)
				tcp_printf(DIE, "invalid mss");
			opt->mss = mss;
			break;
		case 'q':
			opt->quiet = 1;
			break;
		case 's':
			sflag = 1;
			opt->decrement = 0;
			break;
		case 'v':
			opt->verbose = 1;
			break;
		case 'h':
		default:
			tcp_print_usage();
			break;
		}
	}

	if (cflag && sflag)
		tcp_printf(DIE, "can not give both of the option, -c and -s");

	argc -= optind;
	argv += optind;

	if (argc != 2)
		tcp_printf(DIE, "not specified host and port");

	(void) tcp_parse_arg(argv);
	(void) tcp_getaddrinfo();

	psa = (SA_IN *)opt->dst;
	tcp_printf(NOTDIE, "tcp pinging to %s:%s (%s)",
		opt->pdst, opt->dstport, inet_ntoa(psa->sin_addr));

	return;
}


/*----------------------------------------------------------------------------
 * print result
 *----------------------------------------------------------------------------
*/
void
tcp_print_result() {
	tcp_printf(NOTDIE, "\n--- %s tcp ping statistics ---", opt->pdst);
	tcp_printf(NOTDIE, "%ld packet(s) transmitted, %2.1f%% tcp ping successful",
		pinging, (float)(100 * success / pinging));

	exit (success ? 0 : 1);
}


/*----------------------------------------------------------------------------
 * print time of excusion
 *----------------------------------------------------------------------------
*/
void
tcp_print_roundtrip(struct timeval *start, struct timeval *end) {
	static long int n = 0;

	if (start == NULL || end == NULL) {
		tcp_printf(NOTDIE, "can not connect target");
		return;
	}

	if (end->tv_sec == start->tv_sec)
		tcp_printf(NOTDIE, " seq %ld, RRT: %6.4f (ms)",
			++n, (float)(end->tv_usec - start->tv_usec) / 1000);
	else
		tcp_printf(NOTDIE, " seq %ld, RRT: %ld (s)",
			++n, (float)(end->tv_sec - start->tv_sec));

	return;
}

int
tcp_gettimeofday(struct timeval *tv) {
	if (gettimeofday(tv, NULL) == 0)
		return (0);
	tcp_printf(DIE, "can not gettimeofday, quit immediately");
	/* NOTREACHED */
	return (-1);
}


/*----------------------------------------------------------------------------
 * send packet
 *----------------------------------------------------------------------------
*/
int
tcp_get_socket(void) {
	int fd;
	int on = 1;

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		tcp_printf(DIE, "can not assign socket");
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
		tcp_printf(DIE, "can not set sockopt");
	if (opt->psrc)
		if (bind(fd, opt->src, opt->srclen) < 0)
			tcp_printf(DIE, "can not bind source");
	/*
	if (opt->mss) {
		if (setsockopt(fd, IPPROTO_TCP, TCP_MAXSEG, &opt->mss, sizeof(opt->mss)) < 0) {
			tcp_printf(NOTDIE, "ignore mss you set (%d)", opt->mss);
		}
	}

	*/
	
	return (fd);
}

int
tcp_select(int fd) {
	struct timeval tv;;
	fd_set rd;
	int n;

	for (;;) {
		FD_ZERO(&rd);
		FD_SET(fd, &rd);
		memset(&tv, 0, sizeof(tv));
		tv.tv_sec = 1;
		/*
		 * timeval has to be less than one second
		*/
		if ((n = select((fd + 1), &rd, NULL, NULL, &tv)) > 0) {
			if (FD_ISSET(fd, &rd))
				return (n);
			else
				tcp_printf(NOTDIE, "missing file descriptor");
		}
		else if (n < 0) {
			if (errno == EINTR)
				continue;
			else
				tcp_printf(NOTDIE, "select error occured");
		}
	}

	/* timeouted */
	return (0);
}

int
tcp_close(int fd) {
	return (close(fd));
}

int
tcp_print_reply(char *p, int s) {
	int i;

	if (p == NULL || s == 0)
		return (0);

	/*
	 * I want to print reply message in a row, so over 70 char have to
	 * be droped.
	*/
	for (i = 0; i < 70 && i < s && isprint(p[i]); ++i) { }
	p[i] = '\0';

	tcp_printf(NOTDIE, "  --> %s", p);
	return (0);
}

void
tcp_send(void) {
	int sockfd;
	struct timeval start, end;
	int nfd;
	char buff[4096];
	ssize_t n;

	/*
	 * decrement counter to exit main loop, see below main() in detail
	*/
	opt->loop -= opt->decrement;
	++pinging;


	if ((sockfd = tcp_get_socket()) < 0)
		tcp_printf(DIE, "can not assign socket");

	tcp_gettimeofday(&start);
	if (connect(sockfd, opt->dst, opt->dstlen) < 0) {
		tcp_print_roundtrip(NULL, NULL);
		return;
	}
	/*
	else
		tcp_gettimeofday(&end);
	*/

	/*
	 * if vebose mode, accept reply and print message if pritable
	*/
	n = 0;
	if (opt->verbose) {
		if ((nfd = tcp_select(sockfd)) > 0)
			n = recvfrom(sockfd, buff, sizeof(buff), 0, NULL, NULL);
	}
	
	if (tcp_close(sockfd) == 0)
		++success;
	tcp_gettimeofday(&end);
	tcp_print_roundtrip(&start, &end);

	if (opt->verbose && n > 0)
		(void) tcp_print_reply(buff, n);

	return;
}

/*----------------------------------------------------------------------------
 * signal handler
 *----------------------------------------------------------------------------
*/
void
tcp_sigsend(void) {
	tcp_send();
	alarm(1);

	return;
}

void
tcp_set_signal_interupt(void) {
	struct sigaction act, oact;

	act.sa_handler = (void (*)())tcp_print_result;

	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;

	if (sigaction(SIGINT, &act, &oact) < 0)
		tcp_printf(DIE, "can not set signal hander interupt");

	return;
}

void
tcp_set_signal_alarm(void) {
	struct sigaction act, oact;

	act.sa_handler = (void (*)())tcp_sigsend;

	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;

	if (sigaction(SIGALRM, &act, &oact) < 0)
		tcp_printf(DIE, "can not set signal hander alarm");

	return;
}

void
tcp_set_signals() {
	tcp_set_signal_interupt();
	tcp_set_signal_alarm();
	return;
}


/*----------------------------------------------------------------------------
 * main
 *----------------------------------------------------------------------------
*/
int
main(int argc, char **argv) {

	tcp_opt(argc, argv);

	tcp_set_signals();
	alarm(1); /* send first packet */

	do {
		/*
		 * The following select() is a dummy, because that sending
		 * packet function tcp_send() has to be called by signal
		 * alarm (SIGALRM).
		*/
		(void) select((STDIN_FILENO + 1), NULL, NULL, NULL, NULL);
	} while (opt->loop > 0);

	tcp_print_result();
	/* NOTREACHED */

	return (0);
}

/* end of source */
