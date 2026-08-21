#pragma once

#include <stdbool.h>

/* Show a detailed view of a single status by id or URL. When `mobile` is set
 * the local cache is consulted first and no network request is made for
 * already-cached posts; uncached posts are fetched and stored in the cache.
 * Returns an exit code. */
int view_status(const char *ref, bool mobile);
