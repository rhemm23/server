#ifndef SERVER_H
#define SERVER_H

#include <inttypes.h>

typedef struct config {
	uint16_t port;
	int backlog;
} config_t;

typedef struct server {
	int socket_fd;
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
