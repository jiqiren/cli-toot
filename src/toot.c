#include "toot.h"

#include "config.h"
#include "http.h"
#include "json_helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int post_status(const char *text) {
  config c;
  config_init(&c);
  if (!config_load(&c) || c.access_token == nullptr || c.instance == nullptr) {
    fprintf(stderr, "error: not logged in. Run `cli-toot login <instance>`.\n");
    config_free(&c);
    return 2;
  }

  size_t n = strlen(c.instance) + strlen("/api/v1/statuses") + 1;
  char *url = malloc(n);
  if (url == nullptr) {
    config_free(&c);
    return 3;
  }
  snprintf(url, n, "%s/api/v1/statuses", c.instance);

  http_field fields[] = {
      {"status", text},
      {"visibility", "public"},
  };

  http_response resp = {0};
  bool ok = http_post_form(url, fields, 2, c.access_token, &resp);
  free(url);
  config_free(&c);

  if (!ok) {
    fprintf(stderr, "error: network request failed\n");
    http_response_free(&resp);
    return 3;
  }
  if (resp.code < 200 || resp.code >= 300) {
    fprintf(stderr, "error: HTTP %ld\n", resp.code);
    if (resp.body != nullptr) fprintf(stderr, "%s\n", resp.body);
    http_response_free(&resp);
    return 3;
  }

  cJSON *root = json_parse(resp.body, resp.len);
  http_response_free(&resp);
  if (root == nullptr) {
    fprintf(stderr, "error: could not parse response\n");
    return 3;
  }

  const char *post_url = json_get_string(root, "url");
  if (post_url != nullptr) {
    printf("%s\n", post_url);
    json_free(root);
    return 0;
  }

  fprintf(stderr, "posted (no URL returned)\n");
  json_free(root);
  return 0;
}
