#include "loopback.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

static char *url_decode(const char *s, size_t n) {
  char *out = malloc(n + 1);
  if (out == nullptr) return nullptr;
  size_t j = 0;
  for (size_t i = 0; i < n; i++) {
    if (s[i] == '%' && i + 2 < n) {
      char hex[3] = {s[i + 1], s[i + 2], '\0'};
      out[j++] = (char)strtol(hex, nullptr, 16);
      i += 2;
    } else if (s[i] == '+') {
      out[j++] = ' ';
    } else {
      out[j++] = s[i];
    }
  }
  out[j] = '\0';
  return out;
}

static const char *find_query_param(const char *query, const char *key,
                                    size_t *val_len) {
  size_t klen = strlen(key);
  const char *p = query;
  while (*p != '\0') {
    const char *amp = strchr(p, '&');
    size_t plen = (amp == nullptr) ? strlen(p) : (size_t)(amp - p);
    if (plen > klen && p[klen] == '=' && strncmp(p, key, klen) == 0) {
      *val_len = plen - klen - 1;
      return p + klen + 1;
    }
    if (amp == nullptr) break;
    p = amp + 1;
  }
  return nullptr;
}

static char *extract_param(const char *query, const char *key) {
  size_t vlen = 0;
  const char *v = find_query_param(query, key, &vlen);
  if (v == nullptr) return nullptr;
  return url_decode(v, vlen);
}

static const char *html_ok =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Connection: close\r\n"
    "\r\n"
    "<!doctype html><html><head><title>cli-toot</title></head>"
    "<body><h2>Authorized. You can close this tab.</h2></body></html>\r\n";

static const char *html_err =
    "HTTP/1.1 400 Bad Request\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Connection: close\r\n"
    "\r\n"
    "<!doctype html><html><head><title>cli-toot</title></head>"
    "<body><h2>Authorization failed (state mismatch).</h2></body></html>\r\n";

bool loopback_listen(uint16_t *port_out, int *fd_out) {
  int s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) return false;

  int yes = 1;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;

  if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    close(s);
    return false;
  }

  socklen_t alen = sizeof(addr);
  if (getsockname(s, (struct sockaddr *)&addr, &alen) != 0) {
    close(s);
    return false;
  }
  *port_out = ntohs(addr.sin_port);

  if (listen(s, 1) != 0) {
    close(s);
    return false;
  }

  *fd_out = s;
  return true;
}

bool loopback_accept_once(int fd, const char *state, int timeout_seconds,
                          loopback_result *out) {
  out->code = nullptr;
  out->error = nullptr;
  out->ok = false;

  if (timeout_seconds > 0) {
    struct timeval tv = {.tv_sec = timeout_seconds, .tv_usec = 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
  }

  int c = accept(fd, nullptr, nullptr);
  if (c < 0) return false;

  char buf[4096] = {0};
  ssize_t n = read(c, buf, sizeof(buf) - 1);
  if (n <= 0) {
    close(c);
    return false;
  }

  if (getenv("CLI_TOOT_DEBUG") != nullptr) {
    fprintf(stderr, "[debug] loopback raw request:\n%.*s\n", (int)n, buf);
  }

  char *sp = strchr(buf, ' ');
  if (sp == nullptr) {
    write(c, html_err, strlen(html_err));
    close(c);
    return false;
  }
  char *path = sp + 1;
  char *end = strchr(path, ' ');
  if (end == nullptr) end = path + strlen(path);
  *end = '\0';

  char *qmark = strchr(path, '?');
  if (qmark == nullptr) {
    write(c, html_err, strlen(html_err));
    close(c);
    return false;
  }
  char *query = qmark + 1;

  char *got_state = extract_param(query, "state");
  char *code = extract_param(query, "code");
  char *err = extract_param(query, "error");

  bool state_ok = (got_state != nullptr && strcmp(got_state, state) == 0);
  free(got_state);

  if (!state_ok || (code == nullptr && err == nullptr)) {
    write(c, html_err, strlen(html_err));
    close(c);
    free(code);
    free(err);
    return false;
  }

  write(c, html_ok, strlen(html_ok));
  close(c);

  out->code = code;
  out->error = err;
  out->ok = true;
  return true;
}

char *loopback_redirect_uri(uint16_t port) {
  char *uri = malloc(64);
  if (uri == nullptr) return nullptr;
  snprintf(uri, 64, "http://127.0.0.1:%u/callback", port);
  return uri;
}

void loopback_result_free(loopback_result *r) {
  free(r->code);
  free(r->error);
  r->code = nullptr;
  r->error = nullptr;
  r->ok = false;
}
