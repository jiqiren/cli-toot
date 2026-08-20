#include "toot.h"

#include "config.h"
#include "http.h"
#include "json_helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *normalize_status_id(const char *ref) {
  if (ref == nullptr || ref[0] == '\0') return nullptr;
  /* A URL has a path; take the trailing segment as the id. */
  const char *slash = strrchr(ref, '/');
  const char *start = slash != nullptr ? slash + 1 : ref;
  if (*start == '\0') return nullptr;
  /* Ids are alphanumeric (Mastodon snowflakes and GoToSocial ULIDs), plus
   * separators; anything else is not a valid status reference. */
  for (const char *p = start; *p != '\0'; p++) {
    char ch = *p;
    if (!((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') ||
          (ch >= 'a' && ch <= 'z') || ch == '-' || ch == '_'))
      return nullptr;
    if (ch == '/') return nullptr;
  }
  return strdup(start);
}

int post_status(const char *text, const char *in_reply_to_id, cache *db) {
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

  http_field all[3] = {
      {"status", text},
      {"visibility", "public"},
      {"in_reply_to_id", in_reply_to_id != nullptr ? in_reply_to_id : ""},
  };
  size_t nfields = in_reply_to_id != nullptr ? 3 : 2;

  http_response resp = {0};
  bool ok = http_post_form(url, all, nfields, c.access_token, &resp);
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
  if (post_url != nullptr) printf("%s\n", post_url);

  const char *new_id = json_get_string(root, "id");
  if (new_id != nullptr && db != nullptr && db->db != nullptr) {
    cache_set_last_post_id(db, new_id);
  }
  json_free(root);

  if (post_url != nullptr) return 0;
  fprintf(stderr, "posted (no URL returned)\n");
  return 0;
}

int delete_status(const char *ref, cache *db) {
  char *id = normalize_status_id(ref);
  if (id == nullptr) {
    fprintf(stderr, "error: invalid status reference: %s\n",
            ref != nullptr ? ref : "(null)");
    return 1;
  }

  config c;
  config_init(&c);
  if (!config_load(&c) || c.access_token == nullptr || c.instance == nullptr) {
    fprintf(stderr, "error: not logged in. Run `cli-toot login <instance>`.\n");
    config_free(&c);
    free(id);
    return 2;
  }

  size_t n = strlen(c.instance) + strlen("/api/v1/statuses/") + strlen(id) + 1;
  char *url = malloc(n);
  if (url == nullptr) {
    config_free(&c);
    free(id);
    return 3;
  }
  snprintf(url, n, "%s/api/v1/statuses/%s", c.instance, id);

  http_response resp = {0};
  bool ok = http_delete(url, c.access_token, &resp);
  free(url);
  config_free(&c);

  if (!ok) {
    fprintf(stderr, "error: network request failed\n");
    http_response_free(&resp);
    free(id);
    return 3;
  }
  if (resp.code < 200 || resp.code >= 300) {
    fprintf(stderr, "error: HTTP %ld\n", resp.code);
    if (resp.body != nullptr) fprintf(stderr, "%s\n", resp.body);
    http_response_free(&resp);
    free(id);
    return 3;
  }
  http_response_free(&resp);

  if (db != nullptr && db->db != nullptr) cache_delete_status(db, id);
  printf("deleted %s\n", id);
  free(id);
  return 0;
}

char *resolve_status_id(const char *ref) {
  char *id = normalize_status_id(ref);
  if (id == nullptr) return nullptr;

  config c;
  config_init(&c);
  if (!config_load(&c) || c.access_token == nullptr || c.instance == nullptr) {
    config_free(&c);
    return id; /* can't verify; fall back to the normalized id */
  }

  size_t n = strlen(c.instance) + strlen("/api/v1/statuses/") + strlen(id) + 1;
  char *url = malloc(n);
  if (url != nullptr) {
    snprintf(url, n, "%s/api/v1/statuses/%s", c.instance, id);
    http_response gresp = {0};
    if (http_get(url, c.access_token, &gresp) && gresp.code >= 200 &&
        gresp.code < 300) {
      cJSON *g = json_parse(gresp.body, gresp.len);
      if (g != nullptr) {
        const cJSON *inner = cJSON_GetObjectItemCaseSensitive(g, "reblog");
        if (inner != nullptr && cJSON_IsObject(inner)) {
          const cJSON *iid = cJSON_GetObjectItemCaseSensitive(inner, "id");
          if (iid != nullptr && cJSON_IsString(iid)) {
            free(id);
            id = strdup(iid->valuestring);
          }
        }
        json_free(g);
      }
    }
    http_response_free(&gresp);
    free(url);
  }
  config_free(&c);
  return id;
}

int status_action(const char *ref, const char *action) {
  char *id = resolve_status_id(ref);
  if (id == nullptr) {
    fprintf(stderr, "error: invalid status reference: %s\n",
            ref != nullptr ? ref : "(null)");
    return 1;
  }

  config c;
  config_init(&c);
  if (!config_load(&c) || c.access_token == nullptr || c.instance == nullptr) {
    fprintf(stderr, "error: not logged in. Run `cli-toot login <instance>`.\n");
    config_free(&c);
    free(id);
    return 2;
  }

  size_t n = strlen(c.instance) + strlen("/api/v1/statuses//") + strlen(action) +
             strlen(id) + 1;
  char *url = malloc(n);
  if (url == nullptr) {
    config_free(&c);
    free(id);
    return 3;
  }
  snprintf(url, n, "%s/api/v1/statuses/%s/%s", c.instance, id, action);

  http_response resp = {0};
  bool ok = http_post_empty(url, c.access_token, &resp);
  free(url);
  config_free(&c);
  free(id);

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
  http_response_free(&resp);
  printf("%s\n", action);
  return 0;
}