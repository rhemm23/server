#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "util.h"

#include "server.h"

#define WORKER_FDS 1024
#define PACKET_SIZE 8192

/**
 * Sets the send buffer using the received buffer
 */
static void process_peer(peer_t *peer) {
  static char *msg = "hi!";
  peer->send_buf = (uint8_t*)msg;
  peer->send_size = 4;
}

/**
 * Disconnect a peer
 */
static void disconnect_peer(worker_t *worker, int peer_fd) {
  worker->parent->peers[peer_fd] = NULL;
  if(epoll_ctl(worker->epoll_fd, EPOLL_CTL_DEL, peer_fd, NULL) < 0) {
    die("epoll_ctl");
  }
  close(peer_fd);
}

/**
 * Worker loop to handle client states
 */
static void *worker_loop(void *state) {
  worker_t *worker = (worker_t*)state;
  while(!worker->stop) {
    int nready = epoll_wait(worker->epoll_fd, worker->events, WORKER_FDS, -1);
    for(int i = 0; i < nready; i++) {
      struct epoll_event cevent = worker->events[i];
      peer_t *peer = worker->parent->peers[cevent.data.fd];
      if(cevent.events & EPOLLERR) {
        die("epoll_wait");
      } else if((cevent.events & EPOLLIN) && (peer->state == RECEIVING)) {
        uint8_t *buffer = (uint8_t*)smalloc(PACKET_SIZE);
        int nbytes = recv(worker->events[i].data.fd, buffer, PACKET_SIZE, 0);
        if(nbytes == 0) {
          disconnect_peer(worker, cevent.data.fd);
        } else if (nbytes < 0) {
          if(errno != EAGAIN && errno != EWOULDBLOCK) {
            die("recv");
          }
        } else {
          int copy_bytes;
          if(peer->recv_size == 0) {
            // First two bytes are size
            peer->recv_size = (size_t)(buffer[0] << 8 | buffer[1]);
            peer->recv_buf = (uint8_t*)smalloc(peer->recv_size);

            // Copy from buffer
            copy_bytes = nbytes - 2 > peer->recv_size ? peer->recv_size : nbytes - 2;
            memcpy(peer->recv_buf, &buffer[2], copy_bytes);
          } else {
            // Copy new bytes
            int rem_bytes = peer->recv_size - peer->recv_cnt;
            copy_bytes = nbytes > rem_bytes ? rem_bytes : nbytes;
            memcpy(&peer->recv_buf[peer->recv_cnt], buffer, copy_bytes);
          }
          // Update peer state
          peer->recv_cnt += copy_bytes;
          if(peer->recv_cnt == peer->recv_size) {
            process_peer(peer);
            peer->state = RESPONDING;
            
            // Set event to write
            struct epoll_event newevent;
            newevent.data.fd = cevent.data.fd;
            newevent.events |= EPOLLOUT;

            // Modify peer fd epoll
            if(epoll_ctl(worker->epoll_fd, EPOLL_CTL_MOD, cevent.data.fd, &newevent) < 0) {
              die("epoll_ctl");
            }
          }
        }
        free(buffer);
      } else if((cevent.events & EPOLLOUT) && (peer->state == RESPONDING)) {
        int sendlen = peer->send_size - peer->send_cnt;
        int nsent = send(cevent.data.fd, &peer->send_buf[peer->send_cnt], sendlen, 0);
        if(nsent == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
          die("send");
        }
        if(nsent < sendlen) {
          peer->send_cnt += nsent;
        } else {
          disconnect_peer(worker, cevent.data.fd);
        }
      }
    }
  }
}

/**
 * Creates a new worker to handle clients
 */
static worker_t *start_worker(server_t *server) {
  worker_t *worker = (worker_t*)scalloc(1, sizeof(worker_t));
  worker->events = (struct epoll_event*)scalloc(WORKER_FDS, sizeof(struct epoll_event));
  worker->parent = server;

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
          event.events |= EPOLLIN;

          // Setup peer state
          peer_t *peer = (peer_t*)scalloc(1, sizeof(peer_t));
          peer->state = RECEIVING;
          server->peers[peer_fd] = peer;

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

  // Setup peer states
  server->peer_count = config->max_conns;
  server->peers = (peer_t**)scalloc(server->peer_count, sizeof(peer_t*));

  // Setup workers
  server->worker_cycle = 0;
  server->worker_count = config->workers;
  server->workers = (worker_t**)scalloc(server->worker_count, sizeof(worker_t*));
  for(int i = 0; i < server->worker_count; i++) {
    server->workers[i] = start_worker(server);
  }

  // Start listener thread
  if(pthread_create(&server->listener_thread, NULL, server_loop, (void*)server) != 0) {
    die("pthread_create");
  }
  return server;
}

void stop_server(server_t *server) {
  // Join server thread
  server->stop = 1;
  pthread_join(server->listener_thread, NULL);

  // Stop workers
  for(int i = 0; i < server->worker_count; i++) {
    worker_t *worker = server->workers[i];
    worker->stop = 1;
    pthread_join(worker->handler_thread, NULL);
    
    // Free worker resources
    close(worker->epoll_fd);
    free(worker->events);
    free(worker);
  }

  // Free peers
  for(int i = 0; i < server->peer_count; i++) {
    free(server->peers[i]);
  }

  // Free server resources
  close(server->epoll_fd);
  close(server->socket_fd);
  free(server->events);
  free(server->peers);
  free(server);
}
