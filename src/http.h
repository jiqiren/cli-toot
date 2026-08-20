#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct {
  char *body;
  size_t len;
  long code;
} http_response;

typedef struct {
  const char *key;
  const char *value;
} http_field;

void http_global_init(void);
void http_global_cleanup(void);

[[nodiscard]] bool http_post_form(const char *url, const http_field *fields,
                                  size_t nfields, const char *bearer,
                                  http_response *resp);

[[nodiscard]] bool http_post_json(const char *url, const char *body,
                                  const char *bearer, http_response *resp);

[[nodiscard]] bool http_get(const char *url, const char *bearer,
                            http_response *resp);

[[nodiscard]] bool http_delete(const char *url, const char *bearer,
                               http_response *resp);

/* POST with no request body (used for status actions like reblog/favourite). */
[[nodiscard]] bool http_post_empty(const char *url, const char *bearer,
                                   http_response *resp);

void http_response_free(http_response *resp);

[[nodiscard]] char *urlencode(const char *s);
