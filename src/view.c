#include "view.h"

#include "cache.h"
#include "config.h"
#include "http.h"
#include "json_helpers.h"
#include "toot.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int view_status(const char *ref, bool mobile) {
  char *id = normalize_status_id(ref);
  if (id == nullptr) {
    fprintf(stderr, "error: invalid status reference: %s\n",
            ref != nullptr ? ref : "(null)");
    return 1;
  }

  /* A failing cache is not fatal: we simply skip the offline view and the
   * cache store. */
  cache db;
  bool have_db = cache_open(&db);
  if (!have_db) db = (cache){0};

  /* Mobile: already cached? Serve from the cache, no network. */
  if (mobile && have_db && cache_view(&db, id)) {
    cache_close(&db);
    free(id);
    return 0;
  }

  config c;
  config_init(&c);
  if (!config_load(&c) || c.access_token == nullptr || c.instance == nullptr) {
    fprintf(stderr, "error: not logged in. Run `sloptoot login <instance>`.\n");
    config_free(&c);
    cache_close(&db);
    free(id);
    return 2;
  }

  size_t n = strlen(c.instance) + strlen("/api/v1/statuses/") + strlen(id) + 1;
  char *url = malloc(n);
  if (url == nullptr) {
    config_free(&c);
    cache_close(&db);
    free(id);
    return 3;
  }
  snprintf(url, n, "%s/api/v1/statuses/%s", c.instance, id);

  http_response resp = {0};
  bool ok = http_get(url, c.access_token, &resp);
  free(url);
  if (!ok) {
    fprintf(stderr, "error: network request failed\n");
    http_response_free(&resp);
    config_free(&c);
    cache_close(&db);
    free(id);
    return 3;
  }
  if (resp.code < 200 || resp.code >= 300) {
    fprintf(stderr, "error: HTTP %ld\n", resp.code);
    if (resp.body != nullptr) fprintf(stderr, "%s\n", resp.body);
    http_response_free(&resp);
    config_free(&c);
    cache_close(&db);
    free(id);
    return 3;
  }

  cJSON *root = json_parse(resp.body, resp.len);
  http_response_free(&resp);
  if (root == nullptr) {
    fprintf(stderr, "error: could not parse response\n");
    config_free(&c);
    cache_close(&db);
    free(id);
    return 3;
  }

  /* Cache it (a boost wrapper stores its inner post too) so that later
   * `view --mobile` can serve it without network. */
  if (have_db) cache_store_status(&db, root);

  /* A boost wrapper's real content is on the inner `reblog` status. */
  const cJSON *inner = cJSON_GetObjectItemCaseSensitive(root, "reblog");
  bool is_boost = inner != nullptr && cJSON_IsObject(inner);
  const cJSON *detail = is_boost ? inner : root;

  /* Author (of the underlying post). */
  const cJSON *account = cJSON_GetObjectItemCaseSensitive(detail, "account");
  const char *display = "";
  const char *handle = "";
  if (account != nullptr && cJSON_IsObject(account)) {
    display = json_get_string(account, "display_name");
    handle = json_get_string(account, "acct");
  }
  if (display == nullptr || display[0] == '\0') display = handle;

  /* Body text. */
  char text[8192];
  cache_plain_text(json_get_string(detail, "content"), text, sizeof(text));

  /* Timestamps. */
  const char *created = json_get_string(detail, "created_at");
  char rel[16] = "";
  if (created != nullptr) cache_relative_time(created, rel, sizeof(rel));

  /* When viewing a boost, first show the boost line (booster, when, id). */
  if (is_boost) {
    const cJSON *bacc = cJSON_GetObjectItemCaseSensitive(root, "account");
    const char *bhandle = "";
    if (bacc != nullptr && cJSON_IsObject(bacc))
      bhandle = json_get_string(bacc, "acct");
    const char *bcreated = json_get_string(root, "created_at");
    char brel[16] = "";
    if (bcreated != nullptr) cache_relative_time(bcreated, brel, sizeof(brel));
    printf("\xF0\x9F\x9A\x80 @%s boosted\n", bhandle != nullptr ? bhandle : "");
    if (bcreated != nullptr) printf("Boosted: %s (%s)\n", bcreated, brel);
    printf("\n");
  }

  printf("%s @%s\n", display, handle != nullptr ? handle : "");
  printf("%s\n", text);
  printf("\n");
  if (created != nullptr) printf("Posted: %s (%s)\n", created, rel);
  printf("ID:     %s\n", json_get_string(detail, "id"));
  const char *post_url = json_get_string(detail, "url");
  if (post_url != nullptr) printf("URL:    %s\n", post_url);

  const char *visibility = json_get_string(detail, "visibility");
  printf("Visibility: %s\n", visibility != nullptr ? visibility : "?");

  /* Posting client (client name the instance registered for the app). */
  const cJSON *app = cJSON_GetObjectItemCaseSensitive(detail, "application");
  const char *client = app != nullptr ? json_get_string(app, "name") : nullptr;
  printf("Client:   %s\n", client != nullptr && client[0] != '\0' ? client : "-");

  printf("Boosts: %d\n", json_get_int(detail, "reblogs_count"));
  printf("Likes:  %d\n", json_get_int(detail, "favourites_count"));
  printf("Replies:%d\n", json_get_int(detail, "replies_count"));

  /* Attachments: we cannot render media in the terminal, so show the count
   * and each attachment's alt text (description) when one is set. */
  const cJSON *media =
      cJSON_GetObjectItemCaseSensitive(detail, "media_attachments");
  int nmedia = (media != nullptr && cJSON_IsArray(media)) ? cJSON_GetArraySize(media) : 0;
  if (nmedia > 0) {
    printf("Attachments: %d\n", nmedia);
    for (int i = 0; i < nmedia; i++) {
      const cJSON *att = cJSON_GetArrayItem(media, i);
      const char *desc = att != nullptr ? json_get_string(att, "description") : nullptr;
      if (desc != nullptr && desc[0] != '\0') {
        char alt[1024];
        cache_plain_text(desc, alt, sizeof(alt));
        printf("  [%d] %s\n", i + 1, alt);
      }
    }
  }

  json_free(root);
  config_free(&c);
  cache_close(&db);
  free(id);
  return 0;
}