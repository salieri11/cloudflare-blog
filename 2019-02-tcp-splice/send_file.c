/*
 * sendfile.c: simple example of using sendfile(3EXT) interface
 */

#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/sendfile.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "common.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

int
main(int argc, char **argv)
{
	if (argc !=  5) {
		fprintf(stderr, "usage: %s <from file> <to endpoint> <file len> <iterations>\n", argv[0]);
		return (1);
	}

	const char *fromfile = argv[1];
	long len = atol(argv[3]);
	long it = atol(argv[4]);

	int fromfd, tofd;
	
	errno = 0;
	if ((fromfd = open(fromfile, O_RDONLY)) < 0) {
		perror("open");
		return (1);
	}

    struct sockaddr_storage target;
	net_parse_sockaddr(&target, argv[2]);
    tofd = net_connect_tcp_blocking(&target, 0);
	if (tofd < 0) {
		PFATAL("connect()");
	}

	// printf("neeed to send %lu bytes %lu times", len, it);
	int total=0;
	for(int i=0;i < it; i++) {
		int rv=0;
		int sent=0;
		off_t off = 0;
		do {
			if ((rv = sendfile(tofd, fromfd, &off, len-sent)) < 0) {
				fprintf(stderr, "Warning: sendfile returned %d errno %d)\n", rv, errno);
				PFATAL("?");
			}
			sent+=rv;
			// printf("off: %lu, rv: %d, sent: %d \n", off, rv, sent);
		} while(sent < len);
		total+=sent;
		// printf("total: %d \n", total);
	}

	// printf("Sent %d bytes over sendfile of %lu requested \n", total, len*it);

	close(fromfd);
	close(tofd);

	return (0);
}