#pragma once

#include "cache.h"

/* Post a new status. `in_reply_to_id` may be null for a fresh post. On
 * success the new status id is recorded as the last-post chain anchor in the
 * cache. Returns an exit code. */
int post_status(const char *text, const char *in_reply_to_id, cache *db);

/* Delete a status by id or URL. The status is removed from the server and from
 * the cache (if present). Returns an exit code. */
int delete_status(const char *ref, cache *db);

/* Normalize a --reply reference into a numeric Mastodon status id. Accepts a
 * bare id or a status URL (the trailing id is extracted). Returns a fresh
 * allocation or nullptr on error. */
[[nodiscard]] char *normalize_status_id(const char *ref);

/* Resolve `ref` to the id a status action/reply should target. If the status
 * is a boost wrapper, this returns the inner (really-authored) post's id.
 * Requires a loaded config (loaded internally). Returns a fresh allocation,
 * nullptr on error. */
[[nodiscard]] char *resolve_status_id(const char *ref);

/* Perform `action` ("reblog", "favourite", or "bookmark") on a status by id or
 * URL. POSTs to /api/v1/statuses/{id}/{action} and prints a short confirmation.
 * Returns an exit code. */
int status_action(const char *ref, const char *action);