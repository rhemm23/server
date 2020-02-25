#include <signal.h>
#include <stdlib.h>
#include <stdio.h>

#include "server.h"

server_t *server;

void handle_interrupt() {
  printf("Stopping server...\n");
  stop_server(server);
  exit(0);
}

int main() {

  signal(SIGINT, handle_interrupt);

  config_t config;
  config.port = 7270;
  config.backlog = 10;
  config.workers = 4;

  printf("Starting server on port 7270...\n");
  server = start_server(&config);
  printf("Server started...\n");
  if(server == NULL) {
    printf("server is null...\n");
  }
  while(1) { }
}
