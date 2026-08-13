#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "http.h"
#include "json_helpers.h"
#include "oauth.h"
#include "toot.h"

static void print_usage(const char *prog) {
  fprintf(stderr,
          "usage: %s <command> [args]\n"
          "\n"
          "commands:\n"
          "  login <instance>   authenticate with a Mastodon instance\n"
          "  toot \"<text>\"      post a new status\n"
          "  whoami             show the logged-in account\n"
          "  help               show this help\n",
          prog);
}

static int cmd_whoami(void) {
  config c;
  config_init(&c);
  if (!config_load(&c) || c.access_token == nullptr || c.instance == nullptr ||
      c.username == nullptr) {
    fprintf(stderr, "error: not logged in. Run `cli-toot login <instance>`.\n");
    config_free(&c);
    return 2;
  }
  printf("@%s@%s\n", c.username,
         c.instance + (strncmp(c.instance, "https://", 8) == 0 ? 8
                       : strncmp(c.instance, "http://", 7) == 0 ? 7
                                                                : 0));
  config_free(&c);
  return 0;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    print_usage(argv[0]);
    return 1;
  }

  http_global_init();
  int rc = 0;

  if (strcmp(argv[1], "login") == 0) {
    if (argc < 3) {
      fprintf(stderr, "usage: %s login <instance>\n", argv[0]);
      rc = 1;
    } else {
      rc = login(argv[2]);
    }
  } else if (strcmp(argv[1], "toot") == 0) {
    if (argc < 3) {
      fprintf(stderr, "usage: %s toot \"<text>\"\n", argv[0]);
      rc = 1;
    } else {
      rc = post_status(argv[2]);
    }
  } else if (strcmp(argv[1], "whoami") == 0) {
    rc = cmd_whoami();
  } else if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0 ||
             strcmp(argv[1], "-h") == 0) {
    print_usage(argv[0]);
    rc = 0;
  } else {
    fprintf(stderr, "unknown command: %s\n", argv[1]);
    print_usage(argv[0]);
    rc = 1;
  }

  http_global_cleanup();
  return rc;
}
