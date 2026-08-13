#include "loopback.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

static bool send_request(uint16_t port, const char *path) {
  int s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) return false;
  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(port);
  if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    close(s);
    return false;
  }
  char req[1024];
  snprintf(req, sizeof(req), "GET %s HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n",
           path);
  ssize_t w = write(s, req, strlen(req));
  close(s);
  return w > 0;
}

int main(void) {
  const char *state = "xyz123";
  uint16_t port = 0;
  int fd = -1;
  if (!loopback_listen(&port, &fd)) {
    fprintf(stderr, "listen failed\n");
    return 1;
  }

  pid_t pid = fork();
  if (pid < 0) {
    close(fd);
    return 1;
  }
  if (pid == 0) {
    usleep(50 * 1000);
    send_request(port, "/callback?code=ABCdef&state=xyz123");
    _exit(0);
  }

  loopback_result r = {0};
  if (!loopback_accept_once(fd, state, 8, &r)) {
    fprintf(stderr, "accept failed\n");
    close(fd);
    return 1;
  }
  close(fd);
  int status = 0;
  waitpid(pid, &status, 0);

  if (!r.ok || r.code == nullptr || strcmp(r.code, "ABCdef") != 0) {
    fprintf(stderr, "wrong code: %s\n", r.code ? r.code : "(null)");
    return 1;
  }
  loopback_result_free(&r);
  printf("loopback tests ok\n");
  return 0;
}
