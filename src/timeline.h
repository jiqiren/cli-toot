#pragma once

#include "cache.h"
#include "config.h"

#include <stdbool.h>

/* Fetch and print a timeline. `type` is "profile", "home", or a hashtag name
 * (leading '#' optional). When `mobile` is set, serve from the cache instead
 * of hitting the network. When `wrap` is set, print full text instead of
 * truncating to terminal width. Returns an exit code. */
int ls_statuses(const config *c, cache *db, const char *type, bool mobile,
                bool wrap);