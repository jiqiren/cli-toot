#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  char *code;
  char *error;
  bool ok;
} loopback_result;

[[nodiscard]] bool loopback_listen(uint16_t *port_out, int *fd_out);

[[nodiscard]] bool loopback_accept_once(int fd, const char *state,
                                         int timeout_seconds,
                                         loopback_result *out);

[[nodiscard]] char *loopback_redirect_uri(uint16_t port);

void loopback_result_free(loopback_result *r);
