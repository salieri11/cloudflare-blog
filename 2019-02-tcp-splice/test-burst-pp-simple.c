#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "common.h"

const int BUF_LEN = 262144;
const int NUM_OF_BUFFERS = 1024;

int main(int argc, char **argv)
{
	if (argc < 4) {
		FATAL("Usage: %s <target:port> <burst size IN BYTES> <number of bursts>",
		      argv[0]);
	}

	char * memory = malloc(NUM_OF_BUFFERS*BUF_LEN);
	memset(memory, 'a', BUF_LEN * NUM_OF_BUFFERS);

	struct sockaddr_storage target;
	net_parse_sockaddr(&target, argv[1]);

	uint64_t burst_sz=0;
	if (argc > 2) {
		char *endptr;
		double ts = strtod(argv[2], &endptr);
		if (ts < 0 || *endptr != '\0') {
			FATAL("Can't parse number %s", argv[2]);
		}
		burst_sz = ts;
	}

	long burst_count = 1;
	if (argc > 3) {
		char *endptr;
		burst_count = strtol(argv[3], &endptr, 10);
		if (burst_count < 0 || *endptr != '\0') {
			FATAL("Can't parse number %s", argv[2]);
		}
	}

	fprintf(stderr, "[+] Sending %ld blocks of %lu to %s\n", burst_count, burst_sz , net_ntop(&target));

	int fd = net_connect_tcp_blocking(&target, 1);
	if (fd < 0) {
		PFATAL("connect()");
	}

	sleep(1);
	printf ("[+] connected\n");

	/*
	int val = 10 * 1000; // 10 ms, in us. requires CAP_NET_ADMIN
	int r = setsockopt(fd, SOL_SOCKET, SO_BUSY_POLL, &val, sizeof(val));
	if (r < 0) {
		if (errno == EPERM) {
			fprintf(stderr,
				"[ ] Failed to set SO_BUSY_POLL. Are you "
				"CAP_NET_ADMIN?\n");
		} else {
			PFATAL("setsockopt(SOL_SOCKET, SO_BUSY_POLL)");
		}
	}
	*/

	/* Attempt to set large TX and RX buffer. Why not. */
	uint64_t total_t0 = realtime_now();

	int burst_i;
	char* tx_idx = memory;
	int mem_end = BUF_LEN * NUM_OF_BUFFERS;
	for (burst_i = 0; burst_i < burst_count; burst_i += 1, tx_idx += burst_sz) {
		uint64_t t0 = realtime_now();
		if(tx_idx > memory + mem_end)
			tx_idx = memory;

		int tx_bytes = burst_sz;
		while (tx_bytes) {
			if (tx_bytes) {
				int n = send(fd, tx_idx, tx_bytes,0);
				if (n < 0) {
					if (errno == EINTR) {
						continue;
					}
					if (errno == ECONNRESET) {
						fprintf(stderr,
							"[!] ECONNRESET\n");
						break;
					}
					if (errno == EPIPE) {
						fprintf(stderr, "[!] EPIPE\n");
						break;
					}
					if (errno == EAGAIN) {
						// pass
					} else {
						PFATAL("send()");
					}
				}
				if (n == 0) {
					PFATAL("?");
				}
				if (n > 0) {
					tx_bytes -= n;
					// printf("sent: %d, left: %d\n", n, tx_bytes);
				}
			}
		}
    int rx_bytes = burst_sz;
		while (rx_bytes) {
			if (rx_bytes) {
				int n = recv(fd, tx_idx, rx_bytes,0);
				if (n < 0) {
					if (errno == EINTR) {
						continue;
					}
					if (errno == ECONNRESET) {
						fprintf(stderr,
							"[!] ECONNRESET\n");
						break;
					}
					if (errno == EPIPE) {
						fprintf(stderr, "[!] EPIPE\n");
						break;
					}
					if (errno == EAGAIN) {
						// pass
					} else {
						PFATAL("send()");
					}
				}
				if (n == 0) {
					PFATAL("?");
				}
				if (n > 0) {
					rx_bytes -= n;
					// printf("sent: %d, left: %d\n", n, tx_bytes);
				}
			}
		}
		tx_idx += burst_sz;
		uint64_t t1 = realtime_now();
		// printf("%ld\n", (t1 - t0) / 1000);
	}
	close(fd);

	uint64_t total_t1 = realtime_now();

	fprintf(stderr, "[+] Wrote %ld bursts of %lu bytes in %.1fms\n",
		burst_count, burst_sz, (total_t1 - total_t0) / 1000000.);
	free (memory);
	return 0;
}
