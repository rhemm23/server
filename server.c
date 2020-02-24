#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include "util.h"

#include "server.h"

/**
 * Opens a new socket
 */
static int create_socket(uint16_t port, int backlog) {
	int socket_fd, enable = 1;
	if((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		die("socket");
	}
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
	return socket_fd;
}

server_t *start_server(config_t *config) {
	server_t *server = (server_t*)scalloc(1, sizeof(server_t));
	server->socket_fd = create_socket(config->port, config->backlog);
}
