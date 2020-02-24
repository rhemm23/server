#include "server.h"

int main() {
  config_t config;
  config.port = 7070;
  config.backlog = 10;
  config.workers = 4;

  server_t *server = start_server(&config);
  while(1) { }
}
