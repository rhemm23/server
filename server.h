#ifndef SERVER_H
#define SERVER_H

#include <inttypes.h>

typedef struct config {
	uint16_t port;
	int backlog;
	int workers;
} config_t;

typedef struct worker {
	struct epoll_event *events;
	pthread_t handler_thread;
	volatile int stop;
	int epoll_fd;
} worker_t;

typedef struct server {
	struct epoll_event *events;
	pthread_t listener_thread;
	volatile int stop;
	worker_t **workers;
	int worker_count;
	int worker_cycle;
	int socket_fd;
	int epoll_fd;
} server_t;


/**
 * Creates and starts a new server
 */
server_t *start_server(config_t *config);

/**
 * Stops a running server
 */
void stop_server(server_t *server);

#endif
