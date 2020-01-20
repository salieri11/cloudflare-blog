#include <arpa/inet.h>
#include <errno.h>
#include <linux/aio_abi.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include "common.h"
#include "iosubmit.h"

typedef struct _thread_data_t {
  int tid;
  uint64_t sum;
  int cd;
  int target_fd;
  
} thread_data;

void *return_data(void *arg) {
	aio_context_t ctx = {0};
	int r = io_setup(8, &ctx);
	if (r < 0) {
		PFATAL("io_setup(1)");
	}

	thread_data* data = (thread_data*)arg;
	char buf[BUFFER_SIZE];
	while(1) {
		char buf[BUFFER_SIZE];

		struct iocb cb[2] = {{.aio_fildes = data->cd,
					.aio_lio_opcode = IOCB_CMD_PWRITE,
					.aio_buf = (uint64_t)buf,
					.aio_nbytes = 0},
					{.aio_fildes = data->target_fd,
					.aio_lio_opcode = IOCB_CMD_PREAD,
					.aio_buf = (uint64_t)buf,
					.aio_nbytes = BUFFER_SIZE}};
		struct iocb *list_of_iocb[2] = {&cb[0], &cb[1]};

		r = io_submit(ctx, 2, list_of_iocb);
		if (r != 2) {
			PFATAL("io_submit()");
		}

		/* We must pick up the result, since we need to get
		 * the number of bytes read. */
		struct io_event events[2] = {{0}};
		r = io_getevents(ctx, 1, 2, events, NULL);
		if (r < 0) {
			PFATAL("io_getevents()");
		}
		if (events[0].res < 0) {
			errno = -events[0].res;
			perror("io_submit(IOCB_CMD_PWRITE)");
			break;
		}
		if (events[1].res < 0) {
			errno = -events[1].res;
			perror("io_submit(IOCB_CMD_PREAD)");
			break;
		}
		if (events[1].res == 0) {
			fprintf(stderr, "[-] edge side EOF\n");
			break;
		}
		cb[0].aio_nbytes = events[1].res;
		data->sum += events[1].res;
		printf("data: %lu", data->sum);
	}
r:
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

	fprintf(stderr, "[+] Accepting on %s busy_poll=%d\n", net_ntop(&listen),
		busy_poll);

	int sd = net_bind_tcp(&listen);
	if (sd < 0) {
		PFATAL("connect()");
	}

	aio_context_t ctx = {0};
	int r = io_setup(8, &ctx);
	if (r < 0) {
		PFATAL("io_setup()");
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

	struct sockaddr_storage target;
	net_parse_sockaddr(&target, argv[2]);
	int target_fd = net_connect_tcp_blocking(&target, 1);
	if (target_fd < 0) {
		PFATAL("connect()");
	}

	uint64_t t0 = realtime_now();

	char buf[BUFFER_SIZE];

	struct iocb cb[2] = {{.aio_fildes = target_fd,
			      .aio_lio_opcode = IOCB_CMD_PWRITE,
			      .aio_buf = (uint64_t)buf,
			      .aio_nbytes = 0},
			     {.aio_fildes = cd,
			      .aio_lio_opcode = IOCB_CMD_PREAD,
			      .aio_buf = (uint64_t)buf,
			      .aio_nbytes = BUFFER_SIZE}};
	struct iocb *list_of_iocb[2] = {&cb[0], &cb[1]};

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

	uint64_t sum = 0;
	while (1) {
		// io_submit on blocking network sockets will
		// block. It will loop over sockets one by one,
		// blocking on each operation. We abuse this to do
		// write+read in one syscall. In first iteration the
		// write is empty, we do write of size 0.
		r = io_submit(ctx, 2, list_of_iocb);
		if (r != 2) {
			PFATAL("io_submit()");
		}

		/* We must pick up the result, since we need to get
		 * the number of bytes read. */
		struct io_event events[2] = {{0}};
		r = io_getevents(ctx, 1, 2, events, NULL);
		if (r < 0) {
			PFATAL("io_getevents()");
		}
		if (events[0].res < 0) {
			errno = -events[0].res;
			perror("io_submit(IOCB_CMD_PWRITE)");
			break;
		}
		if (events[1].res < 0) {
			errno = -events[1].res;
			perror("io_submit(IOCB_CMD_PREAD)");
			break;
		}
		if (events[1].res == 0) {
			fprintf(stderr, "[-] edge side EOF\n");
			break;
		}
		cb[0].aio_nbytes = events[1].res;
		sum += events[1].res;
	}

	pthread_join(thr, NULL);

	close(cd);
	close(target_fd);

	uint64_t t1 = realtime_now();

	fprintf(stderr, "[+] forwarded %lu in %.1fms\n", sum, (t1 - t0) / 1000000.);
	fprintf(stderr, "[+] returned %lu in %.1fms\n", td.sum, (t1 - t0) / 1000000.);
	goto again_accept;

	return 0;
}
