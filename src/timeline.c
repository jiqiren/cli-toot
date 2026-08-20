#include "timeline.h"

#include "cache.h"
#include "http.h"
#include "json_helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PAGE_LIMIT 40

/* Fetch one page of a timeline. `url` must be the endpoint base without any
 * query string. When `max_id` is non-null the older-than page is requested.
 * Returns a parsed cJSON array (caller frees) or nullptr on error. */
static cJSON *fetch_page(const config *c, const char *base, const char *max_id) {
  /* Build endpoint + query. */
  size_t n = strlen(base) + 256;
  char *url = malloc(n);
  if (url == nullptr) return nullptr;
  if (max_id != nullptr) {
    snprintf(url, n, "%s?limit=%d&max_id=%s", base, PAGE_LIMIT, max_id);
  } else {
    snprintf(url, n, "%s?limit=%d", base, PAGE_LIMIT);
  }

  http_response resp = {0};
  bool ok = http_get(url, c->access_token, &resp);
  free(url);
  if (!ok) {
    fprintf(stderr, "error: network request failed\n");
    http_response_free(&resp);
    return nullptr;
  }
  if (resp.code < 200 || resp.code >= 300) {
    fprintf(stderr, "error: HTTP %ld\n", resp.code);
    if (resp.body != nullptr) fprintf(stderr, "%s\n", resp.body);
    http_response_free(&resp);
    return nullptr;
  }

  cJSON *root = json_parse(resp.body, resp.len);
  http_response_free(&resp);
  return root;
}

/* Concatenate the items of `page` onto `all`. Returns the id of the last item
 * in the page (a fresh allocation), or nullptr. */
static char *append_page(cJSON *all, const cJSON *page, const char **last_id) {
  if (page == nullptr || !cJSON_IsArray(page)) return nullptr;
  *last_id = nullptr;
  for (const cJSON *it = page->child; it != nullptr; it = it->next) {
    cJSON *copy = cJSON_Duplicate(it, true);
    if (copy != nullptr) cJSON_AddItemToArray(all, copy);
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(it, "id");
    if (id != nullptr && cJSON_IsString(id)) *last_id = id->valuestring;
  }
  return (char *)*last_id;
}

/* Number of terminal lines one status occupies when printed: 1 for a normal
 * post, 2 for a boost wrapper (its inner post is the indented second line). */
static int status_lines(const cJSON *it) {
  const cJSON *reblog = cJSON_GetObjectItemCaseSensitive(it, "reblog");
  return (reblog != nullptr && cJSON_IsObject(reblog)) ? 2 : 1;
}

/* Total display lines of a cJSON array of statuses. */
static size_t array_lines(const cJSON *arr) {
  size_t n = 0;
  for (const cJSON *it = arr->child; it != nullptr; it = it->next)
    n += (size_t)status_lines(it);
  return n;
}

int ls_statuses(const config *c, cache *db, const char *type, bool mobile,
                bool wrap) {
  if (c == nullptr || c->instance == nullptr || c->access_token == nullptr) {
    fprintf(stderr,
            "error: not logged in. Run `cli-toot login <instance>`.\n");
    return 2;
  }

  bool is_tag = type != nullptr && type[0] == '#';
  bool is_home = type != nullptr && strcmp(type, "home") == 0;
  const char *cache_type = is_tag ? type + 1
                           : (is_home ? "home"
                                      : "profile");

  /* Mobile: serve from the cache only, no network. */
  if (mobile) {
    cache_list(db, cache_type, wrap);
    return 0;
  }

  /* Build the endpoint base URL. */
  size_t n = strlen(c->instance) + 256;
  char *base = malloc(n);
  if (base == nullptr) return 3;
  if (is_home) {
    snprintf(base, n, "%s/api/v1/timelines/home", c->instance);
  } else if (is_tag) {
    char *enc = urlencode(type + 1);
    if (enc == nullptr) {
      free(base);
      return 3;
    }
    snprintf(base, n, "%s/api/v1/timelines/tag/%s", c->instance, enc);
    free(enc);
  } else {
    if (c->account_id == nullptr) {
      fprintf(stderr, "error: account id missing from config\n");
      free(base);
      return 2;
    }
    snprintf(base, n, "%s/api/v1/accounts/%s/statuses", c->instance,
             c->account_id);
  }

  /* How many display lines to aim for: the whole screen minus two rows (so the
   * shell prompt after `ls` isn't pushed off / doesn't lose a line). Boosts
   * render as two lines, so the budget is counted in lines, not statuses. */
  int target_rows = cache_terminal_rows();
  size_t target_lines = (size_t)((target_rows - 2) > 0 ? (target_rows - 2) : 1);

  cJSON *all = cJSON_CreateArray();
  if (all == nullptr) {
    free(base);
    return 3;
  }

  char *max_id = nullptr;
  int guard = 0;
  while (array_lines(all) < target_lines) {
    cJSON *page = fetch_page(c, base, max_id);
    if (page == nullptr) {
      json_free(all);
      free(max_id);
      free(base);
      return 3;
    }
    if (!cJSON_IsArray(page) || cJSON_GetArraySize(page) == 0) {
      json_free(page);
      break;
    }
    const char *last_id = nullptr;
    append_page(all, page, &last_id);
    /* Copy the id now; `page` is freed below and last_id points into it. */
    char *next_id = last_id != nullptr ? strdup(last_id) : nullptr;
    json_free(page);
    if (next_id == nullptr) {
      /* No id to page on (or OOM); can't fetch more. */
      break;
    }
    free(max_id);
    max_id = next_id;
    if (++guard > 100) break; /* sanity: never loop forever */
  }

  free(max_id);
  free(base);

  if (cJSON_GetArraySize(all) == 0) {
    json_free(all);
    fprintf(stderr, "no posts found\n");
    return 0;
  }

  /* We fetched at least a screenful; drop the oldest posts (whole items, each
   * one or two lines) until the display fits the terminal. */
  while (array_lines(all) > target_lines) {
    cJSON *drop = cJSON_DetachItemFromArray(all, cJSON_GetArraySize(all) - 1);
    if (drop != nullptr) cJSON_Delete(drop);
  }

  cache_store_statuses(db, all, cache_type);
  cache_set_last_type(db, cache_type);
  cache_list_items(db, all, wrap);
  json_free(all);
  return 0;
}