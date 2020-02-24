#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>

#include "util.h"

#include "server.h"

#define WORKER_FDS 1024

/**
 * Worker loop to handle client states
 */
static void *worker_loop(void *state) {
	worker_t *worker = (worker_t*)state;
	while(!worker->stop) {
		int nready = epoll_wait(worker->epoll_fd, worker->events, WORKER_FDS, -1);
		for(int i = 0; i < nready; i++) {
			if(worker->events[i].events & EPOLLERR) {
				die("epoll_wait");
			} else {
				// TODO, handle client input/output
			}
		}
	}
}

/**
 * Creates a new worker to handle clients
 */
static worker_t *start_worker() {
	worker_t *worker = (worker_t*)scalloc(1, sizeof(worker_t));
	worker->events = (struct epoll_event*)scalloc(WORKER_FDS, sizeof(struct epoll_event));
	
	if((worker->epoll_fd = epoll_create1(0)) < 0) {
		die("epoll_create1");
	}
	if(pthread_create(&worker->handler_thread, NULL, worker_loop, (void*)worker) != 0) {
		die("pthread_create");
	}
	return worker;
}

/**
 * Makes a socket non-blocking
 */
static void non_blocking(int socket_fd) {
	int flags;
	if((flags = fcntl(socket_fd, F_GETFL, 0)) < 0) {
		die("fcntl");
	}
	if(fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
		die("fcntl");
	}
}

/**
 * Opens a new socket
 */
static int create_socket(uint16_t port, int backlog) {
	// Create socket
	int socket_fd, enable = 1;
	if((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		die("socket");
	}

	// Bind and listen
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if(bind(socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		die("bind");
	}
	if(setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
		die("setsockopt");
	}
	if(listen(socket_fd, backlog) < 0) {
		die("listen");
	}

	// Set non-blocking
	non_blocking(socket_fd);
	return socket_fd;
}

static void *server_loop(void *state) {
	server_t *server = (server_t*)state;
	while(!server->stop) {
		int nready = epoll_wait(server->epoll_fd, server->events, 1, -1);
		for(int i = 0; i < nready; i++) {
			if(server->events[i].events & EPOLLERR) {
				die("epoll_wait");
			} else {
				struct sockaddr_in peer_addr;
				socklen_t peer_addr_len = sizeof(peer_addr);
				int peer_fd = accept(server->socket_fd, (struct sockaddr*)&peer_addr, &peer_addr_len);
				if(peer_fd < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
					die("accept");
				} else {
					non_blocking(peer_fd);

					// Setup client event
					struct epoll_event event;
					event.data.fd = peer_fd;
					event.events |= EPOLLIN | EPOLLOUT;

					// Add peer to worker epoll
					int worker_fd = server->workers[server->worker_cycle]->epoll_fd;
					if(epoll_ctl(worker_fd, EPOLL_CTL_ADD, peer_fd, &event) < 0) {
						die("epoll_ctl");
					}
					
					// Update cycle
					server->worker_cycle = (server->worker_cycle + 1) % server->worker_count;
				}
			}
		}
	}
}

/**
 * Creates and starts a new server
 */
server_t *start_server(config_t *config) {
	server_t *server = (server_t*)scalloc(1, sizeof(server_t));
	server->socket_fd = create_socket(config->port, config->backlog);
	server->events = (struct epoll_event*)scalloc(1, sizeof(struct epoll_event));

	if((server->epoll_fd = epoll_create1(0)) < 0) {
		die("epoll_create1");
	}

	struct epoll_event accept;
	accept.data.fd = server->socket_fd;
	accept.events = EPOLLIN;
	if(epoll_ctl(server->epoll_fd, EPOLL_CTL_ADD, server->socket_fd, &accept) < 0) {
		die("epoll_ctl");
	}

	// Setup workers
	server->worker_cycle = 0;
	server->worker_count = config->workers;
	server->workers = (worker_t**)scalloc(server->worker_count, sizeof(worker_t*));
	for(int i = 0; i < server->worker_count; i++) {
		server->workers[i] = start_worker();
	}

	// Start listener thread
	if(pthread_create(&server->listener_thread, NULL, server_loop, (void*)server) != 0) {
		die("pthread_create");
	}
}
