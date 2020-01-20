#define _GNU_SOURCE /* splice */

#include <errno.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "common.h"

int main(int argc, char **argv)
{
	if (argc < 3) {
		FATAL("Usage: %s <listen:port>", argv[0]);
	}

	struct sockaddr_storage listen;
	net_parse_sockaddr(&listen, argv[1]);

	int busy_poll = 0;
	if (argc > 3) {
		busy_poll = 1;
	}

	fprintf(stderr, "[+] Accepting on %s busy_poll=%d\n", net_ntop(&listen),
		busy_poll);

	int sd = net_bind_tcp(&listen);
	if (sd < 0) {
		PFATAL("connect()");
	}

again_accept:;
	struct sockaddr_storage client;
	int cd = net_accept(sd, &client);

	if (busy_poll) {
		int val = 10 * 1000; // 10 ms, in us. requires CAP_NET_ADMIN
		int r = setsockopt(cd, SOL_SOCKET, SO_BUSY_POLL, &val,
				   sizeof(val));
		if (r < 0) {
			if (errno == EPERM) {
				fprintf(stderr,
					"[ ] Failed to set SO_BUSY_POLL. Are "
					"you "
					"CAP_NET_ADMIN?\n");
			} else {
				PFATAL("setsockopt(SOL_SOCKET, SO_BUSY_POLL)");
			}
		}
	}

	uint64_t t0 = realtime_now();
	
	/* There is no "good" splice buffer size. Anecdotical evidence
	 * says that it should be no larger than 512KiB since this is
	 * the max we can expect realistically to fit into cpu
	 * cache. */
#define SPLICE_MAX (512*1024)
	
	struct sockaddr_storage target;
	net_parse_sockaddr(&target, argv[2]);
	int target_fd = net_connect_tcp_blocking(&target, 1);
	if (target_fd < 0) {
		PFATAL("connect()");
	}

	sleep(1);

	int pfd[2];
	int r = pipe(pfd);
	if (r < 0) {
		PFATAL("pipe()");
	}

	r = fcntl(pfd[0], F_SETPIPE_SZ, SPLICE_MAX);
	if (r < 0) {
		PFATAL("fcntl()");
	}	

	uint64_t forwared_sum = 0, received_back_sum = 0;
	while (1) {
		/* This is fairly unfair. We are doing 512KiB buffer
		 * in one go, as opposed to naive approaches. Cheating. */
		int n = splice(cd, NULL, pfd[1], NULL, SPLICE_MAX, SPLICE_F_MOVE);
		// printf("n == %d\n", n);
		if (n < 0) {
			printf("error == %d\n", errno);
			if (errno == ECONNRESET) {
				fprintf(stderr, "[!] ECONNRESET\n");
				break;
			}
			if (errno == EAGAIN) {
				break;
			}
			PFATAL("splice()");
		}
		if (n == 0) {
			/* On TCP socket zero means EOF */
			fprintf(stderr, "[-] edge side EOF\n");
			break;
		}

		forwared_sum += n;

		int m = splice(pfd[0], NULL, target_fd, NULL, n, SPLICE_F_MOVE);
		// printf("m == %d\n", m);
		if (m < 0) {
			if (errno == ECONNRESET) {
				fprintf(stderr, "[!] ECONNRESET on origin\n");
				break;
			}
			if (errno == EPIPE) {
				fprintf(stderr, "[!] EPIPE on origin\n");
				break;
			}
			PFATAL("send()");
		}
		if (m == 0) {
			int err;
			socklen_t err_len = sizeof(err);
			int r = getsockopt(cd, SOL_SOCKET, SO_ERROR, &err,
					   &err_len);
			if (r < 0) {
				PFATAL("getsockopt()");
			}
			errno = err;
			PFATAL("send()");
		}
		if (m != n) {
			FATAL("expecting splice to block2");
		}

		int k = splice(target_fd, NULL, pfd[1], NULL, SPLICE_MAX, SPLICE_F_MOVE);
		// printf("m == %d\n", m);
		if (k < 0) {
			if (errno == ECONNRESET) {
				fprintf(stderr, "[!] ECONNRESET on origin\n");
				break;
			}
			if (errno == EPIPE) {
				fprintf(stderr, "[!] EPIPE on origin\n");
				break;
			}
			PFATAL("send()");
		}
		if (k == 0) {
			int err;
			socklen_t err_len = sizeof(err);
			int r = getsockopt(cd, SOL_SOCKET, SO_ERROR, &err,
					   &err_len);
			if (r < 0) {
				PFATAL("getsockopt()");
			}
			errno = err;
			PFATAL("send()");
		}

		received_back_sum += k;

		int l = splice(pfd[0], NULL, cd, NULL, k, SPLICE_F_MOVE);
		// printf("m == %d\n", m);
		if (l < 0) {
			if (errno == ECONNRESET) {
				fprintf(stderr, "[!] ECONNRESET on origin\n");
				break;
			}
			if (errno == EPIPE) {
				fprintf(stderr, "[!] EPIPE on origin\n");
				break;
			}
			PFATAL("send()");
		}
		if (l == 0) {
			int err;
			socklen_t err_len = sizeof(err);
			int r = getsockopt(cd, SOL_SOCKET, SO_ERROR, &err,
					   &err_len);
			if (r < 0) {
				PFATAL("getsockopt()");
			}
			errno = err;
			PFATAL("send()");
		}
		if (l != k) {
			FATAL("expecting splice to block1");
		}
	}

	close(cd);
	close(target_fd);
	uint64_t t1 = realtime_now();

	fprintf(stderr, "[+] Forwarded %lu in %.1fms\n", forwared_sum, (t1 - t0) / 1000000.);
	fprintf(stderr, "[+] Returned %lu in %.1fms\n", received_back_sum, (t1 - t0) / 1000000.);
	goto again_accept;

	return 0;
}
