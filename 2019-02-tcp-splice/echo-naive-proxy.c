#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include "common.h"

typedef struct _thread_data_t {
  int tid;
  uint64_t sum;
  int cd;
  int target_fd;
} thread_data;

void *return_data(void *arg) {
	printf("thread start\n");
	thread_data* data = (thread_data*)arg;
	char buf[BUFFER_SIZE];
	while(1) {
	int k = recv(data->target_fd, buf, sizeof(buf), 0);
		if (k < 0) {
			if (errno == EINTR) {
				fprintf(stderr, "[!] EINTR1\n");
				continue;
			}
			if (errno == ECONNRESET) {
				fprintf(stderr, "[!] 1ECONNRESET\n");
				break;
			}
		}

		if (k == 0) {
			/* On TCP socket zero means EOF */
			fprintf(stderr, "[-] edge side EOF1\n");
			break;
		}

		data->sum += k;

		int l = send(data->cd, buf, k, 0);
		if (l < 0) {
			if (errno == EINTR) {
				fprintf(stderr, "[-] edge side EOF122\n");
				continue;
			}
			if (errno == ECONNRESET) {
				fprintf(stderr, "[!] 2ECONNRESET on origin\n");
				break;
			}
			if (errno == EPIPE) {
				fprintf(stderr, "[!] EPIPE on origin\n");
				break;
			}
			printf("err: %d" ,errno);
			// PFATAL("send()");
		}
		if (l == 0) {
			break;
		}
		if (l != k) {
			int err;
			socklen_t err_len = sizeof(err);
			int r = getsockopt(data->cd, SOL_SOCKET, SO_ERROR, &err,
					   &err_len);
			if (r < 0) {
				PFATAL("getsockopt()");
			}
			errno = err;
			if (errno == EPIPE || errno == ECONNRESET) {
				break;
			}
			// PFATAL("send()");
		}
	}

	printf("thread end\n");
	return 0;
}

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

	char *endptr;
	double ts = strtod(argv[2], &endptr);

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

	printf("[+] accepted\n");

	struct sockaddr_storage target;
	net_parse_sockaddr(&target, argv[2]);
	int target_fd = net_connect_tcp_blocking(&target, 1);
	if (target_fd < 0) {
		PFATAL("connect()");
	}

	printf("[+] connected\n");

	uint64_t t0 = realtime_now();

	char buf[BUFFER_SIZE];

	pthread_t thr;
	thread_data td;
	td.sum = 0;
	td.target_fd = target_fd;
	td.cd = cd;
	int rc = 0;
	if ((rc = pthread_create(&thr, NULL, return_data, &td))) {
      fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
      return EXIT_FAILURE;
	}

	uint64_t forwarded_sum = 0;
	uint64_t return_back_sum = 0;
	while (1) {
		int n = recv(cd, buf, BUFFER_SIZE, 0);
		if (n < 0) {
			if (errno == EINTR) {
				continue;
			}
			if (errno == ECONNRESET) {
				fprintf(stderr, "[!] 3ECONNRESET\n");
				break;
			}
			// PFATAL("read()");
		}

		if (n == 0) {
			/* On TCP socket zero means EOF */
			fprintf(stderr, "[-] edge side EOF2\n");
			break;
		}

		forwarded_sum += n;

		int m = send(target_fd, buf, n, 0);
		if (m < 0) {
			if (errno == EINTR) {
				continue;
			}
			if (errno == ECONNRESET) {
				fprintf(stderr, "[!] 4ECONNRESET on origin\n");
				break;
			}
			if (errno == EPIPE) {
				fprintf(stderr, "[!] EPIPE on origin\n");
				break;
			}
			// PFATAL("send()");
		}
		if (m == 0) {
			break;
		}
		if (m != n) {
			int err;
			socklen_t err_len = sizeof(err);
			int r = getsockopt(cd, SOL_SOCKET, SO_ERROR, &err,
					   &err_len);
			if (r < 0) {
				PFATAL("getsockopt()");
			}
			errno = err;
			if (errno == EPIPE || errno == ECONNRESET) {
				break;
			}
			// PFATAL("send()");
		}
	}


	close(cd);
	close(target_fd);
	pthread_cancel(thr);
	printf("joining\n");
	pthread_join(thr, NULL);
	printf("joined\n");
	
	uint64_t t1 = realtime_now();

	fprintf(stderr, "[+] forwared %lu in %.1fms\n", forwarded_sum, (1 - t0) / 1000000.);
	fprintf(stderr, "[+] returned %lu in %.1fms\n", td.sum, (1 - t0) / 1000000.);
	goto again_accept;

	return 0;
}
