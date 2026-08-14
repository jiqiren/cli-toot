#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "http.h"
#include "json_helpers.h"
#include "oauth.h"
#include "toot.h"
#include "version.h"

static void print_usage(const char *prog) {
  fprintf(stderr,
          "usage: %s <command> [args]\n"
          "\n"
          "commands:\n"
           "  login <instance> [--oob]\n"
           "                     authenticate with a Mastodon instance\n"
           "                     --oob skips the loopback server and uses\n"
           "                     the out-of-band code paste flow\n"
          "  toot \"<text>\"      post a new status\n"
          "  whoami             show the logged-in account\n"
          "  help               show this help\n"
          "  version            show version\n",
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
      fprintf(stderr, "usage: %s login <instance> [--oob]\n", argv[0]);
      rc = 1;
    } else {
      bool force_oob = false;
      const char *instance_arg = nullptr;
      for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--oob") == 0) {
          force_oob = true;
        } else if (instance_arg == nullptr) {
          instance_arg = argv[i];
        } else {
          fprintf(stderr, "error: unexpected argument: %s\n", argv[i]);
          rc = 1;
          break;
        }
      }
      if (rc == 0 && instance_arg == nullptr) {
        fprintf(stderr, "usage: %s login <instance> [--oob]\n", argv[0]);
        rc = 1;
      } else if (rc == 0) {
        rc = login(instance_arg, force_oob);
      }
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
  } else if (strcmp(argv[1], "version") == 0 ||
             strcmp(argv[1], "--version") == 0 ||
             strcmp(argv[1], "-V") == 0) {
    printf("cli-toot %s\n", CLI_TOOT_VERSION);
    rc = 0;
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
