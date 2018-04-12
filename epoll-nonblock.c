/* $begin select */
#define MAXEVENTS 64

#include "csapp.h"

#include<errno.h>
#include<fcntl.h>
#include<stdlib.h>
#include<stdio.h>
#include<sys/epoll.h>
#include<sys/socket.h>
#include<string.h>

void command(void);

int main(int argc, char **argv) 
{
	int listenfd, connfd;
	socklen_t clientlen;
	struct sockaddr_storage clientaddr;
	int efd;
	struct epoll_event event;
	struct epoll_event *events;
	int i;
	int len;

	size_t n; 
	char buf[MAXLINE]; 

	if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(0);
	}

	listenfd = Open_listenfd(argv[1]);

	// set fd to non-blocking (set flags while keeping existing flags)
	if (fcntl(listenfd, F_SETFL, fcntl(listenfd, F_GETFL, 0) | O_NONBLOCK) < 0) {
		fprintf(stderr, "error setting socket option\n");
		exit(1);
	}

	if ((efd = epoll_create1(0)) < 0) {
		fprintf(stderr, "error creating epoll fd\n");
		exit(1);
	}

	event.data.fd = listenfd;
	event.events = EPOLLIN | EPOLLET; // use edge-triggered monitoring
	if (epoll_ctl(efd, EPOLL_CTL_ADD, listenfd, &event) < 0) {
		fprintf(stderr, "error adding event\n");
		exit(1);
	}

	/* Buffer where events are returned */
	events = calloc(MAXEVENTS, sizeof(event));

	while (1) {
		// wait for event to happen (no timeout)
		n = epoll_wait(efd, events, MAXEVENTS, -1);

		for (i = 0; i < n; i++) {
			if ((events[i].events & EPOLLERR) ||
					(events[i].events & EPOLLHUP) ||
					(events[i].events & EPOLLRDHUP)) {
				/* An error has occured on this fd */
				fprintf (stderr, "epoll error on fd %d\n", events[i].data.fd);
				close(events[i].data.fd);
				continue;
			}

			if (listenfd == events[i].data.fd) { //line:conc:select:listenfdready
				clientlen = sizeof(struct sockaddr_storage); 

				// loop and get all the connections that are available
				while ((connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen)) > 0) {

					// set fd to non-blocking (set flags while keeping existing flags)
					if (fcntl(connfd, F_SETFL, fcntl(connfd, F_GETFL, 0) | O_NONBLOCK) < 0) {
						fprintf(stderr, "error setting socket option\n");
						exit(1);
					}

					// add event to epoll file descriptor
					event.data.fd = connfd;
					event.events = EPOLLIN | EPOLLET; // use edge-triggered monitoring
					if (epoll_ctl(efd, EPOLL_CTL_ADD, connfd, &event) < 0) {
						fprintf(stderr, "error adding event\n");
						exit(1);
					}
				}

				if (errno == EWOULDBLOCK || errno == EAGAIN) {
					// no more clients to accept()
				} else {
					perror("error accepting");
				}

			} else { //line:conc:select:listenfdready
				while ((len = recv(events[i].data.fd, buf, MAXLINE, 0)) > 0) {
					printf("Received %d bytes\n", len);
					send(events[i].data.fd, buf, len, 0);
				}
				if (len == 0) {
					// EOF received.
					// Closing the fd will automatically unregister the fd
					// from the efd
					close(events[i].data.fd);
				} else if (errno == EWOULDBLOCK || errno == EAGAIN) {
					// no more data to read()
				} else {
					perror("error reading");
				}
			}
		}
	}
	free(events);
}

/* $end select */
