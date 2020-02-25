#ifndef SERVER_H
#define SERVER_H

#include <inttypes.h>
#include <pthread.h>

typedef struct config {
  uint16_t port;
  int max_conns;
  int backlog;
  int workers;
} config_t;

typedef struct worker {
  struct epoll_event *events;
  pthread_t handler_thread;
  struct server *parent;
  volatile int stop;
  int epoll_fd;
  int wake_fd;
} worker_t;

typedef enum {
  RECEIVING,
  RESPONDING
} peer_state_t;

typedef struct peer {
  peer_state_t state;
  uint8_t *recv_buf;
  uint8_t *send_buf;
  size_t recv_size;
  size_t send_size;
  size_t recv_cnt;
  size_t send_cnt;
} peer_t;

typedef struct server {
  struct epoll_event *events;
  pthread_t listener_thread;
  worker_t **workers;
  peer_t **peers;
  volatile int stop;
  int worker_count;
  int worker_cycle;
  int peer_count;
  int socket_fd;
  int epoll_fd;
  int wake_fd;
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
