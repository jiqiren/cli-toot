#include "browser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__)
#define OPEN_CMD "open"
#else
#define OPEN_CMD "xdg-open"
#endif

bool open_browser(const char *url) {
  size_t n = strlen(url) + strlen(OPEN_CMD " \"\"") + 1;
  char *cmd = malloc(n);
  if (cmd == nullptr) return false;
  snprintf(cmd, n, OPEN_CMD " \"%s\"", url);

  int rc = system(cmd);
  free(cmd);
  return rc == 0;
}
